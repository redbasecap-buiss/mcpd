# mcpd-bridge

Translates **stdio** (what Claude Desktop speaks) to **Streamable HTTP** (what your MCU speaks).

## Usage

```bash
python3 mcpd_bridge.py --host my-device.local
```

### With mDNS discovery

```bash
pip install zeroconf
python3 mcpd_bridge.py --discover
```

### Claude Desktop config

```jsonc
// ~/Library/Application Support/Claude/claude_desktop_config.json  (macOS)
// %APPDATA%\Claude\claude_desktop_config.json                      (Windows)
{
  "mcpServers": {
    "my-esp32": {
      "command": "python3",
      "args": ["/absolute/path/to/mcpd/host/mcpd_bridge.py", "--host", "my-esp32.local"]
    }
  }
}
```

## Requirements

- Python 3.7+
- No dependencies for basic usage (uses only stdlib)
- Optional: `zeroconf` for `--discover` mode
