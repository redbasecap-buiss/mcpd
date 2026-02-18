/**
 * mcpd — Roots Support
 *
 * MCP roots allow the server to advertise filesystem/URI roots that the
 * client can use to understand the server's context. For microcontrollers,
 * roots typically represent hardware subsystems or data domains.
 *
 * Example roots:
 *   - "sensor://temperature/" — temperature sensor namespace
 *   - "gpio://esp32/"        — GPIO pin namespace
 *   - "config://device/"     — device configuration
 */

#ifndef MCPD_ROOTS_H
#define MCPD_ROOTS_H

#include <Arduino.h>
#include <vector>

namespace mcpd {

struct MCPRoot {
    String uri;
    String name;

    MCPRoot(const char* uri, const char* name)
        : uri(uri), name(name) {}

    void toJson(JsonObject& obj) const {
        obj["uri"] = uri;
        if (name.length() > 0) {
            obj["name"] = name;
        }
    }
};

} // namespace mcpd

#endif // MCPD_ROOTS_H
