/**
 * mcpd — Transport Layer
 *
 * Implements MCP Streamable HTTP transport (2025-03-26 spec).
 *
 * The transport is handled directly by the Server class using WebServer.
 * This header defines transport-related constants and utilities.
 */

#ifndef MCPD_TRANSPORT_H
#define MCPD_TRANSPORT_H

#include <Arduino.h>

namespace mcpd {
namespace transport {

// Content types
constexpr const char* CONTENT_TYPE_JSON = "application/json";
constexpr const char* CONTENT_TYPE_SSE = "text/event-stream";

// Headers
constexpr const char* HEADER_SESSION_ID = "Mcp-Session-Id";
constexpr const char* HEADER_ACCEPT = "Accept";
constexpr const char* HEADER_CONTENT_TYPE = "Content-Type";

// CORS headers for browser-based clients
inline void setCORSHeaders(WebServer& server) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers",
                      "Content-Type, Accept, Mcp-Session-Id, Authorization, X-API-Key");
    server.sendHeader("Access-Control-Expose-Headers", "Mcp-Session-Id");
}

} // namespace transport
} // namespace mcpd

#endif // MCPD_TRANSPORT_H
