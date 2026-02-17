#!/usr/bin/env python3
"""
mcpd-bridge — stdio ↔ Streamable HTTP bridge for mcpd

Translates MCP stdio transport (used by Claude Desktop) to Streamable HTTP
transport (used by mcpd on the microcontroller).

Usage:
    python3 mcpd_bridge.py --host <hostname-or-ip> [--port 80] [--path /mcp]

Claude Desktop config (claude_desktop_config.json):
    {
        "mcpServers": {
            "my-device": {
                "command": "python3",
                "args": ["/path/to/mcpd_bridge.py", "--host", "my-device.local"]
            }
        }
    }
"""

import sys
import json
import argparse
import urllib.request
import urllib.error
import logging

logging.basicConfig(
    level=logging.DEBUG,
    format="[mcpd-bridge] %(levelname)s %(message)s",
    stream=sys.stderr,
)
log = logging.getLogger("mcpd-bridge")


def main():
    parser = argparse.ArgumentParser(description="mcpd stdio↔HTTP bridge")
    parser.add_argument("--host", required=True, help="MCU hostname or IP (e.g. my-device.local)")
    parser.add_argument("--port", type=int, default=80, help="HTTP port (default: 80)")
    parser.add_argument("--path", default="/mcp", help="MCP endpoint path (default: /mcp)")
    parser.add_argument("--discover", action="store_true",
                        help="Discover MCU via mDNS (requires zeroconf)")
    args = parser.parse_args()

    if args.discover:
        host, port = discover_mcu()
        if not host:
            log.error("mDNS discovery failed")
            sys.exit(1)
    else:
        host = args.host
        port = args.port

    base_url = f"http://{host}:{port}{args.path}"
    session_id = None

    log.info(f"Bridge started → {base_url}")

    # Read JSON-RPC messages from stdin, forward to MCU via HTTP POST
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            # Validate it's proper JSON
            msg = json.loads(line)
        except json.JSONDecodeError as e:
            log.error(f"Invalid JSON from stdin: {e}")
            continue

        log.debug(f"→ MCU: {line[:200]}")

        # Build HTTP request
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if session_id:
            headers["Mcp-Session-Id"] = session_id

        req = urllib.request.Request(
            base_url,
            data=line.encode("utf-8"),
            headers=headers,
            method="POST",
        )

        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                # Capture session ID from initialize response
                resp_session = resp.headers.get("Mcp-Session-Id")
                if resp_session:
                    session_id = resp_session
                    log.debug(f"Session: {session_id}")

                status = resp.status

                if status == 202:
                    # Notification accepted, no response to forward
                    log.debug("← MCU: 202 Accepted")
                    continue

                body = resp.read().decode("utf-8")
                log.debug(f"← MCU: {body[:200]}")

                # Forward response to stdout (back to MCP client)
                sys.stdout.write(body + "\n")
                sys.stdout.flush()

        except urllib.error.HTTPError as e:
            body = e.read().decode("utf-8", errors="replace")
            log.error(f"HTTP {e.code}: {body}")

            # If 404, session expired — clear session
            if e.code == 404:
                session_id = None
                log.warning("Session expired, will re-initialize on next request")

            # Forward error as JSON-RPC error
            error_response = {
                "jsonrpc": "2.0",
                "id": msg.get("id"),
                "error": {
                    "code": -32000,
                    "message": f"HTTP {e.code}: {body}",
                },
            }
            sys.stdout.write(json.dumps(error_response) + "\n")
            sys.stdout.flush()

        except urllib.error.URLError as e:
            log.error(f"Connection error: {e.reason}")
            error_response = {
                "jsonrpc": "2.0",
                "id": msg.get("id"),
                "error": {
                    "code": -32000,
                    "message": f"Connection error: {e.reason}",
                },
            }
            sys.stdout.write(json.dumps(error_response) + "\n")
            sys.stdout.flush()

        except Exception as e:
            log.error(f"Unexpected error: {e}")

    log.info("stdin closed, bridge exiting")


def discover_mcu():
    """Discover mcpd server via mDNS (optional, requires zeroconf)."""
    try:
        from zeroconf import ServiceBrowser, Zeroconf
        import time

        result = {"host": None, "port": None}

        class Listener:
            def add_service(self, zc, type_, name):
                info = zc.get_service_info(type_, name)
                if info:
                    from ipaddress import ip_address
                    addr = ip_address(info.addresses[0])
                    result["host"] = str(addr)
                    result["port"] = info.port
                    log.info(f"Discovered: {name} at {addr}:{info.port}")

            def remove_service(self, zc, type_, name):
                pass

            def update_service(self, zc, type_, name):
                pass

        zc = Zeroconf()
        browser = ServiceBrowser(zc, "_mcp._tcp.local.", Listener())

        # Wait up to 5 seconds for discovery
        for _ in range(50):
            if result["host"]:
                break
            time.sleep(0.1)

        zc.close()
        return result["host"], result["port"]

    except ImportError:
        log.error("zeroconf not installed. Run: pip install zeroconf")
        return None, None


if __name__ == "__main__":
    main()
