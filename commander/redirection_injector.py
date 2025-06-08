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

            connection = {
                "entityA_ip": src_ip,
                "entityA_port": src_port,
                "entityA_proxy_port_recv": random_port(),
                "entityA_proxy_port_send": random_port(),
                "entityB_ip": dst_ip,
                "entityB_port": dst_port,
                "entityB_proxy_port_recv": random_port(),
                "entityB_proxy_port_send": random_port()
            }

            connections.append(connection)

            log_info(
                f"{src_ip}:{src_port} <-> {dst_ip}:{dst_port} | "
                f"A[recv/send]: {connection['entityA_proxy_port_recv']}/{connection['entityA_proxy_port_send']} "
                f"B[recv/send]: {connection['entityB_proxy_port_recv']}/{connection['entityB_proxy_port_send']}"
            )

    fuzzer["connections"] = connections

    with open(config_path, "w") as f:
        yaml.safe_dump(config, f, sort_keys=False, default_flow_style=False)

    log_success(f"Injected {len(connections)} bidirectional connections into config.")


def inject_tcp_redirections(config_path):
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
    # Include already used ports in fuzzer (UDP connections, maybe pre-injected)
    if "connections" in fuzzer:
        for conn in fuzzer["connections"]:
            used_ports.update([
                conn["entityA_proxy_port_recv"],
                conn["entityA_proxy_port_send"],
                conn["entityB_proxy_port_recv"],
                conn["entityB_proxy_port_send"]
            ])

    def random_port():
        while True:
            p = random.randint(20000, 60000)
            if p not in used_ports:
                used_ports.add(p)
                return p

    tcp_redirections = []
    seen_servers = set()

    for name, entity in config["entities"].items():
        if entity.get("role") != "server":
            continue
        if entity.get("protocol", "udp").lower() != "tcp":
            continue

        server_ip = entity.get("ip")
        server_port = entity.get("port", -1)

        if not server_ip or server_port == -1:
            continue

        key = (server_ip, server_port)
        if key in seen_servers:
            continue
        seen_servers.add(key)

        proxy_port = random_port()

        tcp_redirections.append({
            "server_ip": server_ip,
            "server_port": server_port,
            "proxy_port": proxy_port
        })

        log_info(f"TCP redirect: {server_ip}:{server_port} → proxy_port {proxy_port}")

    fuzzer["tcp_redirections"] = tcp_redirections

    with open(config_path, "w") as f:
        yaml.safe_dump(config, f, sort_keys=False, default_flow_style=False)

    log_success(f"Injected {len(tcp_redirections)} TCP redirections into config.")
