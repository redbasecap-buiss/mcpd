# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 0.27.x  | ✅ Current         |
| < 0.27  | ❌ Not supported   |

## Reporting a Vulnerability

If you discover a security vulnerability in mcpd, please report it responsibly:

1. **Do NOT** open a public GitHub issue
2. Email: **redbasecap-buiss@users.noreply.github.com**
3. Include: description, reproduction steps, affected versions, potential impact

We aim to respond within 48 hours and release a fix within 7 days for critical issues.

## Security Considerations

### Network Exposure

mcpd runs an HTTP server on your microcontroller. By default:

- The server listens on **all interfaces** (0.0.0.0)
- There is **no authentication** enabled by default
- mDNS advertisement makes the device discoverable on the local network

**Recommendations:**

- Always enable API key authentication in production: `server.enableAuth("your-secret-key")`
- Use a strong, randomly generated API key (32+ characters)
- Restrict network access via your router/firewall
- Consider disabling mDNS if discovery isn't needed

### Authentication

mcpd supports multiple authentication methods:

- **API Key** (via `Authorization: Bearer <key>` or `X-API-Key` header)
- **Query parameter** authentication (for environments that can't set headers)
- **Custom authentication callbacks** for integration with external auth systems

### Input Validation

- All JSON-RPC requests are parsed and validated before dispatch
- Unknown methods return standard `-32601 Method not found` errors
- Malformed JSON returns `-32700 Parse error`
- Tool handlers that throw exceptions are caught and returned as `isError` responses
- Resource URIs are validated against registered resources (no path traversal)

### Hardware Access

Tools that interact with hardware (GPIO, I2C, SPI, etc.) execute with full microcontroller privileges. Consider:

- Only register tools that are appropriate for your use case
- Use tool annotations (`readOnlyHint`, `destructiveHint`) to communicate risk to clients
- OTA updates should be protected with authentication

### Transport Security

- **HTTP** (default): No encryption — suitable for trusted local networks only
- **WebSocket**: Same security considerations as HTTP
- **BLE**: Limited range provides some physical security, but no encryption at the application layer

For internet-facing deployments, place mcpd behind a reverse proxy with TLS termination.

## Threat Model

mcpd is designed for **trusted local network** environments. It is NOT designed to be exposed directly to the internet. The primary threats considered are:

1. **Unauthorized local access** → mitigated by API key authentication
2. **Malformed input** → mitigated by input validation and error handling
3. **Resource exhaustion** → mitigated by rate limiting (`MCPRateLimit`)
4. **Firmware tampering** → mitigated by OTA authentication
