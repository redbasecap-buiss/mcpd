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
 * Represents a single MCP tool.
 */
struct MCPTool {
    String name;
    String description;
    String inputSchemaJson;  // JSON Schema as string
    MCPToolHandler handler;

    MCPTool() = default;

    MCPTool(const char* name, const char* description,
            const char* inputSchemaJson, MCPToolHandler handler)
        : name(name), description(description),
          inputSchemaJson(inputSchemaJson), handler(handler) {}

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
    }
};

} // namespace mcpd

#endif // MCPD_TOOL_H
