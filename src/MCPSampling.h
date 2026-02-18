/**
 * mcpd — Sampling Support
 *
 * Implements MCP sampling capability: allows the server to request
 * LLM inference from the client. The client's model generates a response
 * based on the provided messages.
 *
 * This is a server-initiated request: the MCU asks the AI client
 * (e.g., Claude Desktop) to think about something and return a response.
 *
 * Usage:
 *   MCPSamplingRequest req;
 *   req.addUserMessage("What does this sensor reading mean: 42.5°C?");
 *   req.maxTokens = 200;
 *   server.requestSampling(req, [](const MCPSamplingResponse& resp) {
 *       Serial.println(resp.text);
 *   });
 */

#ifndef MCP_SAMPLING_H
#define MCP_SAMPLING_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>

namespace mcpd {

/**
 * Model preference hint for sampling requests.
 */
struct MCPModelHint {
    String name;     // e.g., "claude-3-haiku"

    void toJson(JsonObject& obj) const {
        if (name.length() > 0) obj["name"] = name;
    }
};

/**
 * Model preferences for sampling: priorities and hints.
 */
struct MCPModelPreferences {
    std::vector<MCPModelHint> hints;
    double costPriority = 0;          // 0-1, how much to prioritize cost
    double speedPriority = 0;         // 0-1, how much to prioritize speed
    double intelligencePriority = 0;  // 0-1, how much to prioritize intelligence

    void toJson(JsonObject& obj) const {
        if (!hints.empty()) {
            JsonArray arr = obj["hints"].to<JsonArray>();
            for (const auto& h : hints) {
                JsonObject ho = arr.add<JsonObject>();
                h.toJson(ho);
            }
        }
        if (costPriority > 0) obj["costPriority"] = costPriority;
        if (speedPriority > 0) obj["speedPriority"] = speedPriority;
        if (intelligencePriority > 0) obj["intelligencePriority"] = intelligencePriority;
    }
};

/**
 * A message in a sampling request.
 */
struct MCPSamplingMessage {
    String role;  // "user" or "assistant"
    String text;

    MCPSamplingMessage() = default;
    MCPSamplingMessage(const char* r, const char* t) : role(r), text(t) {}

    void toJson(JsonObject& obj) const {
        obj["role"] = role;
        JsonObject content = obj["content"].to<JsonObject>();
        content["type"] = "text";
        content["text"] = text;
    }
};

/**
 * A sampling request sent from server to client.
 */
struct MCPSamplingRequest {
    std::vector<MCPSamplingMessage> messages;
    MCPModelPreferences modelPreferences;
    String systemPrompt;
    String includeContext;  // "none", "thisServer", "allServers"
    int maxTokens = 256;
    float temperature = -1;  // -1 = not set
    std::vector<String> stopSequences;

    /** Add a user message. */
    void addUserMessage(const char* text) {
        messages.emplace_back("user", text);
    }

    /** Add an assistant message. */
    void addAssistantMessage(const char* text) {
        messages.emplace_back("assistant", text);
    }

    /** Serialize to JSON-RPC request params. */
    void toJson(JsonObject& obj) const {
        JsonArray msgs = obj["messages"].to<JsonArray>();
        for (const auto& m : messages) {
            JsonObject mo = msgs.add<JsonObject>();
            m.toJson(mo);
        }

        if (systemPrompt.length() > 0) {
            obj["systemPrompt"] = systemPrompt;
        }

        if (includeContext.length() > 0) {
            obj["includeContext"] = includeContext;
        }

        obj["maxTokens"] = maxTokens;

        if (temperature >= 0) {
            obj["temperature"] = temperature;
        }

        if (!stopSequences.empty()) {
            JsonArray stops = obj["stopSequences"].to<JsonArray>();
            for (const auto& s : stopSequences) {
                stops.add(s);
            }
        }

        if (!modelPreferences.hints.empty() ||
            modelPreferences.costPriority > 0 ||
            modelPreferences.speedPriority > 0 ||
            modelPreferences.intelligencePriority > 0) {
            JsonObject mp = obj["modelPreferences"].to<JsonObject>();
            modelPreferences.toJson(mp);
        }
    }

