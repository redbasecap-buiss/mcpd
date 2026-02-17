/**
 * mcpd — Authentication middleware
 *
 * Optional Bearer Token / API Key authentication for the MCP server.
 * When enabled, all requests must include a valid token.
 *
 * Supports:
 *   - Bearer token in Authorization header
 *   - API key in X-API-Key header
 *   - API key as ?key= query parameter
 */

#ifndef MCPD_AUTH_H
#define MCPD_AUTH_H

#include <Arduino.h>
#include <WebServer.h>
#include <functional>
#include <vector>

namespace mcpd {

class Auth {
public:
    Auth() = default;

    /**
     * Enable authentication with a single API key/token.
     */
    void setApiKey(const char* key) {
        _keys.clear();
        _keys.push_back(String(key));
        _enabled = true;
    }

    /**
     * Add multiple valid API keys (e.g., for key rotation).
     */
    void addApiKey(const char* key) {
        _keys.push_back(String(key));
        _enabled = true;
    }

    /**
     * Set a custom authentication callback.
     * Return true if the request should be allowed.
     */
    void setAuthCallback(std::function<bool(const String& token)> cb) {
        _callback = cb;
        _enabled = true;
    }

    /**
     * Disable authentication.
     */
    void disable() {
        _enabled = false;
        _keys.clear();
        _callback = nullptr;
    }

    bool isEnabled() const { return _enabled; }

    /**
     * Check if a request is authenticated.
     * Extracts token from Authorization header, X-API-Key header, or query param.
     *
     * @param server  The WebServer instance to read headers from
     * @return true if authenticated (or auth is disabled)
     */
    bool authenticate(WebServer& server) const {
        if (!_enabled) return true;

        String token;

        // 1. Check Authorization: Bearer <token>
        String authHeader = server.header("Authorization");
        if (authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }

        // 2. Check X-API-Key header
        if (token.isEmpty()) {
            token = server.header("X-API-Key");
        }

        // 3. Check query parameter ?key=
        if (token.isEmpty()) {
            token = server.arg("key");
        }

        if (token.isEmpty()) return false;

        // Custom callback
        if (_callback) {
            return _callback(token);
        }

        // Check against stored keys
        for (const auto& key : _keys) {
            if (token == key) return true;
        }

        return false;
    }

    /**
     * Send a 401 Unauthorized response.
     */
    static void sendUnauthorized(WebServer& server) {
        server.sendHeader("WWW-Authenticate", "Bearer realm=\"mcpd\"");
        server.send(401, "application/json",
                    "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":"
                    "{\"code\":-32000,\"message\":\"Unauthorized: valid API key required\"}}");
    }

private:
    bool _enabled = false;
    std::vector<String> _keys;
    std::function<bool(const String& token)> _callback = nullptr;
};

} // namespace mcpd

#endif // MCPD_AUTH_H
