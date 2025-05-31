import socket
import time

def send_counter_requests(server_ip='192.168.1.10', server_port=9999):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2)

    for i in range(5):
        msg = f"msg {i}".encode()
        print(f"[CLIENT] Sending: {msg}")
        sock.sendto(msg, (server_ip, server_port))
        try:
            data, _ = sock.recvfrom(4096)
            print(f"[CLIENT] Received: {data}")
        except socket.timeout:
            print("[CLIENT] No response received")

        time.sleep(1)

if __name__ == "__main__":
    send_counter_requests()
