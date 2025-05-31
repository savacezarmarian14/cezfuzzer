import os
from .utils import log_info, log_success, log_warning, log_error

def load_template(template_path):
    if not os.path.isfile(template_path):
        log_error(f"Template not found: {template_path}")
    with open(template_path) as f:
        return f.read()

def generate_exec_command(entity_cfg):
    return 'CMD ["tail", "-f", "/dev/null"]'

def generate_tproxy_block_from_connections(connections, fuzzer_ip):
    lines = [
        "# === TPROXY UDP REDIRECT ===",
        "iptables -t mangle -N DIVERT_UDP || true",
        "iptables -t mangle -F DIVERT_UDP",
        "iptables -t mangle -A DIVERT_UDP -j MARK --set-mark 1",
        "iptables -t mangle -A DIVERT_UDP -j ACCEPT",
        "iptables -t mangle -A PREROUTING -p udp -m socket -j DIVERT_UDP",
    ]

    for conn in connections:
        # 1. Trafic inițial de la EntityA
        lines.append(
            f"iptables -t mangle -A PREROUTING -p udp -s {conn['entityA_ip']} --dport {conn['entityA_port']} "
            f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['entityA_proxy_port_recv']} --tproxy-mark 0x1/0x1"
        )

        # 2. Trafic inițial de la EntityB
        if conn["entityB_port"] != -1:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['entityB_ip']} --dport {conn['entityB_port']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['entityB_proxy_port_recv']} --tproxy-mark 0x1/0x1"
            )
        else:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['entityB_ip']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['entityB_proxy_port_recv']} --tproxy-mark 0x1/0x1"
            )

       
        # 4. SNAT
        lines.append(
            f"iptables -t nat -D POSTROUTING -p udp --sport {conn['entityA_proxy_port_send']} "
            f"-j SNAT --to-source {conn['entityB_ip']}:{conn['entityB_port'] if conn['entityB_port'] != -1 else ''}"
        )
        lines.append(
            f"iptables -t nat -D POSTROUTING -p udp --sport {conn['entityB_proxy_port_send']} "
            f"-j SNAT --to-source {conn['entityA_ip']}:{conn['entityA_port']}"
        )

    lines.append("ip rule add fwmark 1 lookup local || true")
    lines.append("# === END TPROXY ===")
    return "\n".join(lines)

def generate_client_dnat_rules(ip, port, connections, fuzzer):
    # Introducem regula PREROUTING prima, fără dport, apoi restul regulilor
    lines = []
    for conn in connections:
        # Caz EntityA
        if conn["entityA_ip"] == ip and (port == -1 or conn["entityA_port"] == port):
            recv_port = conn['entityA_proxy_port_recv']
            # 1. Regula generală PREROUTING fără port
            lines.append(f"# === PREROUTING pentru proxy EntityA ===")
            lines.append(
                f"iptables -t nat -I PREROUTING 1 -p udp -d {fuzzer['ip']} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            # 2. DNAT pentru ClientA către EntityB
            dport_cond = f"--dport {conn['entityB_port']}" if conn['entityB_port'] != -1 else ""
            lines.append(f"# === DNAT pentru ClientA către {conn['entityB_ip']} ===")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {conn['entityB_ip']} {dport_cond} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            # 3. Accept răspunsuri de la proxy
            lines.append(
                f"iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['entityA_proxy_port_send']} -j ACCEPT"
            )
            # 4. Redirect Local: SEND ➜ RECV pentru ClientA
            lines.append(f"# === REDIRECT SEND ➜ RECV pentru ClientA ===")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {fuzzer['ip']} --dport {conn['entityA_proxy_port_send']} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            break
        # Caz EntityB
        if conn["entityB_ip"] == ip and (port == -1 or conn["entityB_port"] == port or conn["entityB_port"] == -1):
            recv_port = conn['entityB_proxy_port_recv']
            # 1. Regula generală PREROUTING fără port
            lines.append(f"# === PREROUTING pentru proxy EntityB ===")
            lines.append(
                f"iptables -t nat -I PREROUTING 1 -p udp -d {fuzzer['ip']} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            # 2. DNAT pentru ClientB către EntityA
            dport_cond = f"--dport {conn['entityA_port']}"
            lines.append(f"# === DNAT pentru ClientB către {conn['entityA_ip']} ===")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {conn['entityA_ip']} {dport_cond} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            # 3. Accept răspunsuri de la proxy
            lines.append(
                f"iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['entityB_proxy_port_send']} -j ACCEPT"
            )
            # 4. Redirect Local: SEND ➜ RECV pentru ClientB
            lines.append(f"# === REDIRECT SEND ➜ RECV pentru ClientB ===")
            lines.append(
                f"iptables -t nat -A OUTPUT -p udp -d {fuzzer['ip']} --dport {conn['entityB_proxy_port_send']} -j DNAT --to-destination {fuzzer['ip']}:{recv_port}"
            )
            break
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

        # Pentru fuzzer: generăm automat blocul TPROXY
        if role == "fuzzer" and "connections" in entity_cfg:
            tproxy_block = generate_tproxy_block_from_connections(entity_cfg["connections"], ip)
            ef.write(tproxy_block + "\n")

        # Pentru client: inserăm regulile DNAT modificate
        if role != "fuzzer" and is_fuzzed and proto == "udp":
            fuzzer = next((cfg for cfg in all_entities.values() if cfg.get("role") == "fuzzer"), None)
            if fuzzer and "connections" in fuzzer:
                dnat_block = generate_client_dnat_rules(ip, port, fuzzer["connections"], fuzzer)
                ef.write(dnat_block + "\n")
            else:
                ef.write("# Fuzzer connections nu sunt configurate; nu se generează DNAT.\n")

        # Pentru TCP (indiferent de rol), redirecționare neimplementată
        if is_fuzzed and proto == "tcp":
            ef.write("# TCP redirection not implemented\n")

        ef.write('exec "$@"\n')

    os.chmod(entry_path, 0o755)
    log_success(f"Entrypoint script → {entry_path}")

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

    log_success(f"Generated Dockerfile → {out_path}")
