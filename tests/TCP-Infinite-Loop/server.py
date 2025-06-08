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
                data = conn.recv(1024)
                if not data:
                    break
                print(f"[Server] Received: {data.decode()}")
                response = b"[Server] " + data
                conn.sendall(response)

if __name__ == "__main__":
    start_server()