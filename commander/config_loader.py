import os
import sys
import json
import yaml
from .utils import log_info, log_success, log_warning, log_error

def load_config(path):
    if not os.path.isfile(path):
        log_error(f"Config file '{path}' not found.")
    try:
        with open(path) as f:
            cfg = yaml.safe_load(f)
            log_success(f"Loaded config: {path}")
            return cfg
    except yaml.YAMLError as e:
        log_error(f"Failed to parse YAML: {e}")



def normalize_udp_ports(yaml_path):
    try:
        with open(yaml_path, 'r') as f:
            config = yaml.safe_load(f)
    except Exception as e:
        log_error(f"[normalize_udp_ports] Failed to load YAML: {e}")
        return

    entities = config.get('entities', {})

    # Map IP -> port (sau None)
    ip_to_port = {}

    for name, entity in entities.items():
        if entity.get("protocol") != "udp":
            continue

        ip = entity.get("ip")
        port = entity.get("port", None)

        if port is None:
            entity["port"] = -1
            ip_to_port[ip] = -1
            log_info(f"[normalize_udp_ports] Set port -1 for entity {name}")
        else:
            ip_to_port[ip] = port

    # Completăm porturile lipsă în destinations
    for name, entity in entities.items():
        if entity.get("protocol") != "udp":
            continue

        for dest in entity.get("destinations", []):
            dest_ip = dest.get("ip")
            if "port" not in dest and dest_ip in ip_to_port:
                dest["port"] = ip_to_port[dest_ip]
                log_info(f"[normalize_udp_ports] Set port {dest['port']} for destination {dest_ip} in entity {name}")

    try:
        with open(yaml_path, 'w') as f:
            yaml.dump(config, f, sort_keys=False)
        log_success(f"[normalize_udp_ports] Updated config file saved: {yaml_path}")
    except Exception as e:
        log_error(f"[normalize_udp_ports] Failed to save YAML: {e}")
