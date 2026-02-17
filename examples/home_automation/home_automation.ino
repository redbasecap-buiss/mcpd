/**
 * mcpd — Home Automation Example
 *
 * GPIO control + status reporting.
 * Control relays, read switches, all via MCP.
 */

#include <mcpd.h>
#include <tools/MCPGPIOTool.h>
#include <tools/MCPSystemTool.h>
#include <tools/MCPWiFiTool.h>

const char* WIFI_SSID = "YourSSID";
const char* WIFI_PASS = "YourPassword";

// Pin assignments
const int RELAY_LIGHT   = 26;
const int RELAY_FAN     = 27;
const int SENSOR_DOOR   = 33;
const int SENSOR_MOTION = 32;

mcpd::Server mcp("home-auto");

void setup() {
    Serial.begin(115200);

    // Setup pins
    pinMode(RELAY_LIGHT, OUTPUT);
    pinMode(RELAY_FAN, OUTPUT);
    pinMode(SENSOR_DOOR, INPUT_PULLUP);
    pinMode(SENSOR_MOTION, INPUT);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());

    // ── Built-in tools ─────────────────────────────────
    mcpd::tools::GPIOTool::attach(mcp);
    mcpd::tools::SystemTool::attach(mcp);
    mcpd::tools::WiFiTool::attach(mcp);

    // ── Home-specific tools ────────────────────────────
    mcp.addTool("light_control",
        "Turn the light on or off",
        R"({"type":"object","properties":{"state":{"type":"string","enum":["on","off"],"description":"Desired light state"}},"required":["state"]})",
        [](const JsonObject& args) -> String {
            String state = args["state"].as<String>();
            digitalWrite(RELAY_LIGHT, state == "on" ? HIGH : LOW);
            return String("{\"light\":\"") + state + "\"}";
        });

    mcp.addTool("fan_control",
        "Turn the fan on or off",
        R"({"type":"object","properties":{"state":{"type":"string","enum":["on","off"],"description":"Desired fan state"}},"required":["state"]})",
        [](const JsonObject& args) -> String {
            String state = args["state"].as<String>();
            digitalWrite(RELAY_FAN, state == "on" ? HIGH : LOW);
            return String("{\"fan\":\"") + state + "\"}";
        });

    mcp.addTool("home_status",
        "Get the current status of all home devices and sensors",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            JsonDocument doc;
            doc["light"] = digitalRead(RELAY_LIGHT) ? "on" : "off";
            doc["fan"] = digitalRead(RELAY_FAN) ? "on" : "off";
            doc["door"] = digitalRead(SENSOR_DOOR) ? "closed" : "open";
            doc["motion"] = digitalRead(SENSOR_MOTION) ? "detected" : "none";
            doc["uptimeMs"] = millis();
            String r; serializeJson(doc, r); return r;
        });

    // ── Resources ──────────────────────────────────────
    mcp.addResource("home://status", "Home Status",
        "Current state of all devices", "application/json",
        []() -> String {
            JsonDocument doc;
            doc["light"] = digitalRead(RELAY_LIGHT) ? "on" : "off";
            doc["fan"] = digitalRead(RELAY_FAN) ? "on" : "off";
            doc["door"] = digitalRead(SENSOR_DOOR) ? "closed" : "open";
            doc["motion"] = digitalRead(SENSOR_MOTION) ? "detected" : "none";
            String r; serializeJson(doc, r); return r;
        });

    mcp.begin();
    Serial.println("[mcpd] Home automation server ready!");
}

void loop() {
    mcp.loop();
}
