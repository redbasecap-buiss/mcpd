/**
 * mcpd — Sensor Hub Example
 *
 * Multi-sensor setup with tools AND resources.
 * Demonstrates how to expose sensor data both as callable tools
 * and as readable MCP resources.
 */

#include <mcpd.h>
#include <tools/MCPSystemTool.h>
#include <tools/MCPI2CTool.h>

const char* WIFI_SSID = "YourSSID";
const char* WIFI_PASS = "YourPassword";

mcpd::Server mcp("sensor-hub");

// Simulated sensor data (replace with real sensor reads)
float readTemperature() { return 22.5 + random(-20, 20) / 10.0; }
float readHumidity() { return 55.0 + random(-100, 100) / 10.0; }
float readPressure() { return 1013.25 + random(-50, 50) / 10.0; }

void setup() {
    Serial.begin(115200);
    Wire.begin();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());

    // ── Built-in tools ─────────────────────────────────
    mcpd::tools::SystemTool::attach(mcp);
    mcpd::tools::I2CTool::attach(mcp);

    // ── Custom sensor tools ────────────────────────────
    mcp.addTool("read_environment",
        "Read temperature, humidity, and pressure",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            JsonDocument doc;
            doc["temperature_c"] = readTemperature();
            doc["humidity_pct"] = readHumidity();
            doc["pressure_hpa"] = readPressure();
            doc["timestamp"] = millis();
            String r; serializeJson(doc, r); return r;
        });

    // ── MCP Resources ──────────────────────────────────
    mcp.addResource(
        "sensor://temperature", "Temperature",
        "Current temperature reading in Celsius",
        "application/json",
        []() -> String {
            return String("{\"value\":") + readTemperature() + ",\"unit\":\"°C\"}";
        });

    mcp.addResource(
        "sensor://humidity", "Humidity",
        "Current relative humidity",
        "application/json",
        []() -> String {
            return String("{\"value\":") + readHumidity() + ",\"unit\":\"%\"}";
        });

    mcp.begin();
}

void loop() {
    mcp.loop();
}
