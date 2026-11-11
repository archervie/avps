# avps Architecture and Implementation Guide

The `avps` port scanner is designed for high performance through concurrent execution and low-level socket manipulation. This guide details the internal implementation of the two primary scanning engines: the connect scanner and the stealth scanner.

---

## Threading and Dispatch Model

Both scanning engines rely on POSIX threads and atomic variables to distribute workloads efficiently without the overhead of mutex locks.

- The scanner determines the optimal number of worker threads by calling `get_nprocs()` to detect the number of available CPU cores.
- Work distribution is handled via `<stdatomic.h>`.
- Depending on whether a user provided a port range or a specific list, threads use `atomic_fetch_add()` to atomically increment and claim either a `current_port` integer or a `current_idx` array index.
- Global counters for `successes`, `failures`, and `timeouts` are also updated atomically by the worker threads during connect scans.

## Connect Scan Implementation (`src/scans/connect_scan.c`)

The connect scan performs a standard TCP three-way handshake using the operating system's native networking stack.

- **Socket Initialization:** The engine creates standard stream sockets using `AF_INET` and `SOCK_STREAM`.
- **Non-blocking I/O:** To prevent threads from stalling on unresponsive hosts, the socket is immediately set to non-blocking mode using `fcntl` with the `O_NONBLOCK` flag.
- **Connection Handling:** The thread initiates the connection using `connect()`. Because the socket is non-blocking, this often returns `EINPROGRESS`.
- **Timeout Management:** The scanner uses `select()` to wait for the socket to become writable within the user-defined `timeout` window.
- **State Verification:** If `select()` indicates the socket is ready, the program calls `getsockopt()` with `SO_ERROR` to verify if the connection was actually successful (error code `0`) or if it failed.
- **Result Logging:** The results are mapped to a global `scan_results` array, recording `1` for success, `2` for failure, or `3` for a timeout.

## SYN Scan Implementation (`src/scans/syn_scan.c`)

The stealth scan bypasses the standard TCP handshake by manually crafting and transmitting IP and TCP packets to identify open ports via SYN-ACK responses.

- **Raw Sockets:** The engine initializes raw sockets using `SOCK_RAW` and `IPPROTO_TCP` defined under `src/utils/scan_utils.c`.
- **Manual Headers:** The `IP_HDRINCL` socket option is set via `setsockopt()`, informing the kernel that the application will provide its own IP header.
- **IP Header Construction:** The scanner builds the IPv4 header according to RFC 791, setting the version to 4, the TTL to 64, and specifying the source and destination addresses. The `check` field is left as `0`, allowing the operating system to calculate the IP checksum.
- **TCP Header Construction:** The TCP header is constructed according to RFC 9293. The thread sets the `syn` flag to `1`, the window size to `5840`, and zeroes out the sequence, acknowledgment, and other control flags.
- **Checksum Calculation:** Unlike the IP header, the application must manually calculate the TCP checksum. It creates a pseudo-header containing the source address, destination address, protocol, and TCP length, which is concatenated with the TCP header and passed to the `csum()` utility function defined in `src/utils/utils.c`.
- **Packet Transmission and Reception:** Packets are sent via `sendto()`. The thread then enters a `recvfrom()` loop, using `select()` to enforce the timeout.
- **Response Parsing:** Incoming packets are parsed to separate the IP and TCP headers. If the received packet matches the target ports, the scanner checks the flags: a `syn` and `ack` combination indicates an open port (success), while an `rst` flag indicates a closed port (failure).

## FIN & Null Scan Implementation (`src/scans/fin_scan.c` & `src/scans/null_scan.c`)

- **Raw Sockets**: Similar to the SYN scan, FIN and null scans both use the same raw socket creation function.
- **TCP Header Construction**: For both scans, they both use the common `build_tcp_header()` function and apply the rest of the flags as per the kind of scan. For FIN scans, the `fin` flag is set to 1, while for null scans, all flags are set to `0`.
- **Response Parsing**: For both scans, incoming packets are both parsed and checked. They both specifically look for RST responses, which under RFC 9293 are meant to represent a closed port. Timeouts, however, signal no response and may indicate either an open port or a filtered port. The scanner is unable to differentiate between the two; you can use a SYN scan afterward to test specifically for open ports.

## Support Utilities (`src/utils/scan_utils.c` & `src/utils/utils.c`)

The core engines rely on several helper functions defined in `scan_utils.c` and `utils.c` to handle memory and networking prerequisites.

- **Local IP Resolution**: To accurately create source addresses for stealth scans, `get_source_addr()` creates a dummy UDP datagram socket (`SOCK_DGRAM`) and attempts to connect to port 80 of the target host. It then uses `getsockname()` to extract the local routing IP address chosen by the kernel before closing the socket.
- **Checksum Math**: The `csum()` function implements the standard 16-bit one's complement sum required by TCP/IP protocols to validate data integrity across the pseudo-header.
- **Port Parsing**: The `parse_ports()` function uses `strtok()` to split comma-separated port strings, dynamically allocating and reallocating memory for a `uint16_t` array as it converts the tokens into integers.
- **Scan Utilities**: `scan_utils.c` consists of various functions that centralize repetitive tasks into a singular function call, such as `create_raw_socket()`, `build_ip_header()`, `build_tcp_header()`, and `calculate_tcp_checksum()`.

## Tests (`test/test_checksum.c` & `test/test_server.py`)

You can run the two test scripts created for certain functions of the scanner. `test/test_checksum.c` tests the checksum calculations, while `test/test_server.py` tests the actual scanner outputs by creating a simple TCP server on a specific port. You can create the test binary for the checksum through `make test` and running `./bin/run_test`, and you can run `sudo python3 tests/test_server.py` for the server test. You are free to create more tests or update the current test scripts to reflect any changes.

---

## Contribute to avps

To begin, if you'd like to contribute, then let me (a5v) extend you a thanks in doing so. This is still a novice project, so any contributions are welcome! Furthermore, contributing here is quite lax compared to usual.

**Local Setup**: Simply fork the project and clone that to begin making the changes you'd like to make. No external dependencies are required aside from C and Make.
**Making Changes**: Create a branch that reflects what you plan to add or change. Try to keep strictly to the issue you're resolving.
**Code Commenting**: It's preferable to comment on all changes made to ensure code readability, and updating the documentation to reflect those changes.
**Testing Code**: C code can be crazy at times, so make sure to test your code locally first. Common ports are `127.0.0.1`, `127.0.0.4`, or `192.168.1.1`. You can also run the test scripts under `tests/`.
**Submit a Pull Request**: Create a pull request when you're done. It'll be reviewed and feedback will be provided, and the change possibly pulled into `main`.
