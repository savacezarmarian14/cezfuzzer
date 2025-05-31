import socket

def start_echo_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 9999))
    print("[SERVER] Listening on UDP port 9999", flush=True)

    while True:
        data, addr = sock.recvfrom(4096)
        print(f"[SERVER] Received: from {addr}",flush=True)
        sock.sendto(data, addr)
        print(f"[SERVER] sent:to {addr}",flush=True)


if __name__ == "__main__":
    start_echo_server()