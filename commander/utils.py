import sys
from colorama import init, Fore, Style

# Initialize colorama
init(autoreset=True)

# Constante globale
CONFIG_PATH = None
LAUNCHER_PORT = 23927

# Logging helpers
def log_info(msg):
    print(f"{Fore.BLUE}[INFO]{Style.RESET_ALL} {msg}")

def log_success(msg):
    print(f"{Fore.GREEN}[OK]{Style.RESET_ALL} {msg}")

def log_warning(msg):
    print(f"{Fore.YELLOW}[WARNING]{Style.RESET_ALL} {msg}")

def log_error(msg):
    print(f"{Fore.RED}[ERROR]{Style.RESET_ALL} {msg}")
    sys.exit(1)
