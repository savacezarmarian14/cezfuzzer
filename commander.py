#!/usr/bin/env python3
import os
import sys
import yaml
import pprint
import argparse
import subprocess
from colorama import init, Fore, Style
import json


# Initialize colorama for colored terminal output
init(autoreset=True)

# ------------------- Logging helpers -------------------
def log_info(msg):
    print(f"{Fore.BLUE}[INFO]{Style.RESET_ALL} {msg}")

def log_success(msg):
    print(f"{Fore.GREEN}[OK]{Style.RESET_ALL} {msg}")

def log_warning(msg):
    print(f"{Fore.YELLOW}[WARNING]{Style.RESET_ALL} {msg}")

def log_error(msg):
    print(f"{Fore.RED}[ERROR]{Style.RESET_ALL} {msg}")

# ------------------- Configuration loader -------------------
def load_config(path):
    if not os.path.isfile(path):
        log_error(f"Config file '{path}' not found.")
        sys.exit(1)
    try:
        with open(path) as f:
            cfg = yaml.safe_load(f)
            log_success(f"Loaded config: {path}")
            return cfg
    except yaml.YAMLError as e:
        log_error(f"Failed to parse YAML: {e}")
        sys.exit(1)

# ------------------- Docker network management -------------------
def create_docker_network(net_cfg):
    name = net_cfg.get("docker_network_name")
    subnet = net_cfg.get("subnet")
    gateway = net_cfg.get("gateway")

    # Remove existing network if present
    r = subprocess.run([
        "docker", "network", "ls", "--filter", f"name=^{name}$", "--format", "{{.Name}}"
    ], capture_output=True, text=True)
    if r.stdout.strip() == name:
        log_warning(f"Network '{name}' exists, removing...")
        if subprocess.run(["docker", "network", "rm", name]).returncode != 0:
            log_error(f"Failed to remove network {name}")
            sys.exit(1)
        log_success(f"Removed existing network: {name}")

    # Create the network
    log_info(f"Creating network '{name}' (subnet {subnet}, gateway {gateway})...")
    if subprocess.run([
        "docker", "network", "create", "--subnet", subnet, "--gateway", gateway, name
    ]).returncode != 0:
        log_error(f"Failed to create network {name}")
        sys.exit(1)
    log_success(f"Docker network '{name}' created.")

# ------------------- Template loading -------------------
def load_template(template_path):
    if not os.path.isfile(template_path):
        log_error(f"Template not found: {template_path}")
        sys.exit(1)
    with open(template_path) as f:
        return f.read()

# ------------------- Dockerfile & Entrypoint generation -------------------
def generate_iptables_rule(entity_name, entity_cfg, all_entities):
    # Only for fuzzed, non-fuzzer roles
    if not entity_cfg.get("fuzzed", False) or entity_cfg.get("role") == "fuzzer":
        return ""
    # Find first fuzzer entity
    f = next((c for _, c in all_entities.items() if c.get("role") == "fuzzer"), None)
    if not f:
        log_warning(f"No fuzzer defined for entity '{entity_name}'")
        return ""
    proto = entity_cfg.get("protocol", "tcp").lower()
    flag = "-p tcp" if proto == "tcp" else "-p udp"
    return (
        f"iptables -t nat -A OUTPUT -d {entity_cfg['ip']} {flag} "
        f"--dport {entity_cfg['port']} -j DNAT --to-destination {f['ip']}:{entity_cfg['port']}"
    )

def generate_exec_command(entity_cfg):
    exec_with = entity_cfg.get("exec_with", "").strip()
    local_path = entity_cfg.get("binary_path", "").strip()
    if not local_path:
        log_error(f"Missing 'binary_path' for entity: {entity_cfg}")
        return ""
    # Container will have /app root
    cont_path = "/app/" + local_path.lstrip("./")
    args = entity_cfg.get("args", [])
    parts = ([exec_with] if exec_with else []) + [cont_path] + args
    # Build CMD JSON array
    quoted = ", ".join(f'"{p}"' for p in parts)
    return f"CMD [{quoted}]"

