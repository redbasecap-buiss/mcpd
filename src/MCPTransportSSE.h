/**
 * mcpd — SSE Transport Layer
 *
 * Server-Sent Events transport for streaming MCP responses.
 * Implements the SSE portion of the MCP Streamable HTTP spec (2025-03-26).
 *
 * Usage:
 *   - Client sends GET to /mcp with Accept: text/event-stream
 *   - Server holds connection open, sends events as they occur
 *   - Client sends POST to /mcp with JSON-RPC, server can respond via SSE
 */

#ifndef MCPD_TRANSPORT_SSE_H
#define MCPD_TRANSPORT_SSE_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <functional>
#include <vector>

namespace mcpd {

/**
 * Represents an active SSE connection from a client.
 */
struct SSEClient {
    WiFiClient client;
    String sessionId;
    unsigned long connectedAt;
    unsigned long lastEventId = 0;

    bool isConnected() const {
        return client.connected();
    }

    /**
     * Send an SSE event to this client.
     * @param event  Event type (e.g., "message", "endpoint")
     * @param data   Event data (will be split on newlines)
     * @param id     Optional event ID
     */
    bool sendEvent(const char* event, const String& data, unsigned long id = 0) {
        if (!client.connected()) return false;

        String msg;
        if (id > 0) {
            msg += "id: " + String(id) + "\n";
            lastEventId = id;
        }
        if (event && strlen(event) > 0) {
            msg += "event: " + String(event) + "\n";
        }

        // Split data on newlines — each line gets its own "data:" prefix
        int start = 0;
        for (int i = 0; i <= (int)data.length(); i++) {
            if (i == (int)data.length() || data[i] == '\n') {
                msg += "data: " + data.substring(start, i) + "\n";
                start = i + 1;
            }
        }
        msg += "\n"; // Empty line terminates the event

        size_t written = client.print(msg);
        client.flush();
        return written == msg.length();
    }

    /**
     * Send a JSON-RPC response as an SSE message event.
     */
    bool sendJsonRpc(const String& jsonResponse, unsigned long eventId = 0) {
        return sendEvent("message", jsonResponse, eventId);
    }
};

/**
 * Manages SSE connections for the MCP server.
 */
class SSEManager {
public:
    static constexpr size_t MAX_SSE_CLIENTS = 4;
    static constexpr unsigned long KEEPALIVE_INTERVAL_MS = 30000;
    static constexpr unsigned long CONNECTION_TIMEOUT_MS = 300000; // 5 min

    SSEManager() = default;

    /**
     * Initialize a new SSE connection.
     * Sends HTTP headers and the initial endpoint event.
     *
     * @param client     The WiFiClient from the HTTP server
     * @param sessionId  The MCP session ID
     * @param endpoint   The MCP endpoint path (e.g., "/mcp")
     * @return true if connection was accepted
     */
    bool addClient(WiFiClient client, const String& sessionId, const char* endpoint) {
        // Clean up disconnected clients first
        pruneDisconnected();

        if (_clients.size() >= MAX_SSE_CLIENTS) {
            return false; // Too many connections
        }

        // Send SSE response headers directly
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/event-stream");
        client.println("Cache-Control: no-cache");
        client.println("Connection: keep-alive");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Mcp-Session-Id: " + sessionId);
        client.println();

        SSEClient sse;
        sse.client = client;
        sse.sessionId = sessionId;
        sse.connectedAt = millis();

        // Send the endpoint event per MCP spec
        sse.sendEvent("endpoint", String(endpoint));

        _clients.push_back(sse);
        Serial.printf("[mcpd] SSE client connected (total: %d)\n", _clients.size());
        return true;
    }

    /**
     * Broadcast a JSON-RPC response/notification to all SSE clients
     * with the matching session ID.
     */
    void broadcast(const String& sessionId, const String& jsonResponse) {
        _eventCounter++;
        for (auto& c : _clients) {
            if (c.sessionId == sessionId && c.isConnected()) {
                c.sendJsonRpc(jsonResponse, _eventCounter);
            }
        }
    }

    /**
     * Send to a specific session's SSE clients.
     */
    void sendToSession(const String& sessionId, const String& event,
                       const String& data) {
        _eventCounter++;
        for (auto& c : _clients) {
            if (c.sessionId == sessionId && c.isConnected()) {
                c.sendEvent(event.c_str(), data, _eventCounter);
            }
        }
    }

    /**
     * Call periodically to send keepalive comments and prune dead connections.
     */
    void loop() {
        unsigned long now = millis();
        if (now - _lastKeepalive >= KEEPALIVE_INTERVAL_MS) {
            _lastKeepalive = now;
            for (auto it = _clients.begin(); it != _clients.end();) {
                if (!it->isConnected() ||
                    (now - it->connectedAt > CONNECTION_TIMEOUT_MS)) {
                    it = _clients.erase(it);
                } else {
                    // Send SSE comment as keepalive
                    it->client.println(": keepalive\n");
                    it->client.flush();
                    ++it;
                }
            }
        }
    }

    /** Number of active SSE clients */
    size_t clientCount() const { return _clients.size(); }

    /** Check if any SSE clients are connected for a session */
    bool hasClients(const String& sessionId) const {
        for (const auto& c : _clients) {
            if (c.sessionId == sessionId && c.isConnected()) return true;
        }
        return false;
    }

private:
    std::vector<SSEClient> _clients;
    unsigned long _lastKeepalive = 0;
    unsigned long _eventCounter = 0;

    void pruneDisconnected() {
        _clients.erase(
            std::remove_if(_clients.begin(), _clients.end(),
                           [](const SSEClient& c) { return !c.isConnected(); }),
            _clients.end()
        );
    }
};

} // namespace mcpd

#endif // MCPD_TRANSPORT_SSE_H
