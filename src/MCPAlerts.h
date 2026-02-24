/**
 * mcpd — Alerts (Threshold-Based Alerting System)
 *
 * Lightweight alerting engine for microcontrollers. Define alert rules
 * with numeric thresholds, and check values against them. When a threshold
 * is crossed, the alert fires and invokes registered callbacks. Supports
 * hysteresis to prevent flapping, cooldown periods, severity levels,
 * and JSON serialization of alert state.
 *
 * Designed to pair with EventStore (for logging alerts) and StateStore
 * (for monitoring state values) in a complete sensor monitoring pipeline.
 *
 * Features:
 *   - Named alert rules with configurable thresholds
 *   - Comparison operators: >, >=, <, <=, ==, !=, outside_range, inside_range
 *   - Hysteresis (deadband) to prevent oscillation at boundaries
 *   - Cooldown period (minimum interval between repeated firings)
 *   - Per-alert severity levels (debug → critical)
 *   - Enable/disable individual alerts at runtime
 *   - Global alert listener callbacks
 *   - Per-alert custom callbacks
 *   - Alert state tracking (active, cleared, acknowledged)
 *   - Fire count and timing statistics
 *   - Bounded number of alert rules
 *   - JSON serialization of all alert definitions and state
 *
 * Usage:
 *   mcpd::AlertEngine alerts(32);
 *
 *   alerts.addAlert("temp_high", mcpd::AlertOp::GreaterThan, 40.0);
 *   alerts.addAlert("battery_low", mcpd::AlertOp::LessThan, 20.0,
 *                   mcpd::AlertSeverity::Warning);
 *   alerts.addAlert("pressure_range", mcpd::AlertOp::OutsideRange,
 *                   950.0, 1050.0);
 *
 *   alerts.setHysteresis("temp_high", 2.0);   // 2-degree deadband
 *   alerts.setCooldown("temp_high", 60000);    // min 60s between firings
 *
 *   alerts.onAlert([](const mcpd::AlertEvent& e) {
 *       Serial.printf("ALERT %s: value=%.1f\n", e.name, e.value);
 *   });
 *
 *   // In loop:
 *   alerts.check("temp_high", currentTemp);
 *   alerts.check("battery_low", batteryPercent);
 *   alerts.check("pressure_range", pressure);
 *
 * MIT License — Nicola Spieser
 */

#ifndef MCPD_ALERTS_H
#define MCPD_ALERTS_H

#ifdef MCPD_TEST
#include <Arduino.h>
#else
#include <Arduino.h>
#endif

#include <vector>
#include <functional>
#include <cstring>
#include <cmath>
#include <string>