def generate_dockerfile(entity_name, entity_cfg, all_entities, template_path="Dockerfile.template"):
    # Ensure output dir
    os.makedirs("docker", exist_ok=True)
    tpl = load_template(template_path)

    # 1) create entrypoint script
    rule = generate_iptables_rule(entity_name, entity_cfg, all_entities)
    entry_path = f"docker/entrypoint_{entity_name}.sh"
    with open(entry_path, "w") as ef:
        ef.write("#!/bin/sh\n")
        if rule:
            ef.write(rule + "\n")
        ef.write('exec "$@"\n')
    os.chmod(entry_path, 0o755)
    log_success(f"Entrypoint script -> {entry_path}")

    # 2) fill in Dockerfile.template
    entry_block = (
        f"COPY {entry_path} /entrypoint.sh\n"
        "RUN chmod +x /entrypoint.sh\n"
        "ENTRYPOINT [\"/entrypoint.sh\"]"
    )
    cmd_block = generate_exec_command(entity_cfg)
    content = tpl.replace("# <ENTRYPOINT>", entry_block).replace("# <EXEC_COMMAND>", cmd_block)
    out_path = os.path.join("docker", f"Dockerfile.{entity_name}")
    with open(out_path, "w") as df:
        df.write(content)
    log_success(f"Generated Dockerfile -> {out_path}")

# ------------------- Build & run containers -------------------
def build_image(entity_name):
    df = f"docker/Dockerfile.{entity_name}"
    tag = f"{entity_name}_image"
    log_info(f"Building image '{tag}'...")
    if subprocess.run(["docker", "build", "-f", df, "-t", tag, "."]).returncode != 0:
        log_error(f"Failed to build image {tag}")
        sys.exit(1)
    log_success(f"Built image {tag}")
    return tag

def run_container(entity_name, entity_cfg, image_tag, network_cfg, standby=False):
    cname = f"{entity_name}_container"

    # Remove existing container if present
    existing = subprocess.run([
        "docker", "ps", "-a", "-q", "-f", f"name=^{cname}$"
    ], capture_output=True, text=True).stdout.strip()
    if existing:
        log_warning(f"Container '{cname}' exists, removing...")
        if subprocess.run(["docker", "rm", "-f", cname]).returncode != 0:
            log_error(f"Failed to remove container {cname}")
            sys.exit(1)
        log_success(f"Removed container: {cname}")

    # Run new container
    log_info(f"Running container '{cname}'...")
    cmd = [
        "docker", "run", "-d", "--name", cname,
        "--network", network_cfg.get("docker_network_name"),
        "--ip", entity_cfg.get("ip"),
        image_tag
    ]
    if standby:
        cmd += ["tail", "-f", "/dev/null"]
    if subprocess.run(cmd).returncode != 0:
        log_error(f"Failed to run {cname}")
        sys.exit(1)
    mode = "in standby mode (waiting)" if standby else ""
    log_success(f"Container '{cname}' started {mode}.")

def launch_all_entities(config, standby=False):
    net_cfg = config.get("network", {})
    for name, ent in config.get("entities", {}).items():
        tag = build_image(name)
        run_container(name, ent, tag, net_cfg, standby)

def export_entities_minimal_json(config, output_path="entities_config.json"):
    result = {"udp": {}, "tcp": {}}

    for name, entity in config.get("entities", {}).items():
        proto = entity.get("protocol", "").lower()
        if proto not in ["tcp", "udp"]:
            continue

        entry = {
            "ip": entity.get("ip"),
            "port": entity.get("port"),
            "role": entity.get("role"),
            "destinations": []
        }

        if proto == "udp":
            entry["destinations"] = entity.get("destinations", [])
        elif proto == "tcp" and "connect_to" in entity:
            entry["destinations"] = [entity["connect_to"]]

        result[proto][name] = entry

    with open(output_path, "w") as f:
        json.dump(result, f, indent=2)

    log_success(f"Exported entity metadata → {output_path}")

# ------------------- Main -------------------
def main():
    parser = argparse.ArgumentParser(description="Commander for MITM fuzzer infrastructure")
    parser.add_argument("--config", default="config.yaml", help="Path to config YAML")
    parser.add_argument("--template", default="Dockerfile.template", help="Path to Dockerfile template")
    parser.add_argument("--standby", action="store_true", help="Start containers in standby mode (no auto-exec)")
    args = parser.parse_args()

    cfg = load_config(args.config)
    export_entities_minimal_json(cfg)
    log_info("Loaded configuration:")
    pprint.pprint(cfg, width=120)

    log_info("Creating Docker network...")
    create_docker_network(cfg.get("network", {}))

    log_info("Generating Dockerfiles and entrypoints...")
    for name, ent in cfg.get("entities", {}).items():
        generate_dockerfile(name, ent, cfg.get("entities", {}), args.template)

    log_info("Building images and launching containers...")
    launch_all_entities(cfg, args.standby)

if __name__ == "__main__":
    main()