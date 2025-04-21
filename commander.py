import yaml
import os
import subprocess


REQUIRED_FIELDS = {"name", "type", "ip", "protocol", "port"}

def setup_nat_rules(all_entities, network):
    """
    Insert DNAT into DOCKER-USER and SNAT into POSTROUTING
    for *every* fuzzed entity, redirecting to the fuzzer gateway.
    """
    print("[INFO] Configuring NAT rules in DOCKER-USER/POSTROUTING…")

    # 1. Găsește fuzzer‑ul
    fuzzer = next(e for e in all_entities.values() if e["type"] == "fuzzer")
    f_ip = fuzzer["ip"]

    # 2. Pentru fiecare entitate de tip 'fuzzed'
    for name, tgt in all_entities.items():
        if tgt["type"] != "fuzzed":
            continue

        t_ip = tgt["ip"]
        t_port = tgt["port"]
        t_proto = tgt["protocol"]
        print(f"[DNAT] {t_proto.upper()} {t_ip}:{t_port} → {f_ip}:{t_port}")
        subprocess.run([
            "sudo", "iptables", "-t", "nat", "-I", "PREROUTING",
            "-p",   t_proto,
            "-d",   f"{t_ip}/32",
            "--dport", str(t_port),
            "-j",   "DNAT", "--to-destination", f"{f_ip}:{t_port}"
        ], check=True)

        print(f"[SNAT] {t_proto.upper()} {t_ip}→{f_ip}:{t_port} rewrite src → {t_ip}")
        subprocess.run([
            "sudo", "iptables", "-t", "nat", "-I", "POSTROUTING",
            "-p",   t_proto,
            "-s",   f"{t_ip}/32",
            "-d",   f"{f_ip}/32",
            "--dport", str(t_port),
            "-j",   "SNAT", "--to-source", t_ip
        ], check=True)

    print("[✓] NAT rules set.")

def load_config(config_path="config.yaml"):
    """
    Load and validate the config.yaml structure.
    Returns network dict and a flat list of all entities with full metadata.
    """
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"[ERROR] Config file not found: {config_path}")

    with open(config_path, 'r') as file:
        try:
            config = yaml.safe_load(file)
        except yaml.YAMLError as e:
            raise ValueError(f"[ERROR] YAML parsing error: {e}")

    if 'network' not in config or 'entities' not in config:
        raise ValueError("[ERROR] Missing 'network' or 'entities' in config.")

    network = config['network']
    raw_entities = config['entities']
    all_entities = {}

    # Flatten entity groups into one list
    for group_name, group_entities in raw_entities.items():
        for entity in group_entities:
            # Basic validation
            if not REQUIRED_FIELDS.issubset(entity.keys()):
                raise ValueError(f"[ERROR] Missing fields in {entity.get('name', '?')}")

            name = entity['name']
            if name in all_entities:
                raise ValueError(f"[ERROR] Duplicate entity name: {name}")

            all_entities[name] = entity

    # Validate 'depends_on' and 'fuzzed_entities'
    for entity in all_entities.values():
        for key in ['depends_on', 'fuzzed_entities']:
            if key in entity:
                for dep in entity[key]:
                    if dep not in all_entities:
                        raise ValueError(f"[ERROR] '{key}' contains unknown entity: {dep}")

    return network, all_entities

def check_docker_installed():
    """
    Check if Docker is installed and accessible.
    """
    print("[CHECK] Verifying Docker is installed...")
    try:
        subprocess.run(["docker", "--version"], check=True, stdout=subprocess.DEVNULL)
        print("[OK] Docker is available.")
    except subprocess.CalledProcessError:
        raise RuntimeError("[ERROR] Docker is not installed or not available in PATH.")


