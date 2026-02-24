/**
 * MCPHealthCheck — Structured health monitoring for mcpd
 *
 * Reports component-level health status for MCU servers.
 * Register named health checks (WiFi, heap, sensors, transports)
 * with custom check functions. Query overall health or per-component.
 *
 * Usage:
 *   server.healthCheck().addCheck("wifi", []() -> HealthStatus {
 *       return WiFi.status() == WL_CONNECTED
 *           ? HealthStatus::healthy("Connected")
 *           : HealthStatus::unhealthy("Disconnected");
 *   });
 *   server.healthCheck().addCheck("heap", []() -> HealthStatus {
 *       size_t free = ESP.getFreeHeap();
 *       if (free > 50000) return HealthStatus::healthy(String(free) + " bytes free");
 *       if (free > 10000) return HealthStatus::degraded(String(free) + " bytes free");
 *       return HealthStatus::unhealthy(String(free) + " bytes free");
 *   });
 *   String json = server.healthCheck().toJSON();  // Full health report
 */

#ifndef MCP_HEALTH_CHECK_H
#define MCP_HEALTH_CHECK_H

#include <Arduino.h>
#include <functional>

namespace mcpd {

/**
 * Health status levels.
 */
enum class HealthLevel {
    Healthy,    ///< Component is functioning normally
    Degraded,   ///< Component is working but below optimal
    Unhealthy   ///< Component has failed or is non-functional
};

/**
 * Result of a single health check.
 */
struct HealthStatus {
    HealthLevel level;
    String message;
    unsigned long latencyMs;  ///< How long the check took

    static HealthStatus healthy(const String& msg = "") {
        return {HealthLevel::Healthy, msg, 0};
    }
    static HealthStatus degraded(const String& msg = "") {
        return {HealthLevel::Degraded, msg, 0};
    }
    static HealthStatus unhealthy(const String& msg = "") {
        return {HealthLevel::Unhealthy, msg, 0};
    }
};

/**
 * Health check function type.
 */
using HealthCheckFn = std::function<HealthStatus()>;

/**
 * Listener called when overall health changes.
 */
using HealthChangeListener = std::function<void(HealthLevel oldLevel, HealthLevel newLevel)>;

/**
 * Health check manager — register named checks, run them, get reports.
 *
 * Designed for MCU resource constraints:
 * - Bounded number of checks (configurable, default 16)
 * - O(n) run time where n = number of registered checks
 * - No dynamic allocation after registration
 * - Caching: results are cached and only re-evaluated after a configurable interval
 */
class HealthCheck {
public:
    static constexpr size_t DEFAULT_MAX_CHECKS = 16;
    static constexpr unsigned long DEFAULT_CACHE_MS = 5000;

    HealthCheck(size_t maxChecks = DEFAULT_MAX_CHECKS)
        : _maxChecks(maxChecks), _cacheMs(DEFAULT_CACHE_MS),
          _lastOverall(HealthLevel::Healthy), _lastRunMs(0),
          _listenerId(0), _checksRun(0), _enabled(true) {}

    /**
     * Register a named health check.
     * @param name   Unique name for this check (e.g., "wifi", "heap")
     * @param fn     Function that returns the current health status
     * @param critical  If true, unhealthy makes overall status unhealthy (default: true)
     * @return true if registered, false if full or duplicate name
     */
    bool addCheck(const String& name, HealthCheckFn fn, bool critical = true) {
        if (!fn) return false;
        if (_checks.size() >= _maxChecks) return false;

        // Duplicate check
        for (const auto& c : _checks) {
            if (c.name == name) return false;
        }

        CheckEntry entry;
        entry.name = name;
        entry.fn = fn;
        entry.critical = critical;
        entry.enabled = true;
        entry.lastResult = HealthStatus::healthy("Not yet checked");
        entry.lastCheckMs = 0;
        _checks.push_back(entry);
        return true;
    }

    /**
     * Remove a named health check.
     * @return true if found and removed
     */
    bool removeCheck(const String& name) {
        for (size_t i = 0; i < _checks.size(); i++) {
            if (_checks[i].name == name) {
                _checks.erase(_checks.begin() + i);
                return true;
            }
        }
        return false;
    }

    /**
     * Enable or disable a specific check without removing it.
     */
    bool setCheckEnabled(const String& name, bool enabled) {
        for (auto& c : _checks) {
            if (c.name == name) {
                c.enabled = enabled;
                return true;
            }
        }
        return false;
    }

    /**
     * Enable or disable the entire health check system.
     */
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }

    /**
     * Set cache duration. Results within this window are reused without re-running checks.
     * @param ms  Cache duration in milliseconds (0 = no caching)
     */
    void setCacheDuration(unsigned long ms) { _cacheMs = ms; }
    unsigned long cacheDuration() const { return _cacheMs; }

    /**
     * Number of registered checks.
     */
    size_t checkCount() const { return _checks.size(); }

    /**
     * Maximum number of checks.
     */
    size_t maxChecks() const { return _maxChecks; }

    /**
     * Check if a named check exists.
     */
    bool hasCheck(const String& name) const {
        for (const auto& c : _checks) {
            if (c.name == name) return true;
        }
        return false;
    }

    /**
     * Get the list of registered check names.
     */
    std::vector<String> checkNames() const {
        std::vector<String> names;
        names.reserve(_checks.size());
        for (const auto& c : _checks) {
            names.push_back(c.name);
        }
        return names;
    }

