/**
 * mcpd Example — LoRa Smart Farm Node
 *
 * ESP32 with LoRa radio + soil moisture + DHT22 + relay for irrigation.
 * Multiple nodes form a LoRa mesh. Each node exposes its sensors and
 * actuators as MCP tools. An AI agent can monitor the entire farm,
 * coordinate irrigation, and make decisions based on weather patterns.
 *
 * Hardware:
 *   - ESP32 + SX1278 LoRa (Heltec WiFi LoRa 32 or TTGO LoRa32)
 *   - DHT22 on GPIO 4
 *   - Soil moisture sensor on ADC GPIO 36
 *   - Relay module on GPIO 16 (irrigation valve)
 *   - Optional: I2S microphone for acoustic pest detection
 *
 * Dependencies:
 *   - mcpd
 *   - sandeepmistry/LoRa
 *   - adafruit/DHT sensor library
 *   - bblanchon/ArduinoJson
 */

#include <WiFi.h>
#include <mcpd.h>
#include <mcpd/tools/MCPLoRaTool.h>
#include <mcpd/tools/MCPDHTTool.h>
#include <mcpd/tools/MCPGPIOTool.h>
#include <mcpd/tools/MCPADCTool.h>
#include <mcpd/tools/MCPRelayTool.h>
#include <mcpd/tools/MCPI2SAudioTool.h>
#include <DHT.h>

// ── Configuration ──────────────────────────────────────────────────────

const char* WIFI_SSID     = "your-ssid";
const char* WIFI_PASSWORD = "your-password";
const char* NODE_NAME     = "farm-node-1";

// Pin assignments
#define DHT_PIN        4
#define SOIL_MOISTURE  36
#define RELAY_PIN      16
#define LORA_SS        18
#define LORA_RST       14
#define LORA_DIO0      26

// LoRa configuration (EU 868 MHz)
#define LORA_FREQ      868E6
#define LORA_SF        10    // Higher SF = longer range, slower
#define LORA_BW        125000
#define LORA_TX_POWER  17

// ── Globals ────────────────────────────────────────────────────────────

mcpd::Server mcp(NODE_NAME, "1.0.0");
DHT dht(DHT_PIN, DHT22);

// ── Custom soil moisture tool ──────────────────────────────────────────

void addSoilMoistureTool(mcpd::Server& server) {
    server.addTool("soil_read",
        "Read soil moisture sensor. Returns raw ADC value (0-4095) and estimated percentage (0-100%). "
        "Dry soil ≈ 3000+, wet soil ≈ 1000-. Calibrate for your soil type.",
        R"({"type":"object","properties":{
            "samples":{"type":"integer","description":"Number of samples to average (default: 10)","default":10}
        }})",
        [](const JsonObject& args) -> String {
            int samples = args["samples"] | 10;

            long total = 0;
            int minVal = 4095, maxVal = 0;
            for (int i = 0; i < samples; i++) {
                int val = analogRead(SOIL_MOISTURE);
                total += val;
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
                delay(10);
            }
            int avg = total / samples;

            // Map to percentage (inverted: lower ADC = wetter)
            int pct = map(avg, 4095, 0, 0, 100);
            pct = constrain(pct, 0, 100);

            JsonDocument doc;
            doc["raw"] = avg;
            doc["moisturePercent"] = pct;
            doc["min"] = minVal;
            doc["max"] = maxVal;
            doc["samples"] = samples;

            // Qualitative assessment
            if (pct < 20) doc["status"] = "very_dry";
            else if (pct < 40) doc["status"] = "dry";
            else if (pct < 60) doc["status"] = "moist";
            else if (pct < 80) doc["status"] = "wet";
            else doc["status"] = "saturated";

            String result;
            serializeJson(doc, result);
            return result;
        });
}

// ── Setup ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n🌱 mcpd LoRa Smart Farm Node");

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Initialize sensors
    dht.begin();
    pinMode(SOIL_MOISTURE, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Register MCP tools
    mcpd::tools::DHTTool::attach(mcp, dht);
    addSoilMoistureTool(mcp);

    // LoRa radio for mesh communication
    mcpd::tools::LoRaPins loraPins;
    loraPins.ss = LORA_SS;
    loraPins.reset = LORA_RST;
    loraPins.dio0 = LORA_DIO0;
    mcpd::addLoRaTools(mcp, loraPins);

    // Relay for irrigation valve
    mcpd::addRelayTools(mcp);

    // I2S microphone (optional — for acoustic monitoring)
    mcpd::tools::I2SPins i2sPins;
    i2sPins.bck = 26;
    i2sPins.ws = 25;
    i2sPins.dataIn = 22;
    mcpd::addI2SAudioTools(mcp, i2sPins);

    // Add a farming-focused prompt
    mcp.addPrompt("farm_status",
        "Get a comprehensive farm status report for this node",
        {},
        [](const JsonObject& args) -> String {
            return "You are monitoring a smart farm node. Read the soil moisture (soil_read), "
                   "temperature/humidity (dht_read), and check LoRa for messages from other nodes "
                   "(lora_receive). If soil is dry (<30%), consider activating irrigation via the "
                   "relay. Report the overall health of this zone.";
        });

    // Expose soil moisture as a resource
    mcp.addResource("sensor://soil/current",
        "Current soil moisture reading",
        "application/json",
        [](const String& uri) -> String {
            int val = analogRead(SOIL_MOISTURE);
            int pct = map(val, 4095, 0, 0, 100);
            pct = constrain(pct, 0, 100);
            JsonDocument doc;
            doc["raw"] = val;
            doc["percent"] = pct;
            String result;
            serializeJson(doc, result);
            return result;
        });

    // Start MCP server
    mcp.begin();
    Serial.printf("🌱 Farm node ready — %d tools registered\n", mcp.toolCount());
}

// ── Loop ───────────────────────────────────────────────────────────────

void loop() {
    mcp.loop();
}