    /** Build a complete JSON-RPC request string. */
    String toJsonRpc(int requestId) const {
        JsonDocument doc;
        doc["jsonrpc"] = "2.0";
        doc["id"] = requestId;
        doc["method"] = "sampling/createMessage";
        JsonObject params = doc["params"].to<JsonObject>();
        toJson(params);
        String output;
        serializeJson(doc, output);
        return output;
    }
};

/**
 * Response from a sampling request.
 */
struct MCPSamplingResponse {
    String role;    // typically "assistant"
    String text;
    String model;   // model that was used
    String stopReason;  // "endTurn", "stopSequence", "maxTokens"
    bool valid = false;

    /** Parse from JSON-RPC result. */
    static MCPSamplingResponse fromJson(const JsonObject& result) {
        MCPSamplingResponse resp;
        resp.role = result["role"].as<const char*>() ?: "";
        resp.model = result["model"].as<const char*>() ?: "";
        resp.stopReason = result["stopReason"].as<const char*>() ?: "";

        JsonObject content = result["content"];
        if (!content.isNull()) {
            const char* type = content["type"].as<const char*>();
            if (type && strcmp(type, "text") == 0) {
                resp.text = content["text"].as<const char*>() ?: "";
            }
        }

        resp.valid = resp.text.length() > 0;
        return resp;
    }
};

/** Callback for sampling responses. */
using MCPSamplingCallback = std::function<void(const MCPSamplingResponse& response)>;

/**
 * Manages pending sampling requests.
 * Sampling is asynchronous: the server sends a request via SSE,
 * and the client responds via POST.
 */
class SamplingManager {
public:
    struct PendingRequest {
        int requestId;
        MCPSamplingCallback callback;
        unsigned long sentAt;
    };

    SamplingManager() = default;

    /** Queue a sampling request. Returns the request ID. */
    int queueRequest(const MCPSamplingRequest& request, MCPSamplingCallback callback) {
        int id = _nextId++;
        String jsonRpc = request.toJsonRpc(id);
        _pending.push_back({id, callback, millis()});
        _outgoing.push_back(jsonRpc);
        return id;
    }

    /** Get and clear pending outgoing messages (to send via SSE). */
    std::vector<String> drainOutgoing() {
        std::vector<String> out;
        std::swap(out, _outgoing);
        return out;
    }

    /** Handle a response from the client. Returns true if it matched a pending request. */
    bool handleResponse(int requestId, const JsonObject& result) {
        for (auto it = _pending.begin(); it != _pending.end(); ++it) {
            if (it->requestId == requestId) {
                MCPSamplingResponse resp = MCPSamplingResponse::fromJson(result);
                if (it->callback) {
                    it->callback(resp);
                }
                _pending.erase(it);
                return true;
            }
        }
        return false;
    }

    /** Check if there are pending requests. */
    bool hasPending() const { return !_pending.empty(); }

    /** Number of pending requests. */
    size_t pendingCount() const { return _pending.size(); }

    /** Timeout old requests (default: 60s). */
    void pruneExpired(unsigned long timeoutMs = 60000) {
        unsigned long now = millis();
        _pending.erase(
            std::remove_if(_pending.begin(), _pending.end(),
                [now, timeoutMs](const PendingRequest& p) {
                    return (now - p.sentAt) > timeoutMs;
                }),
            _pending.end()
        );
    }

private:
    std::vector<PendingRequest> _pending;
    std::vector<String> _outgoing;
    int _nextId = 9000;  // Start high to avoid conflicts with client request IDs
};

} // namespace mcpd

#endif // MCP_SAMPLING_H