namespace mcpd {

// ─── Enums ──────────────────────────────────────────────────────────────

enum class AlertOp : uint8_t {
    GreaterThan = 0,
    GreaterEqual,
    LessThan,
    LessEqual,
    Equal,
    NotEqual,
    OutsideRange,   ///< value < low OR value > high
    InsideRange      ///< low <= value <= high
};

inline const char* alertOpToString(AlertOp op) {
    switch (op) {
        case AlertOp::GreaterThan:  return ">";
        case AlertOp::GreaterEqual: return ">=";
        case AlertOp::LessThan:    return "<";
        case AlertOp::LessEqual:   return "<=";
        case AlertOp::Equal:       return "==";
        case AlertOp::NotEqual:    return "!=";
        case AlertOp::OutsideRange: return "outside_range";
        case AlertOp::InsideRange:  return "inside_range";
        default:                    return "?";
    }
}

inline AlertOp alertOpFromString(const char* s) {
    if (!s) return AlertOp::GreaterThan;
    if (strcmp(s, ">") == 0)              return AlertOp::GreaterThan;
    if (strcmp(s, ">=") == 0)             return AlertOp::GreaterEqual;
    if (strcmp(s, "<") == 0)              return AlertOp::LessThan;
    if (strcmp(s, "<=") == 0)             return AlertOp::LessEqual;
    if (strcmp(s, "==") == 0)             return AlertOp::Equal;
    if (strcmp(s, "!=") == 0)             return AlertOp::NotEqual;
    if (strcmp(s, "outside_range") == 0)  return AlertOp::OutsideRange;
    if (strcmp(s, "inside_range") == 0)   return AlertOp::InsideRange;
    return AlertOp::GreaterThan;
}

enum class AlertSeverity : uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

inline const char* alertSeverityToString(AlertSeverity s) {
    switch (s) {
        case AlertSeverity::Debug:    return "debug";
        case AlertSeverity::Info:     return "info";
        case AlertSeverity::Warning:  return "warning";
        case AlertSeverity::Error:    return "error";
        case AlertSeverity::Critical: return "critical";
        default:                      return "unknown";
    }
}

inline AlertSeverity alertSeverityFromString(const char* s) {
    if (!s) return AlertSeverity::Info;
    if (strcmp(s, "debug") == 0)    return AlertSeverity::Debug;
    if (strcmp(s, "info") == 0)     return AlertSeverity::Info;
    if (strcmp(s, "warning") == 0)  return AlertSeverity::Warning;
    if (strcmp(s, "error") == 0)    return AlertSeverity::Error;
    if (strcmp(s, "critical") == 0) return AlertSeverity::Critical;
    return AlertSeverity::Info;
}

enum class AlertState : uint8_t {
    Clear = 0,       ///< Not triggered
    Active = 1,      ///< Threshold crossed, alert firing
    Acknowledged = 2 ///< Active but user acknowledged
};

inline const char* alertStateToString(AlertState s) {
    switch (s) {
        case AlertState::Clear:        return "clear";
        case AlertState::Active:       return "active";
        case AlertState::Acknowledged: return "acknowledged";
        default:                       return "unknown";
    }
}

// ─── Structs ────────────────────────────────────────────────────────────

/**
 * Alert event passed to listener callbacks when an alert fires or clears.
 */
struct AlertEvent {
    const char* name;           ///< Alert rule name
    double value;               ///< The value that triggered it
    double threshold;           ///< Primary threshold
    double thresholdHigh;       ///< Secondary threshold (for range ops)
    AlertOp op;                 ///< Comparison operator
    AlertSeverity severity;     ///< Severity level
    bool fired;                 ///< true = alert fired, false = alert cleared
    unsigned long timestampMs;  ///< millis() when this event occurred
};

using AlertListener = std::function<void(const AlertEvent&)>;
using AlertCallback = std::function<void(const AlertEvent&)>;

/**
 * Internal alert rule definition + state.
 */
struct AlertRule {
    String name;
    AlertOp op = AlertOp::GreaterThan;
    double threshold = 0.0;
    double thresholdHigh = 0.0;   ///< For range operators
    double hysteresis = 0.0;      ///< Deadband
    unsigned long cooldownMs = 0; ///< Min interval between firings
    AlertSeverity severity = AlertSeverity::Warning;
    bool enabled = true;

    // State
    AlertState state = AlertState::Clear;
    double lastValue = 0.0;
    unsigned long lastFiredMs = 0;
    unsigned long lastClearedMs = 0;
    uint32_t fireCount = 0;
    uint32_t checkCount = 0;
    AlertCallback callback;       ///< Optional per-alert callback
};

// ─── AlertEngine ────────────────────────────────────────────────────────

class AlertEngine {
public:
    explicit AlertEngine(size_t maxAlerts = 32) : _maxAlerts(maxAlerts) {
        _rules.reserve(std::min(maxAlerts, (size_t)16));
    }

    // ── Rule Management ─────────────────────────────────────────────

    /**
     * Add an alert rule (single threshold).
     * Returns true on success, false if capacity reached or name duplicate.
     */
    bool addAlert(const char* name, AlertOp op, double threshold,
                  AlertSeverity severity = AlertSeverity::Warning) {
        if (!name || strlen(name) == 0) return false;
        if (_rules.size() >= _maxAlerts) return false;
        if (findRule(name) != nullptr) return false;

        AlertRule rule;
        rule.name = name;
        rule.op = op;
        rule.threshold = threshold;
        rule.severity = severity;
        _rules.push_back(std::move(rule));
        return true;
    }

