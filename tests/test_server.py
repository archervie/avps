import socket
import subprocess
import threading
import time

TARGET_HOST = "127.0.0.1"
OPEN_PORT = 55555
CLOSED_PORT = 55556
SCANNER_BIN = "../bin/avps"


class LocalTestServer:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.sock = None
        self.running = False
        self.thread = None

    def start(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Allow fast re-binding during automated tests
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)
        self.running = True

        self.thread = threading.Thread(target=self._listen_loop, daemon=True)
        self.thread.start()
        # Brief pause to ensure socket is ready
        time.sleep(0.1)

    def _listen_loop(self) -> None:
        while self.running:
            try:
                if self.sock:
                    self.sock.settimeout(0.5)
                    client_sock, _ = self.sock.accept()
                    # Accept connection and close it immediately
                    client_sock.close()
            except TimeoutError:
                continue
            except OSError:
                break

    def stop(self) -> None:
        self.running = False
        if self.sock:
            self.sock.close()
        if self.thread:
            self.thread.join(timeout=1.0)


def run_scanner_test(mode: str) -> None:
    # Runs the scan on the local server; type of scan
    # depends on input
    print(f"Running scan with mode {mode}...")
    cmd = [
        SCANNER_BIN,
        "-m",
        mode,
        "-k",
        f"{OPEN_PORT},{CLOSED_PORT}",
        TARGET_HOST,
    ]

    print(f"Executing scanner: {' '.join(cmd)}")
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    output = result.stdout + result.stderr
    print("\nScanner Output:\n")
    print(output.strip())
    open_detected = (
        f"Connection successful on port {OPEN_PORT}" in output
        or f"SYN-ACK received on port {OPEN_PORT}" in output
        or f"No RST received on port {OPEN_PORT}" in output
    )
    closed_detected = (
        f"Connection failed on port {CLOSED_PORT}" in output
        or f"RST received on port {CLOSED_PORT}" in output
    )
    print("\n")
    if open_detected:
        print(f"[PASS] Port {OPEN_PORT} was correctly identified as OPEN.")
    else:
        print(f"[FAIL] Port {OPEN_PORT} was NOT detected as open.")
    if closed_detected:
        print(f"[PASS] Port {CLOSED_PORT} was correctly identified as CLOSED.")
    else:
        print(f"[FAIL] Port {CLOSED_PORT} was NOT detected as closed.")

    print("--------")


if __name__ == "__main__":
    print(f"Starting local TCP test server on {TARGET_HOST}:{OPEN_PORT}...")
    server = LocalTestServer(TARGET_HOST, OPEN_PORT)
    server.start()

    run_scanner_test("connect")
    run_scanner_test("syn")
    run_scanner_test("fin")
    run_scanner_test("null")

    print("Stopping test server...")
    server.stop()
