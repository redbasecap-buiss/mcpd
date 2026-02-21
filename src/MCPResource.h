/**
 * mcpd — MCP Resource definition
 */

#ifndef MCPD_RESOURCE_H
#define MCPD_RESOURCE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>

#include "MCPIcon.h"

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
    String audience;      // "user", "assistant", or empty (both)
    float priority = -1;  // 0.0 to 1.0, -1 = unset
    String lastModified;  // ISO 8601 timestamp (MCP 2025-11-25)
    bool hasAnnotations = false;

    MCPResourceAnnotations& setAudience(const char* a) { audience = a; hasAnnotations = true; return *this; }
    MCPResourceAnnotations& setPriority(float p) { priority = p; hasAnnotations = true; return *this; }
    MCPResourceAnnotations& setLastModified(const char* ts) { lastModified = ts; hasAnnotations = true; return *this; }

    void toJson(JsonObject& obj) const {
        if (!audience.isEmpty()) {
            JsonArray arr = obj["audience"].to<JsonArray>();
            arr.add(audience);
        }
        if (priority >= 0.0f) obj["priority"] = priority;
        if (!lastModified.isEmpty()) obj["lastModified"] = lastModified;
    }
};

/**
 * Represents a single MCP resource.
 */
struct MCPResource {
    String uri;
    String name;
    String title;           // Optional: human-readable display name (MCP 2025-11-25)
    String description;
    String mimeType;
    int size = -1;          // Optional: raw content size in bytes (MCP 2025-11-25), -1 = unset
    MCPResourceHandler handler;
    MCPResourceAnnotations annotations;
    std::vector<MCPIcon> icons; // Optional: icons for UI display (MCP 2025-11-25)

    MCPResource() = default;

    MCPResource(const char* uri, const char* name,
                const char* description, const char* mimeType,
                MCPResourceHandler handler)
        : uri(uri), name(name), description(description),
          mimeType(mimeType), handler(handler) {}

    /** Set display title (MCP 2025-11-25) */
    MCPResource& setTitle(const char* t) { title = t; return *this; }

    /** Set resource size in bytes (MCP 2025-11-25) */
    MCPResource& setSize(int s) { size = s; return *this; }

    /** Add an icon (MCP 2025-11-25) */
    MCPResource& addIcon(const MCPIcon& icon) { icons.push_back(icon); return *this; }

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
        if (!title.isEmpty()) obj["title"] = title;
        obj["description"] = description;
        obj["mimeType"] = mimeType;
        if (size >= 0) obj["size"] = size;
        if (annotations.hasAnnotations) {
            JsonObject ann = obj["annotations"].to<JsonObject>();
            annotations.toJson(ann);
        }
        iconsToJson(icons, obj);
    }
};

} // namespace mcpd

#endif // MCPD_RESOURCE_H
