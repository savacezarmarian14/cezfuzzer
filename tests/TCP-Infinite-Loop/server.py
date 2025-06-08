import socket

HOST = '192.168.1.10'
PORT = 12345

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"[Server] Listening on {HOST}:{PORT}", flush=True)

        conn, addr = s.accept()
        with conn:
            print(f"[Server] Connected by {addr}", flush=True)
            while True:
                chunks = []
                while True:
                    data = conn.recv(4096)
                    if not data:
                        break  # client closed connection
                    chunks.append(data)
                    if len(data) < 4096:
                        break  # end of message
                if not chunks:
                    break  # connection fully closed

                full_data = b''.join(chunks)
                print(f"[Server] Received: {full_data.decode(errors='replace')}")
                response = b"[Server] " + full_data
                conn.sendall(response)

if __name__ == "__main__":
    start_server()
