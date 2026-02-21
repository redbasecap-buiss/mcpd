/**
 * mcpd — MCP Resource definition
 */

#ifndef MCPD_RESOURCE_H
#define MCPD_RESOURCE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace mcpd {

/**
 * Resource handler function.
 * Returns the resource content as a string.
 */
using MCPResourceHandler = std::function<String()>;

/**
 * Resource annotations per MCP 2025-03-26 spec.
 * Describes the intended audience and priority of a resource.
 */
struct MCPResourceAnnotations {
    String audience;     // "user", "assistant", or empty (both)
    float priority = -1; // 0.0 to 1.0, -1 = unset
    bool hasAnnotations = false;

    MCPResourceAnnotations& setAudience(const char* a) { audience = a; hasAnnotations = true; return *this; }
    MCPResourceAnnotations& setPriority(float p) { priority = p; hasAnnotations = true; return *this; }

    void toJson(JsonObject& obj) const {
        if (!audience.isEmpty()) {
            JsonArray arr = obj["audience"].to<JsonArray>();
            arr.add(audience);
        }
        if (priority >= 0.0f) obj["priority"] = priority;
    }
};

/**
 * Represents a single MCP resource.
 */
struct MCPResource {
    String uri;
    String name;
    String description;
    String mimeType;
    MCPResourceHandler handler;
    MCPResourceAnnotations annotations;

    MCPResource() = default;

    MCPResource(const char* uri, const char* name,
                const char* description, const char* mimeType,
                MCPResourceHandler handler)
        : uri(uri), name(name), description(description),
          mimeType(mimeType), handler(handler) {}

    /** Builder-style: set annotations */
    MCPResource& annotate(const MCPResourceAnnotations& ann) {
        annotations = ann;
        annotations.hasAnnotations = true;
        return *this;
    }

    /** Convenience: set audience hint */
    MCPResource& setAudience(const char* audience) {
        annotations.setAudience(audience);
        return *this;
    }

    /** Convenience: set priority hint (0.0–1.0) */
    MCPResource& setPriority(float priority) {
        annotations.setPriority(priority);
        return *this;
    }

    /**
     * Serialize for resources/list response.
     */
    void toJson(JsonObject& obj) const {
        obj["uri"] = uri;
        obj["name"] = name;
        obj["description"] = description;
        obj["mimeType"] = mimeType;
        if (annotations.hasAnnotations) {
            JsonObject ann = obj["annotations"].to<JsonObject>();
            annotations.toJson(ann);
        }
    }
};

} // namespace mcpd

#endif // MCPD_RESOURCE_H
