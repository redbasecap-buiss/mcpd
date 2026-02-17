# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] - 2026-02-17

### Added
- Initial release
- MCP Server core with Streamable HTTP transport (spec 2025-03-26)
- JSON-RPC 2.0 message handling via ArduinoJson
- `initialize`, `tools/list`, `tools/call`, `resources/list`, `resources/read`, `ping`
- Capability negotiation
- Session management (`Mcp-Session-Id`)
- mDNS service advertisement (`_mcp._tcp`)
- Built-in tools: GPIO, System, WiFi, I2C
- Python stdio↔HTTP bridge for Claude Desktop integration
- Three examples: basic_server, sensor_hub, home_automation
