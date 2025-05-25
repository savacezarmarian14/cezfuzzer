import subprocess
import shlex
import time
import socket
import struct
from .utils import log_info, log_success, log_warning, log_error

def build_image(entity_name):
    df = f"docker/Dockerfile.{entity_name}"
    tag = f"{entity_name}_image"
    log_info(f"Building image '{tag}'...")
    if subprocess.run(["docker", "build", "-f", df, "-t", tag, "."]).returncode != 0:
        log_error(f"Failed to build image {tag}")
    log_success(f"Built image {tag}")
    return tag

def run_container(entity_name, entity_cfg, image_tag, network_cfg, standby=True):
    cname = f"{entity_name}_container"

    # Remove existing container if present
    existing = subprocess.run([
        "docker", "ps", "-a", "-q", "-f", f"name=^{cname}$"
    ], capture_output=True, text=True).stdout.strip()
    if existing:
        log_warning(f"Container '{cname}' exists, removing...")
        if subprocess.run(["docker", "rm", "-f", cname]).returncode != 0:
            log_error(f"Failed to remove container {cname}")
        log_success(f"Removed container: {cname}")

    # Run new container
    log_info(f"Running container '{cname}'...")
    cmd = [
        "docker", "run", "-d", "--name", cname,
        "--network", network_cfg.get("docker_network_name"),
        "--ip", entity_cfg.get("ip"),
        "--cap-add=NET_ADMIN",
        image_tag
    ]

    if subprocess.run(cmd).returncode != 0:
        log_error(f"Failed to run {cname}")
    mode = "in standby mode (waiting)" if standby else ""
    log_success(f"Container '{cname}' started {mode}.")

def launch_all_entities(config, standby=True):
    net_cfg = config.get("network", {})
    for name, ent in config.get("entities", {}).items():
        tag = build_image(name)
        run_container(name, ent, tag, net_cfg, standby)

def build_launcher_cmd(container, launcher, fuzzer_ip=None, extra_args=None):
    args = [launcher]
    if fuzzer_ip:
        args.append(fuzzer_ip)
    if extra_args:
        args.extend(arg for arg in extra_args if arg)

    arg_string = ' '.join(shlex.quote(arg) for arg in args)

    return [
        "docker", "exec", "-d", container,
        "sh", "-c", f"{arg_string} > /tmp/launcher.log 2>&1"
    ]

def run_launcher(config, config_path):
    # sort to get fuzzer first
    entities = dict(
        sorted(config.get("entities", {}).items(), key=lambda item: item[1].get("role") != "fuzzer")
    )

    fuzzer_ip = next(ent["ip"] for ent in entities.values() if ent.get("role") == "fuzzer")

    for name, ent in entities.items():
        container = f"{name}_container"
        if ent.get("role") == "fuzzer":
            launcher = "/app/build/luncher/server/server"
            cmd = build_launcher_cmd(container, launcher, None, [config_path])
        else:
            launcher = "/app/build/luncher/client/client"
            cmd = build_launcher_cmd(container, launcher, fuzzer_ip, [config_path])

        log_info(f"Starting {launcher} in container '{container}'...")
        print("[debug]", cmd)

        if subprocess.run(cmd).returncode != 0:
            log_error(f"Failed to start {launcher} in {container}")
        log_success(f"Launched {launcher} in {container}")

        if ent.get("role") == "fuzzer":
            time.sleep(0.2)

def connect_to_launcher(config, launcher_port):
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    entities = dict(
        sorted(config.get("entities", {}).items(), key=lambda item: item[1].get("role") != "fuzzer")
    )
    launcher_server_IP = next(ent["ip"] for ent in entities.values() if ent.get("role") == "fuzzer")

    client_socket.connect((launcher_server_IP, launcher_port))
    print(f"[INFO] Connected to {launcher_server_IP}:{launcher_port}")

    message = "[COMMANDER]".encode()
    length_bytes = struct.pack("L", len(message))

    client_socket.sendall(length_bytes)
    client_socket.sendall(message)
