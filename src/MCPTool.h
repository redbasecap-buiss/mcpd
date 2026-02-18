/**
 * mcpd — MCP Tool definition
 */

#ifndef MCPD_TOOL_H
#define MCPD_TOOL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace mcpd {

/**
 * Tool handler function.
 * Receives the "arguments" object from tools/call.
 * Must return a JSON string that will be placed in content[0].text
 */
using MCPToolHandler = std::function<String(const JsonObject& arguments)>;

/**
 * Tool annotations per MCP 2025-03-26 spec.
 * Hints that describe tool behavior for clients/models.
 */
struct MCPToolAnnotations {
    String title;                // Human-readable title
    bool readOnlyHint = false;   // Tool only reads, no side effects
    bool destructiveHint = true; // Tool may perform destructive updates (default true per spec)
    bool idempotentHint = false; // Repeated calls with same args have no additional effect
    bool openWorldHint = true;   // Tool interacts with external entities (default true per spec)
    bool hasAnnotations = false; // Internal: whether annotations were explicitly set

    void toJson(JsonObject& obj) const {
        if (!title.isEmpty()) obj["title"] = title;
        obj["readOnlyHint"] = readOnlyHint;
        obj["destructiveHint"] = destructiveHint;
        obj["idempotentHint"] = idempotentHint;
        obj["openWorldHint"] = openWorldHint;
    }
};

/**
 * Represents a single MCP tool.
 */
struct MCPTool {
    String name;
    String description;
    String inputSchemaJson;  // JSON Schema as string
    MCPToolHandler handler;
    MCPToolAnnotations annotations;

    MCPTool() = default;

    MCPTool(const char* name, const char* description,
            const char* inputSchemaJson, MCPToolHandler handler)
        : name(name), description(description),
          inputSchemaJson(inputSchemaJson), handler(handler) {}

    /**
     * Set tool annotations (builder-style, returns reference).
     */
    MCPTool& setAnnotations(const MCPToolAnnotations& ann) {
        annotations = ann;
        annotations.hasAnnotations = true;
        return *this;
    }

    /** Convenience: mark as read-only (sets readOnlyHint=true, destructiveHint=false) */
    MCPTool& markReadOnly() {
        annotations.readOnlyHint = true;
        annotations.destructiveHint = false;
        annotations.hasAnnotations = true;
        return *this;
    }

    /** Convenience: mark as idempotent */
    MCPTool& markIdempotent() {
        annotations.idempotentHint = true;
        annotations.hasAnnotations = true;
        return *this;
    }

    /** Convenience: mark as local-only (openWorldHint=false) */
    MCPTool& markLocalOnly() {
        annotations.openWorldHint = false;
        annotations.hasAnnotations = true;
        return *this;
    }

    /**
     * Serialize this tool for tools/list response.
     * Writes into the provided JsonObject.
     */
    void toJson(JsonObject& obj) const {
        obj["name"] = name;
        obj["description"] = description;

        // Parse the input schema string into the JSON object
        JsonDocument schemaDoc;
        deserializeJson(schemaDoc, inputSchemaJson);
        obj["inputSchema"] = schemaDoc.as<JsonVariant>();

        // Include annotations if set
        if (annotations.hasAnnotations) {
            JsonObject ann = obj["annotations"].to<JsonObject>();
            annotations.toJson(ann);
        }
    }
};

} // namespace mcpd

#endif // MCPD_TOOL_H