    /**
     * Add a range-based alert rule (two thresholds).
     */
    bool addAlert(const char* name, AlertOp op, double low, double high,
                  AlertSeverity severity = AlertSeverity::Warning) {
        if (!name || strlen(name) == 0) return false;
        if (_rules.size() >= _maxAlerts) return false;
        if (findRule(name) != nullptr) return false;
        if (op != AlertOp::OutsideRange && op != AlertOp::InsideRange) return false;

        AlertRule rule;
        rule.name = name;
        rule.op = op;
        rule.threshold = low;
        rule.thresholdHigh = high;
        rule.severity = severity;
        _rules.push_back(std::move(rule));
        return true;
    }

    /**
     * Remove an alert rule by name. Returns true if found and removed.
     */
    bool removeAlert(const char* name) {
        for (auto it = _rules.begin(); it != _rules.end(); ++it) {
            if (it->name == name) {
                _rules.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * Remove all alert rules.
     */
    void clearAlerts() {
        _rules.clear();
    }

    // ── Configuration ───────────────────────────────────────────────

    bool setHysteresis(const char* name, double hysteresis) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->hysteresis = std::abs(hysteresis);
        return true;
    }

    bool setCooldown(const char* name, unsigned long cooldownMs) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->cooldownMs = cooldownMs;
        return true;
    }

    bool setSeverity(const char* name, AlertSeverity severity) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->severity = severity;
        return true;
    }

    bool setEnabled(const char* name, bool enabled) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->enabled = enabled;
        return true;
    }

