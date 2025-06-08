import argparse
from .utils import CONFIG_PATH, LAUNCHER_PORT
from .redirection_injector import inject_fuzzer_redirections, inject_tcp_redirections
from .config_loader import load_config, normalize_udp_ports
from .docker_network import create_docker_network
from .docker_build import generate_dockerfile, generate_all_dockerfiles
from .docker_launcher import launch_all_entities, run_launcher

def main():
    parser = argparse.ArgumentParser(description="Commander for MITM fuzzer infrastructure")
    parser.add_argument("--config", default="config.yaml", help="Path to config YAML")
    parser.add_argument("--template", default="Dockerfile.template", help="Path to Dockerfile template")
    parser.add_argument("--standby", action="store_true", help="Start containers in standby mode (no auto-exec)")
    args = parser.parse_args()

    normalize_udp_ports(args.config)

    inject_fuzzer_redirections(args.config)
    inject_tcp_redirections(args.config)


    cfg = load_config(args.config)

    create_docker_network(cfg.get("network", {}))

    generate_all_dockerfiles(cfg.get("entities", {}), args.template)

    launch_all_entities(config=cfg, standby=args.standby)

    run_launcher(config=cfg, config_path=args.config)


if __name__ == "__main__":
    main()
