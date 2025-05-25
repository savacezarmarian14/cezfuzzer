import subprocess
import sys
from .utils import log_info, log_success, log_warning, log_error

def create_docker_network(net_cfg):
    name = net_cfg.get("docker_network_name")
    subnet = net_cfg.get("subnet")
    gateway = net_cfg.get("gateway")

    # Verifică dacă există deja rețeaua
    r = subprocess.run([
        "docker", "network", "ls", "--filter", f"name=^{name}$", "--format", "{{.Name}}"
    ], capture_output=True, text=True)
    if r.stdout.strip() == name:
        log_warning(f"Network '{name}' exists, removing...")
        if subprocess.run(["docker", "network", "rm", name]).returncode != 0:
            log_error(f"Failed to remove network {name}")
        log_success(f"Removed existing network: {name}")

    # Creează rețeaua
    log_info(f"Creating network '{name}' (subnet {subnet}, gateway {gateway})...")
    if subprocess.run([
        "docker", "network", "create", "--subnet", subnet, "--gateway", gateway, name
    ]).returncode != 0:
        log_error(f"Failed to create network {name}")
    log_success(f"Docker network '{name}' created.")
