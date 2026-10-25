# avps (Archervie's Port Scanner)

avps is a high-performance, multithreaded IPv4 port scanner written in C. It uses POSIX threads and non-blocking sockets to rapidly scan targets, offering both standard TCP connect scans and stealth (SYN) scans via raw sockets.

## Features

- **Multithreaded Execution:** Automatically detects the number of available CPU cores and spawns optimized worker threads to maximize scanning throughput.
- **Multiple Scan Modes:**
  - Connect Scan (`-m connect`): Completes the full TCP three-way handshake using standard system socket APIs.
  - Stealth Scan (`-m stealth`): Constructs raw IPv4 and TCP SYN packets to identify open ports without completing the full connection, minimizing logs on the target system (requires root/sudo to run).
- **Flexible Targeting:** Scan custom ranges (e.g., 1-1024) or a comma-separated list of known ports (e.g., 22,80,443).
- **Configurable Timeouts:** Adjust the timeout duration per connection to account for network latency.

## Prerequisites

- A Linux/Unix-based operating system.
- `gcc` and `make` installed.
- **Root privileges** are required to execute stealth scans (due to the use of raw sockets for packet sending).

## Build Instructions

This project includes a standard `Makefile`. To build the project, simply clone the repository and run `make`:

```bash
git clone https://github.com/archervie/avps.git
cd avps
make
./bin/avps ...
```

## Usage
```./bin/avps [-m --mode mode] [-r --range start-end] [-t --timeout timeout] [-s --success-only] [-k --known-ports] host```

- **-m --mode**: Choose between 'connect' or 'stealth' depending on the type of scan you want.
- **-r --range**: Choose a range in the format of start-end (e.g. 1-1024) where end > start. This is the range of ports to scan.
- **-t --timeout**: Choose the timeout for the packets before the program moves on. Use a floating point number for this.
- **-s --success-only**: Choose to select only the successful connections. Useful if scanning a large number of ports.
- **-k --known-ports**: Scan through a list of user-inputted ports instead of a range. Do so through a comma-separated list (e.g. 22,80,443).

Do note that -k overpowers -r due to how the scanner is programmed.

## Examples
1. ```./bin/avps -m connect -r 1-1000 -s 192.168.1.1```
This performs a connect scan from ports 1 to 1000 on 192.168.1.1.

2. ```sudo ./bin/avps -m stealth -k 22,80,443,8080 192.168.1.1```
This performs a stealth scan on ports 22, 80, 443, and 8080 on 192.168.1.1.

3. ```./bin/avps -m connect -t 0.1 127.0.0.1```
This performs a connect scan from ports 1 to 65,535 with a timeout of 0.1s on 127.0.0.1.

## Architectural Notes
- The scanner uses <stdatomic.h> to safely distribute ports across worker threads without the overhead of mutex locks, making it efficient.
- Stealth mode manually constructs IP and TCP headers in compliance with RFC 791 and RFC 9293, and calculates necessary pseudo-header checksums. However, the IP checksum is left to the operating system unlike the TCP checksum.


## Disclaimer
This tool is intended for educational purposes and network administration. Only scan networks and hosts that you own or have explicit authorization to test.
