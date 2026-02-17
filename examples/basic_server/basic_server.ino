/**
 * mcpd — Basic Server Example
 *
 * Minimal MCP server with one custom tool.
 * Connect to Claude Desktop via the mcpd-bridge.
 */

#include <mcpd.h>

// WiFi credentials
const char* WIFI_SSID = "YourSSID";
const char* WIFI_PASS = "YourPassword";

// Create MCP server
mcpd::Server mcp("basic-server");

void setup() {
    Serial.begin(115200);
    Serial.println("\n[mcpd] Basic Server Example");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());

    // Register a simple tool
    mcp.addTool(
        "hello",
        "Say hello — returns a greeting with the given name",
        R"({
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "Name to greet"
                }
            },
            "required": ["name"]
        })",
        [](const JsonObject& args) -> String {
            String name = args["name"].as<String>();
            return String("{\"greeting\":\"Hello, ") + name + "! I'm an ESP32 running mcpd.\"}";
        }
    );

    // Register a tool that reads an analog pin
    mcp.addTool(
        "read_adc",
        "Read the ADC value from GPIO 34",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            int value = analogRead(34);
            float voltage = value * 3.3 / 4095.0;
            return String("{\"raw\":") + value + ",\"voltage\":" + voltage + "}";
        }
    );

    // Start the MCP server
    mcp.begin();
}

void loop() {
    mcp.loop();
}