def create_docker_network(network, network_name="fuzznet"):
    """
    Create a Docker bridge network using values from config if it doesn't exist.
    """
    subnet = network["subnet"]
    gateway = network["gateway"]

    print(f"[CHECK] Ensuring Docker network '{network_name}' exists...")
    result = subprocess.run(["docker", "network", "ls", "--filter", f"name=^{network_name}$", "--format", "{{.Name}}"],
                            capture_output=True, text=True)

    if network_name in result.stdout.split():
        print(f"[OK] Network '{network_name}' already exists.")
    else:
        print(f"[CREATE] Creating Docker network '{network_name}'...")
        try:
            subprocess.run([
                "docker", "network", "create",
                "--driver", "bridge",
                "--subnet", subnet,
                "--gateway", gateway,
                network_name
            ], check=True)
            print(f"[SUCCESS] Network '{network_name}' created.")
        except subprocess.CalledProcessError:
            raise RuntimeError(f"[ERROR] Failed to create Docker network '{network_name}'.")

def get_start_order(entities_dict):
    """
    Return a list of entity names in the order they should be started,
    based on their 'depends_on' relationships.

    Raises RuntimeError on circular dependency.
    """
    visited = set()
    temp_stack = set()
    start_order = []

    # Flatten all entity definitions
    flat_entities = {}
    for entity in entities_dict.values():
        flat_entities[entity["name"]] = entity

    def visit(name):
        if name in visited:
            return
        if name in temp_stack:
            raise RuntimeError(f"[ERROR] Circular dependency detected involving '{name}'")

        temp_stack.add(name)
        entity = flat_entities.get(name)
        if not entity:
            raise ValueError(f"[ERROR] Entity '{name}' not found in config.")

        for dep in entity.get("depends_on", []):
            visit(dep)

        temp_stack.remove(name)
        visited.add(name)
        start_order.append(name)

    # Run DFS on all entities
    for entity_name in flat_entities.keys():
        if entity_name not in visited:
            visit(entity_name)

    print(start_order)
    return start_order


def start_entity_container(entity, network_name="fuzznet"):
    """
    Start a Docker container for one entity based on config.
    """
    docker_cfg = entity.get("docker", {})
    name = entity["name"]
    ip = entity["ip"]
    image = docker_cfg.get("image", "fuzz_base")
    mount = docker_cfg.get("mount")  # ex: ./build/something:/app/binary
    cmd = docker_cfg.get("cmd", "/app/binary")
    workdir = docker_cfg.get("workdir", "/app")
    env_vars = docker_cfg.get("env", {})
    ports = docker_cfg.get("ports", [])
    args = docker_cfg.get("args", [])
    gateway = docker_cfg.get("gateway", None)

    if not mount:
        raise ValueError(f"[ERROR] Entity '{name}' has no 'mount' path defined in docker config.")

    local_path, container_path = mount.split(":")
    local_path = os.path.abspath(local_path)

    # Build docker run command
    docker_cmd = [
        "docker", "run", "-d",
        "--name", f"{name}_container",
        "--network", network_name,
        "--ip", ip,
        "-v", f"{local_path}:{container_path}",
        "-w", workdir
    ]

    if entity.get("type") == "fuzzer":
        docker_cmd.extend(["--cap-add=NET_ADMIN"])

    # Add environment variables
    for key, value in env_vars.items():
        docker_cmd.extend(["-e", f"{key}={value}"])

    # Add ports
    for port_map in ports:
        docker_cmd.extend(["-p", port_map])

    # Add image + CMD + optional args
    docker_cmd.append(image)

    # If fuzzer, prepend IP forwarding activation
    
    docker_cmd.append(cmd)
    docker_cmd.extend(args)

    print(f"[DOCKER] Starting container for '{name}'...")
    subprocess.run(docker_cmd, check=True)
    print(f"[SUCCESS] Container for '{name}' is running.")


if __name__ == "__main__":
    network, entities = load_config()
    print("[INFO] [✓] Network config loaded:", network)
    print("[INFO] [✓] Entities found:", list(entities.keys()))

    check_docker_installed()
    print("[INFO] [✓] Docker checked")

    create_docker_network(network)
    print("[INFO] [✓] Network created: ", network)

    setup_nat_rules(entities, network)


    start_order = get_start_order(entities)
    print ("[INFO] Start order: ", start_order)

    # Flatten entities
    all_entities = {}
    for entity in entities.values():
        print(entity)
        all_entities[entity["name"]] = entity

    for name in start_order:
        entity = all_entities[name]
        start_entity_container(entity)