    /**
     * Run all enabled checks and return the overall health level.
     * Uses cached results if within the cache window.
     */
    HealthLevel run() {
        if (!_enabled) return HealthLevel::Healthy;

        unsigned long now = millis();

        // Use cached results if fresh enough
        if (_cacheMs > 0 && _lastRunMs > 0 && (now - _lastRunMs) < _cacheMs) {
            return _lastOverall;
        }

        HealthLevel overall = HealthLevel::Healthy;

        for (auto& c : _checks) {
            if (!c.enabled) continue;

            unsigned long start = millis();
            c.lastResult = c.fn();
            unsigned long end = millis();
            c.lastResult.latencyMs = end - start;
            c.lastCheckMs = now;

            if (c.critical) {
                if (c.lastResult.level == HealthLevel::Unhealthy) {
                    overall = HealthLevel::Unhealthy;
                } else if (c.lastResult.level == HealthLevel::Degraded &&
                           overall != HealthLevel::Unhealthy) {
                    overall = HealthLevel::Degraded;
                }
            }
        }

        _checksRun++;
        _lastRunMs = now;

        // Notify listeners if overall level changed
        if (overall != _lastOverall) {
            HealthLevel old = _lastOverall;
            _lastOverall = overall;
            for (const auto& l : _listeners) {
                if (l.fn) l.fn(old, overall);
            }
        } else {
            _lastOverall = overall;
        }

        return overall;
    }

    /**
     * Get the result of a specific check (from last run).
     */
    HealthStatus checkResult(const String& name) const {
        for (const auto& c : _checks) {
            if (c.name == name) return c.lastResult;
        }
        return HealthStatus::unhealthy("Check not found: " + name);
    }

    /**
     * Get the overall health level from the last run (without re-running).
     */
    HealthLevel overallHealth() const { return _lastOverall; }

    /**
     * Add a listener for overall health level changes.
     * @return Listener ID for removal
     */
    size_t onChange(HealthChangeListener fn) {
        if (!fn) return 0;
        size_t id = ++_listenerId;
        ListenerEntry entry;
        entry.id = id;
        entry.fn = fn;
        _listeners.push_back(entry);
        return id;
    }

    /**
     * Remove a listener by ID.
     */
    bool removeListener(size_t id) {
        for (size_t i = 0; i < _listeners.size(); i++) {
            if (_listeners[i].id == id) {
                _listeners.erase(_listeners.begin() + i);
                return true;
            }
        }
        return false;
    }

    /**
     * Total number of health check runs since creation.
     */
    unsigned long totalRuns() const { return _checksRun; }

    /**
     * Invalidate the cache, forcing the next run() to re-evaluate all checks.
     */
    void invalidate() { _lastRunMs = 0; }

    /**
     * Reset everything — removes all checks and listeners.
     */
    void reset() {
        _checks.clear();
        _listeners.clear();
        _lastOverall = HealthLevel::Healthy;
        _lastRunMs = 0;
        _checksRun = 0;
    }

    /**
     * Convert health level to string.
     */
    static const char* levelToString(HealthLevel level) {
        switch (level) {
            case HealthLevel::Healthy:   return "healthy";
            case HealthLevel::Degraded:  return "degraded";
            case HealthLevel::Unhealthy: return "unhealthy";
            default:                     return "unknown";
        }
    }

    /**
     * Serialize the full health report to JSON.
     *
     * Format:
     * {
     *   "status": "healthy|degraded|unhealthy",
     *   "checks": {
     *     "wifi": { "status": "healthy", "message": "Connected", "latencyMs": 2 },
     *     "heap": { "status": "degraded", "message": "12000 bytes free", "latencyMs": 0 }
     *   },
     *   "totalRuns": 42,
     *   "uptimeMs": 123456
     * }
     */
    String toJSON() {
        // Run checks if needed
        run();

        String json = "{\"status\":\"";
        json += levelToString(_lastOverall);
        json += "\",\"checks\":{";

        bool first = true;
        for (const auto& c : _checks) {
            if (!first) json += ",";
            first = false;
            json += "\"";
            json += c.name;
            json += "\":{\"status\":\"";
            json += levelToString(c.lastResult.level);
            json += "\",\"message\":\"";
            // Escape quotes in message
            String escaped = c.lastResult.message;
            escaped.replace("\"", "\\\"");
            json += escaped;
            json += "\",\"latencyMs\":";
            json += String(c.lastResult.latencyMs);
            json += ",\"critical\":";
            json += c.critical ? "true" : "false";
            json += ",\"enabled\":";
            json += c.enabled ? "true" : "false";
            json += "}";
        }

        json += "},\"totalRuns\":";
        json += String(_checksRun);
        json += ",\"uptimeMs\":";
        json += String(millis());
        json += "}";

        return json;
    }

    /**
     * Serialize stats summary to JSON.
     */
    String statsJSON() const {
        String json = "{\"status\":\"";
        json += levelToString(_lastOverall);
        json += "\",\"checkCount\":";
        json += String(_checks.size());
        json += ",\"totalRuns\":";
        json += String(_checksRun);
        json += ",\"cacheDurationMs\":";
        json += String(_cacheMs);
        json += ",\"enabled\":";
        json += _enabled ? "true" : "false";
        json += "}";
        return json;
    }

private:
    struct CheckEntry {
        String name;
        HealthCheckFn fn;
        bool critical;
        bool enabled;
        HealthStatus lastResult;
        unsigned long lastCheckMs;
    };

    struct ListenerEntry {
        size_t id;
        HealthChangeListener fn;
    };

    std::vector<CheckEntry> _checks;
    std::vector<ListenerEntry> _listeners;
    size_t _maxChecks;
    unsigned long _cacheMs;
    HealthLevel _lastOverall;
    unsigned long _lastRunMs;
    size_t _listenerId;
    unsigned long _checksRun;
    bool _enabled;
};

} // namespace mcpd

#endif // MCP_HEALTH_CHECK_H