    bool setCallback(const char* name, AlertCallback cb) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->callback = std::move(cb);
        return true;
    }

    // ── Listeners ───────────────────────────────────────────────────

    /**
     * Register a global alert listener (called for all alert events).
     */
    void onAlert(AlertListener listener) {
        _listeners.push_back(std::move(listener));
    }

    /**
     * Remove all global listeners.
     */
    void clearListeners() {
        _listeners.clear();
    }

    // ── Checking ────────────────────────────────────────────────────

    /**
     * Check a value against a named alert rule.
     * Returns true if the alert is currently active after this check.
     */
    bool check(const char* name, double value) {
        AlertRule* r = findRule(name);
        if (!r || !r->enabled) return false;

        r->checkCount++;
        r->lastValue = value;
        unsigned long now = millis();

        bool conditionMet = evaluateCondition(*r, value);

        if (conditionMet && r->state == AlertState::Clear) {
            // Check cooldown
            if (r->cooldownMs > 0 && r->lastFiredMs > 0 &&
                (now - r->lastFiredMs) < r->cooldownMs) {
                return false;
            }
            // Fire!
            r->state = AlertState::Active;
            r->lastFiredMs = now;
            r->fireCount++;
            notifyListeners(*r, value, true, now);
            return true;
        }
        else if (!conditionMet && r->state != AlertState::Clear) {
            // Check hysteresis for clearing
            if (r->hysteresis > 0.0 && !evaluateClearWithHysteresis(*r, value)) {
                return (r->state == AlertState::Active);
            }
            // Clear
            AlertState prevState = r->state;
            r->state = AlertState::Clear;
            r->lastClearedMs = now;
            if (prevState == AlertState::Active || prevState == AlertState::Acknowledged) {
                notifyListeners(*r, value, false, now);
            }
            return false;
        }

        return (r->state == AlertState::Active);
    }

    /**
     * Check a value against all alerts with a matching tag prefix.
     * Useful for checking all "temp_*" alerts at once.
     * Returns the number of alerts that are currently active.
     */
    int checkAll(const char* prefix, double value) {
        int active = 0;
        for (auto& r : _rules) {
            if (prefix == nullptr || strlen(prefix) == 0 ||
                strncmp(r.name.c_str(), prefix, strlen(prefix)) == 0) {
                if (check(r.name.c_str(), value)) active++;
            }
        }
        return active;
    }

    // ── Acknowledgment ──────────────────────────────────────────────

    /**
     * Acknowledge an active alert (suppresses repeated notifications
     * until it clears and re-fires).
     */
    bool acknowledge(const char* name) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        if (r->state != AlertState::Active) return false;
        r->state = AlertState::Acknowledged;
        return true;
    }

    /**
     * Manually reset an alert to clear state.
     */
    bool reset(const char* name) {
        AlertRule* r = findRule(name);
        if (!r) return false;
        r->state = AlertState::Clear;
        return true;
    }

    /**
     * Reset all alerts to clear state.
     */
    void resetAll() {
        for (auto& r : _rules) {
            r.state = AlertState::Clear;
        }
    }

    // ── Queries ─────────────────────────────────────────────────────

    size_t count() const { return _rules.size(); }
    size_t capacity() const { return _maxAlerts; }

    /**
     * Get number of currently active (not cleared) alerts.
     */
    size_t activeCount() const {
        size_t n = 0;
        for (const auto& r : _rules) {
            if (r.state != AlertState::Clear) n++;
        }
        return n;
    }

    /**
     * Get names of all active alerts.
     */
    std::vector<String> activeAlerts() const {
        std::vector<String> result;
        for (const auto& r : _rules) {
            if (r.state != AlertState::Clear) {
                result.push_back(r.name);
            }
        }
        return result;
    }

    /**
     * Check if a specific alert exists.
     */
    bool hasAlert(const char* name) const {
        return findRuleConst(name) != nullptr;
    }

    /**
     * Get the state of a specific alert.
     */
    AlertState getState(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->state : AlertState::Clear;
    }

    /**
     * Get the fire count of a specific alert.
     */
    uint32_t getFireCount(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->fireCount : 0;
    }

    /**
     * Get the check count of a specific alert.
     */
    uint32_t getCheckCount(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->checkCount : 0;
    }

    /**
     * Get the last value checked for a specific alert.
     */
    double getLastValue(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->lastValue : 0.0;
    }

    /**
     * Get the last fired timestamp for a specific alert.
     */
    unsigned long getLastFiredMs(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->lastFiredMs : 0;
    }

    /**
     * Check if a specific alert is enabled.
     */
    bool isEnabled(const char* name) const {
        const AlertRule* r = findRuleConst(name);
        return r ? r->enabled : false;
    }

    // ── JSON Serialization ──────────────────────────────────────────

    /**
     * Serialize all alert definitions and state as a JSON array.
     */
    String toJSON() const {
        String json = "[";
        bool first = true;
        for (const auto& r : _rules) {
            if (!first) json += ",";
            first = false;
            json += "{";
            json += "\"name\":\"" + escapeJSON(r.name) + "\"";
            json += ",\"op\":\""; json += alertOpToString(r.op); json += "\"";
            json += ",\"threshold\":"; json += String(r.threshold, 6);
            if (r.op == AlertOp::OutsideRange || r.op == AlertOp::InsideRange) {
                json += ",\"thresholdHigh\":"; json += String(r.thresholdHigh, 6);
            }
            json += ",\"hysteresis\":"; json += String(r.hysteresis, 6);
            json += ",\"cooldownMs\":"; json += String((unsigned long)r.cooldownMs);
            json += ",\"severity\":\""; json += alertSeverityToString(r.severity); json += "\"";
            json += ",\"enabled\":"; json += r.enabled ? "true" : "false";
            json += ",\"state\":\""; json += alertStateToString(r.state); json += "\"";
            json += ",\"lastValue\":"; json += String(r.lastValue, 6);
            json += ",\"fireCount\":"; json += String(r.fireCount);
            json += ",\"checkCount\":"; json += String(r.checkCount);
            json += ",\"lastFiredMs\":"; json += String(r.lastFiredMs);
            json += ",\"lastClearedMs\":"; json += String(r.lastClearedMs);
            json += "}";
        }
        json += "]";
        return json;
    }

    /**
     * Serialize only active alerts as JSON array.
     */
    String activeToJSON() const {
        String json = "[";
        bool first = true;
        for (const auto& r : _rules) {
            if (r.state == AlertState::Clear) continue;
            if (!first) json += ",";
            first = false;
            json += "{";
            json += "\"name\":\"" + escapeJSON(r.name) + "\"";
            json += ",\"state\":\""; json += alertStateToString(r.state); json += "\"";
            json += ",\"severity\":\""; json += alertSeverityToString(r.severity); json += "\"";
            json += ",\"lastValue\":"; json += String(r.lastValue, 6);
            json += ",\"fireCount\":"; json += String(r.fireCount);
            json += ",\"lastFiredMs\":"; json += String(r.lastFiredMs);
            json += "}";
        }
        json += "]";
        return json;
    }

    /**
     * Get a summary string suitable for logging/display.
     */
    String summary() const {
        String s;
        s += "Alerts: ";
        s += String((unsigned long)_rules.size());
        s += " rules, ";
        s += String((unsigned long)activeCount());
        s += " active";
        return s;
    }

