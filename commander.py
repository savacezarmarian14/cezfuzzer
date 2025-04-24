import yaml
import pprint
import sys
import os
from colorama import init, Fore, Style
import subprocess

# Initialize colorama for cross-platform colored output
init(autoreset=True)

def log_info(msg):
    """Print an informational log message in blue."""
    print(f"{Fore.BLUE}[INFO]{Style.RESET_ALL} {msg}")

def log_success(msg):
    """Print a success log message in green."""
    print(f"{Fore.GREEN}[OK]{Style.RESET_ALL} {msg}")

def log_warning(msg):
    """Print a warning log message in yellow."""
    print(f"{Fore.YELLOW}[WARNING]{Style.RESET_ALL} {msg}")

def log_error(msg):
    """Print an error log message in red."""
    print(f"{Fore.RED}[ERROR]{Style.RESET_ALL} {msg}")

def load_config(path="config.yaml"):
    """
    Load and parse the YAML configuration file.

    Args:
        path (str): Path to the YAML configuration file (default: "config.yaml").

    Returns:
        dict: Parsed configuration as a dictionary.

    Exits:
        If the file does not exist or is invalid YAML.
    """
    if not os.path.isfile(path):
        log_error(f"Config file '{path}' not found.")
        sys.exit(1)

    try:
        with open(path, "r") as f:
            config = yaml.safe_load(f)
            log_success("Config file loaded successfully.")
            return config
    except yaml.YAMLError as e:
        log_error(f"Failed to parse YAML: {e}")
        sys.exit(1)

def create_docker_network(network_cfg):
    """
    Create a Docker network based on the configuration.
    If the network already exists, it will be removed and recreated.

    Args:
        network_cfg (dict): Dictionary containing 'docker_network_name', 'subnet', and 'gateway'.
    """
    name = network_cfg["docker_network_name"]
    subnet = network_cfg["subnet"]
    gateway = network_cfg["gateway"]

    # Check if the network already exists
    result = subprocess.run(
        ["docker", "network", "ls", "--filter", f"name=^{name}$", "--format", "{{.Name}}"],
        capture_output=True, text=True
    )

    if result.stdout.strip() == name:
        log_warning(f"Docker network '{name}' already exists. Removing it...")
        rm_result = subprocess.run(["docker", "network", "rm", name])
        if rm_result.returncode != 0:
            log_error(f"Failed to remove Docker network '{name}'")
            exit(1)
        log_success(f"Removed existing network '{name}'")

    # Create the network fresh
    log_info(f"Creating Docker network '{name}'...")
    create_cmd = [
        "docker", "network", "create",
        "--subnet", subnet,
        "--gateway", gateway,
        name
    ]
    create_result = subprocess.run(create_cmd)
    if create_result.returncode != 0:
        log_error(f"Failed to create Docker network '{name}'")
        exit(1)

    log_success(f"Docker network '{name}' created successfully.\n")

def load_template_content(template_path):
    """Reads and returns the content of the Dockerfile template."""
    try:
        with open(template_path, "r") as f:
            return f.read()
    except FileNotFoundError:
        log_error(f"Template file not found: {template_path}")
        return None

def generate_iptables_rule(entity_name, entity_cfg, all_entities):
    """Generates an iptables redirect rule for fuzzed entities, except for fuzzer roles."""
    if entity_cfg.get("role") == "fuzzer":
        return ""  # Fuzzers do not need iptables rules

    if not entity_cfg.get("fuzzed", False):
        return ""

    fuzzer = next((cfg for name, cfg in all_entities.items() if cfg["role"] == "fuzzer"), None)
    if not fuzzer:
        log_warning(f"No fuzzer found for fuzzed entity '{entity_name}'")
        return ""

    proto = entity_cfg.get("protocol", "tcp").lower()
    proto_flag = "-p tcp" if proto == "tcp" else "-p udp"
    return (
        f"RUN iptables -t nat -A OUTPUT -d {entity_cfg['ip']} {proto_flag} "
        f"--dport {entity_cfg['port']} -j DNAT --to-destination {fuzzer['ip']}:{entity_cfg['port']}"
    )

def generate_exec_command(entity_cfg):
    """Generates the final CMD instruction to start the app inside the container."""
    exec_with = entity_cfg.get("exec_with", "").strip()
    local_binary_path = entity_cfg.get("binary_path", "").strip()

    if not local_binary_path:
        log_error(f"'binary_path' missing or invalid for entity '{entity_cfg}'")
        return ""

    # Full path inside container based on 'COPY . /app'
    container_binary_path = "/app/" + local_binary_path.lstrip("./")
    args = entity_cfg.get("args", [])

    cmd_parts = []
    if exec_with:
        cmd_parts.append(exec_with)
    cmd_parts.append(container_binary_path)
    cmd_parts.extend(args)

    return 'CMD [{}]'.format(", ".join(repr(arg) for arg in cmd_parts))

def generate_dockerfile(entity_name, entity_cfg, all_entities, template_path="Dockerfile.template"):
    """
    Generates Dockerfile.<entity_name> in docker/ directory using the shared template.
    Inserts iptables rules and CMD instruction dynamically.
    """
    os.makedirs("docker", exist_ok=True)
    output_path = f"docker/Dockerfile.{entity_name}"

    template_content = load_template_content(template_path)
    if template_content is None:
        return

    iptables_rule = generate_iptables_rule(entity_name, entity_cfg, all_entities)
    exec_command = generate_exec_command(entity_cfg)

    dockerfile_content = template_content \
        .replace("# <IPTABLES_RULES>", iptables_rule) \
        .replace("# <EXEC_COMMAND>", exec_command)

    with open(output_path, "w") as f:
        f.write(dockerfile_content)

    log_success(f"Dockerfile generated for '{entity_name}' → {output_path}")


def main():
    """
    Main entry point for the commander.
    Loads configuration and (soon) executes orchestration logic.
    """
    log_info("Loading configuration...")
    config = load_config()

    log_info("Displaying configuration:")
    pprint.pprint(config, width=100)

    log_info("Step 1: Creating Docker network...")
    create_docker_network(config["network"])

    log_info("Step 2: Generating Dockerfiles from template...")
    for entity_name, entity_cfg in config["entities"].items():
        generate_dockerfile(entity_name, entity_cfg, config["entities"])
    

if __name__ == "__main__":
    main()
