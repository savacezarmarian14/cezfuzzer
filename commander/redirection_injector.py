import yaml
import sys
import random
from .utils import log_info, log_success, log_error

def inject_fuzzer_redirections(config_path):
    with open(config_path, "r") as f:
        config = yaml.safe_load(f)

    fuzzer = next(
        (cfg for cfg in config["entities"].values() if cfg.get("role") == "fuzzer"),
        None
    )

    if not fuzzer:
        log_error("No fuzzer entity found in config.")
        sys.exit(1)

    used_ports = set()

    def random_port():
        while True:
            p = random.randint(20000, 60000)
            if p not in used_ports:
                used_ports.add(p)
                return p

    connections = []
    seen_pairs = set()

    for name, entity in config["entities"].items():
        if entity.get("role") == "fuzzer":
            continue

        src_ip = entity["ip"]
        src_port = entity["port"]

        for dest in entity.get("destinations", []):
            dst_ip = dest["ip"]
            dst_port = dest["port"]

            # define unique undirected pair
            key = tuple(sorted([(src_ip, src_port), (dst_ip, dst_port)]))
            if key in seen_pairs:
                continue
            seen_pairs.add(key)

            port_src_proxy = random_port()
            port_dst_proxy = random_port()

            connections.append({
                "src_ip": src_ip,
                "src_port": src_port,
                "port_src_proxy": port_src_proxy,
                "dst_ip": dst_ip,
                "dst_port": dst_port,
                "port_dst_proxy": port_dst_proxy
            })

            log_info(f"{src_ip}:{src_port} -> {dst_ip}:{dst_port} | proxy_ports: {port_src_proxy}/{port_dst_proxy}")

    fuzzer["connections"] = connections

    with open(config_path, "w") as f:
        yaml.safe_dump(config, f, sort_keys=False, default_flow_style=False)

    log_success(f"Injected {len(connections)} bidirectional connections into config.")

