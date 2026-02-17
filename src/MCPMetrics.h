/**
 * mcpd — Prometheus Metrics
 *
 * Exposes a /metrics endpoint in Prometheus exposition format.
 * Tracks request count, latency, uptime, free heap, and SSE connections.
 *
 * Usage:
 *   mcpd::Metrics metrics;
 *   metrics.begin(server);  // pass your WebServer
 *   // On each request:
 *   metrics.recordRequest(method, durationMs);
 */

#ifndef MCPD_METRICS_H
#define MCPD_METRICS_H

#include <Arduino.h>
#include <WebServer.h>
#include <map>

namespace mcpd {

class Metrics {
public:
    Metrics() : _startTime(0) {}

    /**
     * Register the /metrics endpoint on the given WebServer.
     */
    void begin(WebServer& server) {
        _startTime = millis();
        server.on("/metrics", HTTP_GET, [this, &server]() {
            server.send(200, "text/plain; version=0.0.4; charset=utf-8",
                        _render());
        });
        Serial.println("[mcpd] Prometheus metrics enabled at /metrics");
    }

    /**
     * Record a completed request.
     * @param method  JSON-RPC method name (e.g., "tools/call")
     * @param durationMs  Request processing time in milliseconds
     */
    void recordRequest(const String& method, unsigned long durationMs) {
        _totalRequests++;
        _methodCounts[method]++;
        _totalLatencyMs += durationMs;

        // Track max latency
        if (durationMs > _maxLatencyMs) {
            _maxLatencyMs = durationMs;
        }
    }

    /** Record an error */
    void recordError() { _totalErrors++; }

    /** Set current SSE client count (called by server) */
    void setSSEClients(size_t count) { _sseClients = count; }

    // Getters
    unsigned long totalRequests() const { return _totalRequests; }
    unsigned long totalErrors() const { return _totalErrors; }
    unsigned long uptimeSeconds() const { return (millis() - _startTime) / 1000; }

private:
    unsigned long _startTime;
    unsigned long _totalRequests = 0;
    unsigned long _totalErrors = 0;
    unsigned long _totalLatencyMs = 0;
    unsigned long _maxLatencyMs = 0;
    size_t _sseClients = 0;
    std::map<String, unsigned long> _methodCounts;

    String _render() {
        String out;
        out.reserve(1024);

        // Uptime
        out += "# HELP mcpd_uptime_seconds Time since server start\n";
        out += "# TYPE mcpd_uptime_seconds gauge\n";
        out += "mcpd_uptime_seconds " + String(uptimeSeconds()) + "\n\n";

        // Free heap
        out += "# HELP mcpd_free_heap_bytes Free heap memory in bytes\n";
        out += "# TYPE mcpd_free_heap_bytes gauge\n";
        out += "mcpd_free_heap_bytes " + String(ESP.getFreeHeap()) + "\n\n";

        // Min free heap (since boot)
        out += "# HELP mcpd_min_free_heap_bytes Minimum free heap since boot\n";
        out += "# TYPE mcpd_min_free_heap_bytes gauge\n";
        out += "mcpd_min_free_heap_bytes " + String(ESP.getMinFreeHeap()) + "\n\n";

        // Total requests
        out += "# HELP mcpd_requests_total Total JSON-RPC requests processed\n";
        out += "# TYPE mcpd_requests_total counter\n";
        out += "mcpd_requests_total " + String(_totalRequests) + "\n\n";

        // Requests by method
        if (!_methodCounts.empty()) {
            out += "# HELP mcpd_requests_by_method_total Requests by JSON-RPC method\n";
            out += "# TYPE mcpd_requests_by_method_total counter\n";
            for (const auto& kv : _methodCounts) {
                out += "mcpd_requests_by_method_total{method=\"" + kv.first + "\"} "
                       + String(kv.second) + "\n";
            }
            out += "\n";
        }

        // Errors
        out += "# HELP mcpd_errors_total Total error responses\n";
        out += "# TYPE mcpd_errors_total counter\n";
        out += "mcpd_errors_total " + String(_totalErrors) + "\n\n";

        // Latency
        out += "# HELP mcpd_request_latency_ms_avg Average request latency in ms\n";
        out += "# TYPE mcpd_request_latency_ms_avg gauge\n";
        unsigned long avgLatency = _totalRequests > 0
                                       ? _totalLatencyMs / _totalRequests : 0;
        out += "mcpd_request_latency_ms_avg " + String(avgLatency) + "\n\n";

        out += "# HELP mcpd_request_latency_ms_max Maximum request latency in ms\n";
        out += "# TYPE mcpd_request_latency_ms_max gauge\n";
        out += "mcpd_request_latency_ms_max " + String(_maxLatencyMs) + "\n\n";

        // SSE clients
        out += "# HELP mcpd_sse_clients Active SSE connections\n";
        out += "# TYPE mcpd_sse_clients gauge\n";
        out += "mcpd_sse_clients " + String(_sseClients) + "\n\n";

        // WiFi RSSI
        out += "# HELP mcpd_wifi_rssi_dbm WiFi signal strength\n";
        out += "# TYPE mcpd_wifi_rssi_dbm gauge\n";
        out += "mcpd_wifi_rssi_dbm " + String(WiFi.RSSI()) + "\n";

        return out;
    }
};

} // namespace mcpd

#endif // MCPD_METRICS_H
