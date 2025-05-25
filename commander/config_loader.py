import os
import sys
import json
import yaml
from .utils import log_success, log_error

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
