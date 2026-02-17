/**
 * mcpd — Built-in I2C Tools
 *
 * Provides: i2c_scan, i2c_read, i2c_write
 */

#ifndef MCPD_I2C_TOOL_H
#define MCPD_I2C_TOOL_H

#include "../mcpd.h"
#include <Wire.h>

namespace mcpd {
namespace tools {

class I2CTool {
public:
    static void attach(Server& server, TwoWire& wire = Wire) {
        // i2c_scan
        server.addTool("i2c_scan",
            "Scan the I2C bus and return addresses of connected devices",
            R"({"type":"object","properties":{}})",
            [&wire](const JsonObject& args) -> String {
                JsonDocument doc;
                JsonArray devices = doc["devices"].to<JsonArray>();

                for (uint8_t addr = 1; addr < 127; addr++) {
                    wire.beginTransmission(addr);
                    if (wire.endTransmission() == 0) {
                        char hex[7];
                        snprintf(hex, sizeof(hex), "0x%02X", addr);
                        devices.add(hex);
                    }
                }
                doc["count"] = devices.size();

                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2c_read
        server.addTool("i2c_read",
            "Read bytes from an I2C device",
            R"({"type":"object","properties":{"address":{"type":"integer","description":"I2C device address (decimal)"},"count":{"type":"integer","description":"Number of bytes to read (max 32)","default":1}},"required":["address"]})",
            [&wire](const JsonObject& args) -> String {
                uint8_t addr = args["address"];
                int count = args["count"] | 1;
                if (count > 32) count = 32;

                wire.requestFrom(addr, (uint8_t)count);
                JsonDocument doc;
                JsonArray bytes = doc["bytes"].to<JsonArray>();

                while (wire.available()) {
                    bytes.add(wire.read());
                }
                doc["address"] = addr;
                doc["requested"] = count;
                doc["received"] = bytes.size();

                String result;
                serializeJson(doc, result);
                return result;
            });

        // i2c_write
        server.addTool("i2c_write",
            "Write bytes to an I2C device",
            R"({"type":"object","properties":{"address":{"type":"integer","description":"I2C device address (decimal)"},"bytes":{"type":"array","items":{"type":"integer"},"description":"Bytes to write"}},"required":["address","bytes"]})",
            [&wire](const JsonObject& args) -> String {
                uint8_t addr = args["address"];
                JsonArray bytes = args["bytes"];

                wire.beginTransmission(addr);
                for (JsonVariant b : bytes) {
                    wire.write((uint8_t)b.as<int>());
                }
                uint8_t error = wire.endTransmission();

                JsonDocument doc;
                doc["address"] = addr;
                doc["bytesWritten"] = bytes.size();
                doc["success"] = (error == 0);
                doc["error"] = error;

                String result;
                serializeJson(doc, result);
                return result;
            });
    }
};

} // namespace tools
} // namespace mcpd

#endif // MCPD_I2C_TOOL_H
