import os
from .utils import log_info, log_success, log_warning, log_error

def load_template(template_path):
    if not os.path.isfile(template_path):
        log_error(f"Template not found: {template_path}")
    with open(template_path) as f:
        return f.read()

def generate_exec_command(entity_cfg):
    return 'CMD ["tail", "-f", "/dev/null"]'

def generate_udp_dnat_rules_for_client(ip, port, connections, fuzzer):
    lines = []

    for conn in connections:
        # Case EntityA
        if conn["entityA_ip"] == ip and (port == -1 or conn["entityA_port"] == port):
            recv_port = conn['entityA_proxy_port_recv']
            dport_cond = f"--dport {conn['entityB_port']}" if conn['entityB_port'] != -1 else ""
            lines.append(f"# DNAT for EntityA to {conn['entityB_ip']}")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {conn['entityB_ip']} {dport_cond} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            lines.append(
                f"iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['entityA_proxy_port_send']} -j ACCEPT"
            )

        # Case EntityB
        if conn["entityB_ip"] == ip and (port == -1 or conn["entityB_port"] == port or conn["entityB_port"] == -1):
            recv_port = conn['entityB_proxy_port_recv']
            dport_cond = f"--dport {conn['entityA_port']}" if conn['entityA_port'] != -1 else ""
            lines.append(f"# DNAT for EntityB to {conn['entityA_ip']}")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {conn['entityA_ip']} {dport_cond} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            lines.append(
                f"iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['entityB_proxy_port_send']} -j ACCEPT"
            )

    if not lines:
        lines.append("# No DNAT rules generated. No matching fuzzer connections found.")

    return "\n".join(lines)

def generate_tcp_dnat_rules_for_client(entity_cfg, fuzzer):
    lines = []

    # Ensure we only process TCP clients
    if entity_cfg.get("role") == "server":
        return ""  # No redirection for TCP servers

    client_ip = entity_cfg.get("ip")
    connect_info = entity_cfg.get("connect_to", {})
    target_ip = connect_info.get("ip")
    target_port = connect_info.get("port")

    fuzzer_ip = fuzzer.get("ip")
    tcp_redirections = fuzzer.get("tcp_redirections", [])

    if not all([target_ip, target_port, fuzzer_ip]):
        lines.append("# Missing client connect_to or fuzzer information. Skipping TCP DNAT.")
        return "\n".join(lines)

    # Look for the matching redirection rule
    match = next(
        (r for r in tcp_redirections
         if r.get("server_ip") == target_ip and r.get("server_port") == target_port),
        None
    )

    if not match:
        lines.append(f"# No matching TCP redirection for {target_ip}:{target_port}. Skipping.")
        return "\n".join(lines)

    proxy_port = match["proxy_port"]

    lines.append(f"# DNAT rule for TCP client {client_ip}: redirecting connect() to {target_ip}:{target_port} → fuzzer {fuzzer_ip}:{proxy_port}")
    lines.append(
        f"iptables -t nat -A OUTPUT -p tcp -d {target_ip} --dport {target_port} -j DNAT --to-destination {fuzzer_ip}:{proxy_port}"
    )

    return "\n".join(lines)

def generate_all_dockerfiles(entities: dict, template_path="Dockerfile.template"):
    for entity_name, entity_cfg in entities.items():
        generate_dockerfile(entity_name, entity_cfg, entities, template_path)

def generate_dockerfile(entity_name, entity_cfg, all_entities, template_path="Dockerfile.template"):
    os.makedirs("docker", exist_ok=True)
    tpl = load_template(template_path)

    entry_path = f"docker/entrypoint_{entity_name}.sh"
    with open(entry_path, "w") as ef:
        ef.write("#!/bin/sh\n")

        role = entity_cfg.get("role")
        ip = entity_cfg.get("ip")
        port = entity_cfg.get("port", -1)
        proto = entity_cfg.get("protocol", "udp").lower()
        is_fuzzed = entity_cfg.get("fuzzed", False)

        # For fuzzed UDP clients: insert DNAT rules
        if role != "fuzzer" and is_fuzzed and proto == "udp":
            fuzzer = next((cfg for cfg in all_entities.values() if cfg.get("role") == "fuzzer"), None)
            if fuzzer and "connections" in fuzzer:
                dnat_block = generate_udp_dnat_rules_for_client(ip, port, fuzzer["connections"], fuzzer)
                ef.write(dnat_block + "\n")
            else:
                ef.write("# No fuzzer connections found. Skipping UDP DNAT generation.\n")

        # For fuzzed TCP clients: insert DNAT rules (no connections needed)
        if role != "fuzzer" and is_fuzzed and proto == "tcp":
            fuzzer = next((cfg for cfg in all_entities.values() if cfg.get("role") == "fuzzer"), None)
            if fuzzer:
                dnat_block = generate_tcp_dnat_rules_for_client(entity_cfg, fuzzer)
                ef.write(dnat_block + "\n")
            else:
                ef.write("# No fuzzer found. Skipping TCP DNAT generation.\n")
        ef.write('exec "$@"\n')

    os.chmod(entry_path, 0o755)
    log_success(f"Entrypoint script generated → {entry_path}")

    entry_block = (
        f"COPY {entry_path} /entrypoint.sh\n"
        "RUN chmod +x /entrypoint.sh\n"
        "ENTRYPOINT [\"/entrypoint.sh\"]"
    )
    cmd_block = generate_exec_command(entity_cfg)
    content = tpl.replace("# <ENTRYPOINT>", entry_block)
    content = content.replace("# <EXEC_COMMAND>", cmd_block)

    out_path = os.path.join("docker", f"Dockerfile.{entity_name}")
    with open(out_path, "w") as df:
        df.write(content)

    log_success(f"Dockerfile generated → {out_path}")
