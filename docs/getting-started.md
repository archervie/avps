# Getting Started with avps

This guide will walk you through compiling `avps` and running your first network scans.

## 1. Choose Your Scan Mode

As of version 1.0, `avps` requires you to explicitly declare the type of scan you want to perform using the `-m` or `--mode` flag. There are two options:

- **Connect (`-m connect`):** Performs a full TCP three-way handshake with the host at every visited port. This does not require root privileges, but it is highly likely to be noticed and logged by the target system.
- **SYN (`-m syn`):** Sends only a SYN flag to the host to pretend to initialize a connection. The scanner checks if the host sends a corresponding SYN-ACK flag and records the result. This is much less likely to be logged, but requires **root privileges (sudo)** to execute.
- **FIN (`-m fin`):** Sends only a FIN flag to the host. The scan checks for the corresponding RST flag, but marks down the port as open or filtered if no response is received within the timeout. This also requires **root**.
- **Null (`-m null`):** Sends a TCP packet with no flags set to the host. The results are similar to the FIN scan, and requires **root**.

## 2. Define Your Targets

By default, `avps` scans the full range of ports from 1 to 65,535. You can narrow this down in two ways:

- **By Range (`-r` or `--range`):** Define a specific start and end point. For example, `-r 1-800` will scan all ports from 1 to 800.
- **By List (`-k` or `--known-ports`):** Provide a comma-separated list of specific ports. For example, `-k 22,80,443`. _(Note: If you provide both flags, the `-k` list will override the `-r` range)._

## 3. Tune the Scanner

For wide or remote scans, you can adjust the scanner's behavior to suit your network environment:

- **Timeout (`-t` or `--timeout`):** Defines the amount of time (in seconds) the scanner waits for a response. The default is `1.0`. You can use floating-point values (e.g., `-t 0.5`). It is generally not recommended to drop below 1.0 seconds for non-local scanning, as you may miss open ports due to network latency.
- **Filter Output (`-s` or `--success-only`):** By default, `avps` outputs the result for every port scanned, including failures. For wide scans where the majority of ports are closed, append the `-s` flag so the terminal only prints successful, open connections.

## 4. Choose your Host

The scanner requires you specify a host (does not require an option flag) at the end of the command. The host is represented by an IPv4 address.

- It is **required** to have explicit permission to scan the host network, as doing so without authorization can appear as the reconnissance stage of a cyberattack, and have your IP address permanently blacklisted by the service's firewall, even if that was not the intent.
  _(Note: While IPv6 support and DNS resolution are currently on the list of things to add, they are currently not supported)._

## 5. Example LAN Workflows

**The "Quick Check" (Localhost)**
Scan your own machine for common web and SSH ports, hiding closed ports from the output:
`./bin/avps -k 22,80,443,8080 -s 127.0.0.1`

**The "Quiet Recon" (Remote)**
Perform a stealth scan on the first 1024 ports of a remote server in your local network (requires `sudo`):
`sudo ./bin/avps -m syn -r 1-1024 192.168.1.50`

**The "Aggressive Sweep" (Local Network)**
Scan all 65,535 ports on a local router with a shortened timeout:
`./bin/avps -m connect -t 0.5 -s 192.168.1.1`

## 6. Real-World Testing

If you wish to go beyond your LAN, below are some real-world services you can use `avps` on to test which are verified to allow scans:

1. **scanme.nmap.org**: 45.33.32.156
2. **testphp.vulnweb.com**: 44.228.249.3
3. **telehack.com**: 64.13.139.230
