/**
 * mcpd — Server Diagnostics Tool
 *
 * Exposes server self-inspection as an MCP tool: uptime, registered tools/resources/prompts,
 * active transports, session info, memory usage, and version info.
 *
 * Include after mcpd.h. Usage:
 *   #include <mcpd.h>
 *   #include <mcpd/MCPDiagnostics.h>
 *   mcpd::addDiagnosticsTool(mcp);
 *
 * MIT License
 */

#ifndef MCPD_DIAGNOSTICS_H
#define MCPD_DIAGNOSTICS_H

#include "mcpd.h"

namespace mcpd {

static unsigned long _diagBootMs = millis();

/**
 * Register the server diagnostics tool.
 * Provides a comprehensive JSON snapshot of the running MCP server state.
 */
inline void addDiagnosticsTool(Server& server) {

    MCPTool diagTool;
    diagTool.name = "server_diagnostics";
    diagTool.description = "Get comprehensive server diagnostics: version, uptime, registered capabilities, memory, transport status.";
    diagTool.inputSchemaJson = R"({
        "type": "object",
        "properties": {
            "include_tools": {
                "type": "boolean",
                "description": "Include list of registered tool names",
                "default": false
            },
            "include_resources": {
                "type": "boolean",
                "description": "Include list of registered resource URIs",
                "default": false
            }
        }
    })";
    diagTool.annotations.title = "Server Diagnostics";
    diagTool.annotations.readOnlyHint = true;

    diagTool.handler = [&server](const JsonObject& params) -> String {
        unsigned long uptimeMs = millis() - _diagBootMs;
        unsigned long uptimeSec = uptimeMs / 1000;

        JsonDocument doc;

        // Version info
        JsonObject version = doc["version"].to<JsonObject>();
        version["mcpd"] = MCPD_VERSION;
        version["mcp_protocol"] = MCPD_MCP_PROTOCOL_VERSION;

        // Server info
        doc["name"] = server.getName();
        doc["port"] = server.getPort();

        // Uptime
        JsonObject uptime = doc["uptime"].to<JsonObject>();
        uptime["seconds"] = uptimeSec;
        uptime["formatted"] = String(uptimeSec / 86400) + "d " +
                               String((uptimeSec % 86400) / 3600) + "h " +
                               String((uptimeSec % 3600) / 60) + "m " +
                               String(uptimeSec % 60) + "s";

        // Memory (ESP32)
#ifdef ESP32
        JsonObject memory = doc["memory"].to<JsonObject>();
        memory["free_heap"] = ESP.getFreeHeap();
        memory["min_free_heap"] = ESP.getMinFreeHeap();
        memory["heap_size"] = ESP.getHeapSize();
        memory["usage_percent"] = serialized(
            String(100.0f * (1.0f - (float)ESP.getFreeHeap() / ESP.getHeapSize()), 1));
        if (ESP.getPsramSize() > 0) {
            memory["psram_free"] = ESP.getFreePsram();
            memory["psram_size"] = ESP.getPsramSize();
        }

        // Network
        JsonObject network = doc["network"].to<JsonObject>();
        network["ip"] = WiFi.localIP().toString();
        network["rssi"] = WiFi.RSSI();
        network["ssid"] = WiFi.SSID();
        network["mac"] = WiFi.macAddress();
#endif

        // Rate limiter stats
        if (server.rateLimiter().isEnabled()) {
            JsonObject rl = doc["rate_limiter"].to<JsonObject>();
            rl["requests_per_second"] = server.rateLimiter().requestsPerSecond();
            rl["burst_capacity"] = (int)server.rateLimiter().burstCapacity();
            rl["total_allowed"] = (long)server.rateLimiter().totalAllowed();
            rl["total_denied"] = (long)server.rateLimiter().totalDenied();
        }

        // Session info
        JsonObject sessions = doc["sessions"].to<JsonObject>();
        sessions["summary"] = serialized(server.sessions().summary());

        // Heap monitor warning
        if (server.heap().isLow()) {
            doc["warning"] = "Low memory!";
        }

        String out;
        serializeJson(doc, out);
        return out;
    };

    server.addTool(diagTool);
}

} // namespace mcpd

#endif // MCPD_DIAGNOSTICS_H
