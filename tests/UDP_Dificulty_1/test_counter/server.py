import socket

def start_counter_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 9999))
    print("[SERVER] Listening on UDP port 9999")

    counter = 1
    while True:
        data, addr = sock.recvfrom(4096)
        print(f"[SERVER] Received: {data} from {addr}")
        response = str(counter).encode()
        sock.sendto(response, addr)
        print(f"[SERVER] Replied with: {response}")
        counter += 1

if __name__ == "__main__":
    start_counter_server()
