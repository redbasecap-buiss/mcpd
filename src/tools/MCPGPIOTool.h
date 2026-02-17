/**
 * mcpd — Built-in GPIO Tools
 *
 * Provides: digital_read, digital_write, analog_read, pin_mode
 */

#ifndef MCPD_GPIO_TOOL_H
#define MCPD_GPIO_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class GPIOTool {
public:
    static void attach(Server& server) {
        // pin_mode
        server.addTool("pin_mode", "Set the mode of a GPIO pin",
            R"({"type":"object","properties":{"pin":{"type":"integer","description":"GPIO pin number"},"mode":{"type":"string","enum":["INPUT","OUTPUT","INPUT_PULLUP","INPUT_PULLDOWN"],"description":"Pin mode"}},"required":["pin","mode"]})",
            [](const JsonObject& args) -> String {
                int pin = args["pin"];
                String mode = args["mode"].as<String>();
                if (mode == "INPUT") pinMode(pin, INPUT);
                else if (mode == "OUTPUT") pinMode(pin, OUTPUT);
                else if (mode == "INPUT_PULLUP") pinMode(pin, INPUT_PULLUP);
                #ifdef INPUT_PULLDOWN
                else if (mode == "INPUT_PULLDOWN") pinMode(pin, INPUT_PULLDOWN);
                #endif
                else return "{\"error\":\"Unknown mode\"}";
                return String("{\"pin\":") + pin + ",\"mode\":\"" + mode + "\"}";
            });

        // digital_read
        server.addTool("digital_read", "Read the digital value of a GPIO pin",
            R"({"type":"object","properties":{"pin":{"type":"integer","description":"GPIO pin number"}},"required":["pin"]})",
            [](const JsonObject& args) -> String {
                int pin = args["pin"];
                int value = digitalRead(pin);
                return String("{\"pin\":") + pin + ",\"value\":" + value + "}";
            });

        // digital_write
        server.addTool("digital_write", "Write a digital value to a GPIO pin",
            R"({"type":"object","properties":{"pin":{"type":"integer","description":"GPIO pin number"},"value":{"type":"integer","enum":[0,1],"description":"HIGH (1) or LOW (0)"}},"required":["pin","value"]})",
            [](const JsonObject& args) -> String {
                int pin = args["pin"];
                int value = args["value"];
                digitalWrite(pin, value);
                return String("{\"pin\":") + pin + ",\"value\":" + value + "}";
            });

        // analog_read
        server.addTool("analog_read", "Read the analog value of a pin (0-4095 on ESP32)",
            R"({"type":"object","properties":{"pin":{"type":"integer","description":"Analog-capable GPIO pin number"}},"required":["pin"]})",
            [](const JsonObject& args) -> String {
                int pin = args["pin"];
                int value = analogRead(pin);
                return String("{\"pin\":") + pin + ",\"value\":" + value + "}";
            });
    }
};

} // namespace tools
} // namespace mcpd

#endif // MCPD_GPIO_TOOL_H
