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
 * Represents a single MCP resource.
 */
struct MCPResource {
    String uri;
    String name;
    String description;
    String mimeType;
    MCPResourceHandler handler;

    MCPResource() = default;

    MCPResource(const char* uri, const char* name,
                const char* description, const char* mimeType,
                MCPResourceHandler handler)
        : uri(uri), name(name), description(description),
          mimeType(mimeType), handler(handler) {}

    /**
     * Serialize for resources/list response.
     */
    void toJson(JsonObject& obj) const {
        obj["uri"] = uri;
        obj["name"] = name;
        obj["description"] = description;
        obj["mimeType"] = mimeType;
    }
};

} // namespace mcpd

#endif // MCPD_RESOURCE_H
