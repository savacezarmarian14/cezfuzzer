import socket

HOST = '192.168.1.10'
PORT = 12345

def start_client():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        print(f"[Client] Connected to {HOST}:{PORT}")
        while True:
            message = b"[Client] Hello"
            s.sendall(message)
            data = s.recv(1024)
            print(f"[Client] Received: {data.decode()}")

if __name__ == "__main__":
    start_client()
