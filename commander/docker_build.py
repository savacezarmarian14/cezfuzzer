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
        # Reguli pentru trafic A -> B
        if conn["dst_port"] != -1:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['src_ip']} --dport {conn['dst_port']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['port_src_proxy']} --tproxy-mark 0x1/0x1"
            )
        else:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['src_ip']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['port_src_proxy']} --tproxy-mark 0x1/0x1"
            )

        # Reguli pentru trafic B -> A
        if conn["src_port"] != -1:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['dst_ip']} --dport {conn['src_port']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['port_dst_proxy']} --tproxy-mark 0x1/0x1"
            )
        else:
            lines.append(
                f"iptables -t mangle -A PREROUTING -p udp -s {conn['dst_ip']} "
                f"-j TPROXY --on-ip {fuzzer_ip} --on-port {conn['port_dst_proxy']} --tproxy-mark 0x1/0x1"
            )

        # Reguli SNAT
        if conn["src_port"] != -1:
            lines.append(
                f"iptables -t nat -A POSTROUTING -p udp --sport {conn['port_src_proxy']} "
                f"-j SNAT --to-source {conn['src_ip']}:{conn['src_port']}"
            )
        else:
            lines.append(
                f"iptables -t nat -A POSTROUTING -p udp --sport {conn['port_src_proxy']} "
                f"-j SNAT --to-source {conn['src_ip']}"
            )
            
        if conn["dst_port"] != -1:
            lines.append(
                f"iptables -t nat -A POSTROUTING -p udp --sport {conn['port_dst_proxy']} "
                f"-j SNAT --to-source {conn['dst_ip']}:{conn['dst_port']}"
            )
        else:
            lines.append(
                f"iptables -t nat -A POSTROUTING -p udp --sport {conn['port_dst_proxy']} "
                f"-j SNAT --to-source {conn['dst_ip']}"
            )

    lines.append("ip rule add fwmark 1 lookup local || true")
    lines.append("# === END TPROXY ===")
    return "\n".join(lines)

def generate_client_dnat_rules(ip, port, connections, fuzzer):
    lines = []
    for conn in connections:
        # Caz 1: Entitatea curentă este sursa în conexiune
        if conn["src_ip"] == ip:
            # Verificăm dacă portul entității corespunde sau este dinamic
            if port == -1 or conn["src_port"] == port or conn["src_port"] == -1:
                # Regulă pentru trafic către destinație
                dport_condition = f"--dport {conn['dst_port']}" if conn["dst_port"] != -1 else ""
                lines.append(f"""
# === CLIENT → PROXY DNAT for {conn['dst_ip']}:{'ANY' if conn['dst_port'] == -1 else conn['dst_port']} ===
iptables -t nat -A OUTPUT -p udp -d {conn['dst_ip']} {dport_condition} -j DNAT --to-destination {fuzzer['ip']}:{conn['port_src_proxy']}
iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['port_src_proxy']} -j ACCEPT
# === END ===
""")

        # Caz 2: Entitatea curentă este destinația în conexiune
        if conn["dst_ip"] == ip:
            # Verificăm dacă portul entității corespunde sau este dinamic
            if port == -1 or conn["dst_port"] == port or conn["dst_port"] == -1:
                # Regulă pentru trafic către sursă
                dport_condition = f"--dport {conn['src_port']}" if conn["src_port"] != -1 else ""
                lines.append(f"""
# === CLIENT → PROXY DNAT for {conn['src_ip']}:{'ANY' if conn['src_port'] == -1 else conn['src_port']} ===
iptables -t nat -A OUTPUT -p udp -d {conn['src_ip']} {dport_condition} -j DNAT --to-destination {fuzzer['ip']}:{conn['port_dst_proxy']}
iptables -A INPUT -p udp -s {fuzzer['ip']} --sport {conn['port_dst_proxy']} -j ACCEPT
# === END ===
""")
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
        port = entity_cfg.get("port", -1)  # Default la -1 dacă nu există
        proto = entity_cfg.get("protocol", "udp").lower()
        is_fuzzed = entity_cfg.get("fuzzed", False)

        if role == "fuzzer" and "connections" in entity_cfg:
            tproxy_block = generate_tproxy_block_from_connections(entity_cfg["connections"], ip)
            ef.write(tproxy_block + "\n")

        if is_fuzzed and role != "fuzzer" and proto == "udp":
            fuzzer = next((e for e in all_entities.values() if e.get("role") == "fuzzer"), None)
            if fuzzer and "connections" in fuzzer:
                dnat_block = generate_client_dnat_rules(ip, port, fuzzer["connections"], fuzzer)
                ef.write(dnat_block + "\n")

        elif is_fuzzed and proto == "tcp":
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