import socket
import time

def send_echo_messages(server_ip='192.168.1.10', server_port=9999):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    messages = [b"hello", b"fuzz", b"test"]
    for msg in messages:
        print(f"[CLIENT] Sending: {msg}", flush=True)
        sock.sendto(msg, (server_ip, server_port))
        try:
            data, _ = sock.recvfrom(4096)
            print(f"[CLIENT] Received: {data}", flush=True)
        except socket.timeout:
            print("[CLIENT] No response received", flush=True)

        time.sleep(1)

if __name__ == "__main__":
    send_echo_messages()