private:
    size_t _maxAlerts;
    std::vector<AlertRule> _rules;
    std::vector<AlertListener> _listeners;

    AlertRule* findRule(const char* name) {
        for (auto& r : _rules) {
            if (r.name == name) return &r;
        }
        return nullptr;
    }

    const AlertRule* findRuleConst(const char* name) const {
        for (const auto& r : _rules) {
            if (r.name == name) return &r;
        }
        return nullptr;
    }

    /**
     * Evaluate whether the condition is met (without hysteresis).
     */
    bool evaluateCondition(const AlertRule& r, double value) const {
        switch (r.op) {
            case AlertOp::GreaterThan:  return value > r.threshold;
            case AlertOp::GreaterEqual: return value >= r.threshold;
            case AlertOp::LessThan:    return value < r.threshold;
            case AlertOp::LessEqual:   return value <= r.threshold;
            case AlertOp::Equal:       return std::abs(value - r.threshold) < 1e-9;
            case AlertOp::NotEqual:    return std::abs(value - r.threshold) >= 1e-9;
            case AlertOp::OutsideRange:
                return value < r.threshold || value > r.thresholdHigh;
            case AlertOp::InsideRange:
                return value >= r.threshold && value <= r.thresholdHigh;
            default: return false;
        }
    }

    /**
     * For clearing: check if value has moved far enough past threshold
     * (by hysteresis amount) to warrant clearing.
     * Returns true if the alert SHOULD be cleared (value is safely past threshold).
     */
    bool evaluateClearWithHysteresis(const AlertRule& r, double value) const {
        double h = r.hysteresis;
        switch (r.op) {
            case AlertOp::GreaterThan:
            case AlertOp::GreaterEqual:
                return value < (r.threshold - h);
            case AlertOp::LessThan:
            case AlertOp::LessEqual:
                return value > (r.threshold + h);
            case AlertOp::Equal:
                return std::abs(value - r.threshold) > h;
            case AlertOp::NotEqual:
                return std::abs(value - r.threshold) < 1e-9;
            case AlertOp::OutsideRange:
                // Clear when value is safely inside range
                return value >= (r.threshold + h) && value <= (r.thresholdHigh - h);
            case AlertOp::InsideRange:
                // Clear when value is safely outside range
                return value < (r.threshold - h) || value > (r.thresholdHigh + h);
            default: return true;
        }
    }

    void notifyListeners(const AlertRule& r, double value, bool fired, unsigned long now) {
        AlertEvent evt;
        evt.name = r.name.c_str();
        evt.value = value;
        evt.threshold = r.threshold;
        evt.thresholdHigh = r.thresholdHigh;
        evt.op = r.op;
        evt.severity = r.severity;
        evt.fired = fired;
        evt.timestampMs = now;

        // Per-alert callback
        if (r.callback) r.callback(evt);

        // Global listeners
        for (auto& listener : _listeners) {
            listener(evt);
        }
    }

    static String escapeJSON(const String& s) {
        String result;
        result.reserve(s.length());
        for (size_t i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        return result;
    }
};

} // namespace mcpd

#endif // MCPD_ALERTS_H
