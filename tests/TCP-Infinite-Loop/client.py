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

            # Recv loop until server closes connection or no more data
            chunks = []
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                if len(chunk) < 4096:
                    break  # optional: assume server sent full message
            full_data = b''.join(chunks)
            print(f"[Client] Received: {full_data.decode(errors='replace')}")

if __name__ == "__main__":
    start_client()
