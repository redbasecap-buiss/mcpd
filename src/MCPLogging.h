/**
 * mcpd — Logging Support
 *
 * MCP logging capability: clients can set log level via logging/setLevel,
 * and the server sends log notifications via notifications/message.
 *
 * Log levels (RFC 5424 severity): debug, info, notice, warning, error,
 * critical, alert, emergency
 */

#ifndef MCP_LOGGING_H
#define MCP_LOGGING_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace mcpd {

enum class LogLevel : uint8_t {
    DEBUG     = 0,
    INFO      = 1,
    NOTICE    = 2,
    WARNING   = 3,
    ERROR     = 4,
    CRITICAL  = 5,
    ALERT     = 6,
    EMERGENCY = 7
};

/**
 * Convert LogLevel to its MCP string representation.
 */
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:     return "debug";
        case LogLevel::INFO:      return "info";
        case LogLevel::NOTICE:    return "notice";
        case LogLevel::WARNING:   return "warning";
        case LogLevel::ERROR:     return "error";
        case LogLevel::CRITICAL:  return "critical";
        case LogLevel::ALERT:     return "alert";
        case LogLevel::EMERGENCY: return "emergency";
        default:                  return "info";
    }
}

/**
 * Parse a log level string to LogLevel enum.
 * Returns LogLevel::INFO if unrecognized.
 */
inline LogLevel logLevelFromString(const char* str) {
    if (!str) return LogLevel::INFO;
    String s(str);
    if (s == "debug")     return LogLevel::DEBUG;
    if (s == "info")      return LogLevel::INFO;
    if (s == "notice")    return LogLevel::NOTICE;
    if (s == "warning")   return LogLevel::WARNING;
    if (s == "error")     return LogLevel::ERROR;
    if (s == "critical")  return LogLevel::CRITICAL;
    if (s == "alert")     return LogLevel::ALERT;
    if (s == "emergency") return LogLevel::EMERGENCY;
    return LogLevel::INFO;
}

/**
 * Callback type for delivering log notifications to the transport layer.
 * The server calls this with a serialized JSON-RPC notification string.
 */
using LogNotificationSink = std::function<void(const String& jsonNotification)>;

/**
 * Logging — manages log level and emits MCP log notifications.
 *
 * Usage:
 *   mcpd::Logging logging;
 *   logging.setSink([](const String& msg) { sendToClient(msg); });
 *   logging.log(mcpd::LogLevel::INFO, "sensor", "Temperature reading: 23.5°C");
 */
class Logging {
public:
    Logging() : _level(LogLevel::WARNING) {}

    /**
     * Set the minimum log level. Messages below this level are suppressed.
     * Called by the server when it receives logging/setLevel from the client.
     */
    void setLevel(LogLevel level) { _level = level; }
    LogLevel getLevel() const { return _level; }

    /**
     * Set the notification sink — how log messages are delivered to the client.
     */
    void setSink(LogNotificationSink sink) { _sink = std::move(sink); }

    /**
     * Emit a log message if it meets the current level threshold.
     *
     * @param level   Severity level
     * @param logger  Logger name (e.g. component name)
     * @param message The log message
     * @param data    Optional structured data (JSON string), or nullptr
     */
    void log(LogLevel level, const char* logger, const char* message,
             const char* data = nullptr) {
        // Suppress messages below current level
        if (static_cast<uint8_t>(level) < static_cast<uint8_t>(_level)) {
            return;
        }

        // Also print to Serial for local debugging
        Serial.printf("[mcpd:%s] %s: %s\n", logLevelToString(level), logger, message);

        // Send notification to client if sink is set
        if (_sink) {
            _sink(buildNotification(level, logger, message, data));
        }
    }

    // Convenience methods
    void debug(const char* logger, const char* msg)   { log(LogLevel::DEBUG, logger, msg); }
    void info(const char* logger, const char* msg)     { log(LogLevel::INFO, logger, msg); }
    void notice(const char* logger, const char* msg)   { log(LogLevel::NOTICE, logger, msg); }
    void warning(const char* logger, const char* msg)  { log(LogLevel::WARNING, logger, msg); }
    void error(const char* logger, const char* msg)    { log(LogLevel::ERROR, logger, msg); }
    void critical(const char* logger, const char* msg) { log(LogLevel::CRITICAL, logger, msg); }

private:
    LogLevel _level;
    LogNotificationSink _sink;

    /**
     * Build a JSON-RPC notification for notifications/message.
     */
    String buildNotification(LogLevel level, const char* logger,
                             const char* message, const char* data) {
        JsonDocument doc;
        doc["jsonrpc"] = "2.0";
        doc["method"] = "notifications/message";

        JsonObject params = doc["params"].to<JsonObject>();
        params["level"] = logLevelToString(level);
        params["logger"] = logger;
        params["data"] = message;

        // If structured data is provided, parse and embed it
        if (data) {
            JsonDocument dataDoc;
            DeserializationError err = deserializeJson(dataDoc, data);
            if (!err) {
                params["data"] = dataDoc.as<JsonVariant>();
            }
        }

        String output;
        serializeJson(doc, output);
        return output;
    }
};

} // namespace mcpd

#endif // MCP_LOGGING_H
