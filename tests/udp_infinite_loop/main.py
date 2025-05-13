import socket
import time
import sys
import threading

def parse_ip_port(arg):
    try:
        ip, port = arg.split(":")
        return ip, int(port)
    except ValueError:
        print(f"Invalid IP:PORT format: {arg}")
        sys.exit(1)

def sender(sock, local_ip, local_port, destinations):
    msg_count = 0
    while True:
        msg_count += 1
        message = f"[{local_ip}:{local_port}] Message no {msg_count}"
        encoded = message.encode()

        for dest_ip, dest_port in destinations:
            try:
                sock.sendto(encoded, (dest_ip, dest_port))
                print(f"[{local_ip}:{local_port}] → {dest_ip}:{dest_port} : {message}", flush=True)
            except Exception as e:
                print(f"[{local_ip}:{local_port}] Failed to send to {dest_ip}:{dest_port}: {e}", flush=True)

        time.sleep(1)

def receiver(sock):
    while True:
        try:
            data, addr = sock.recvfrom(2048)
            print(f"[RECEIVED from {addr[0]}:{addr[1]}] {data.decode(errors='ignore')}", flush=True)
        except Exception as e:
            print(f"[RECEIVER ERROR] {e}", flush=True)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 main.py <LOCAL_IP:PORT> <DEST1_IP:PORT> [<DEST2_IP:PORT> ...]")
        sys.exit(1)

    local_ip, local_port = parse_ip_port(sys.argv[1])
    destinations = [parse_ip_port(arg) for arg in sys.argv[2:]]

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((local_ip, local_port))
    print(f"[{local_ip}:{local_port}] started (sending & receiving)", flush=True)

    # Start receiver thread
    recv_thread = threading.Thread(target=receiver, args=(sock,), daemon=True)
    recv_thread.start()

    # Run sender in main thread
    sender(sock, local_ip, local_port, destinations)
