import argparse
from .utils import CONFIG_PATH, LAUNCHER_PORT
from .redirection_injector import inject_fuzzer_redirections
from .config_loader import load_config, export_entities_minimal_json
from .docker_network import create_docker_network
from .docker_build import generate_dockerfile, generate_all_dockerfiles
from .docker_launcher import launch_all_entities, run_launcher
# from .docker_launcher import connect_to_launcher  # dacă vrei mai târziu

def main():
    parser = argparse.ArgumentParser(description="Commander for MITM fuzzer infrastructure")
    parser.add_argument("--config", default="config.yaml", help="Path to config YAML")
    parser.add_argument("--template", default="Dockerfile.template", help="Path to Dockerfile template")
    parser.add_argument("--standby", action="store_true", help="Start containers in standby mode (no auto-exec)")
    args = parser.parse_args()

    # 1. Injectează redirecțiile pentru fuzzer
    inject_fuzzer_redirections(args.config)

    # 2. Încarcă configul
    cfg = load_config(args.config)

    # 3. Export JSON auxiliar
    export_entities_minimal_json(cfg)

    # 4. Creează rețea Docker
    create_docker_network(cfg.get("network", {}))

    # 5. Generează Dockerfile-uri + entrypoint-uri
    generate_all_dockerfiles(cfg.get("entities", {}), args.template)


    # 6. Build + run containere
    launch_all_entities(config=cfg, standby=args.standby)

    # 7. Pornește launcher-ele
    run_launcher(config=cfg, config_path=args.config)

    # 8. (Opțional) conectează la fuzzer
    # connect_to_launcher(config=cfg, launcher_port=LAUNCHER_PORT)

if __name__ == "__main__":
    main()
