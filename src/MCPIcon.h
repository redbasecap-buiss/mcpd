/**
 * mcpd — MCP Icon definition
 *
 * Icons per MCP 2025-11-25 spec. Displayable in client UIs.
 * Supports PNG, JPEG, SVG, WebP via URI or data: URL.
 */

#ifndef MCPD_ICON_H
#define MCPD_ICON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

namespace mcpd {

/**
 * A single icon that can be displayed in a client UI.
 */
struct MCPIcon {
    String src;             // URI or data: URL
    String mimeType;        // Optional: "image/png", "image/svg+xml", etc.
    std::vector<String> sizes;  // Optional: "48x48", "96x96", "any"
    String theme;           // Optional: "light" or "dark"

    MCPIcon() = default;

    MCPIcon(const char* src) : src(src) {}

    MCPIcon(const char* src, const char* mimeType)
        : src(src), mimeType(mimeType) {}

    MCPIcon& setMimeType(const char* mt) { mimeType = mt; return *this; }
    MCPIcon& addSize(const char* s) { sizes.push_back(s); return *this; }
    MCPIcon& setTheme(const char* t) { theme = t; return *this; }

    void toJson(JsonObject& obj) const {
        obj["src"] = src;
        if (!mimeType.isEmpty()) obj["mimeType"] = mimeType;
        if (!sizes.empty()) {
            JsonArray arr = obj["sizes"].to<JsonArray>();
            for (const auto& s : sizes) arr.add(s);
        }
        if (!theme.isEmpty()) obj["theme"] = theme;
    }
};

/**
 * Serialize a vector of icons into a JsonObject parent.
 * Adds "icons" array if icons is non-empty.
 */
inline void iconsToJson(const std::vector<MCPIcon>& icons, JsonObject& parent) {
    if (icons.empty()) return;
    JsonArray arr = parent["icons"].to<JsonArray>();
    for (const auto& icon : icons) {
        JsonObject obj = arr.add<JsonObject>();
        icon.toJson(obj);
    }
}

} // namespace mcpd

#endif // MCPD_ICON_H
