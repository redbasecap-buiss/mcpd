/**
 * mcpd Example — Asset Tracker
 *
 * GPS location tracking with persistent storage and relay-controlled
 * power management. AI can read location, store waypoints, calculate
 * distances, and control power to peripherals.
 *
 * Hardware:
 *   - ESP32 dev board
 *   - NEO-6M GPS module (TX→GPIO16, RX→GPIO17)
 *   - 2-channel relay module (GPIO 25, 26)
 *   - Status LED (GPIO 2, built-in)
 *
 * Wiring:
 *   GPS VCC  → Relay CH1 (switchable power)
 *   GPS TX   → ESP32 GPIO16 (Serial2 RX)
 *   GPS RX   → ESP32 GPIO17 (Serial2 TX)
 *   Relay IN1 → GPIO 25 (GPS power)
 *   Relay IN2 → GPIO 26 (External sensor power)
 */

#include <WiFi.h>
#include <mcpd.h>
#include <mcpd/tools/MCPGPSTool.h>
#include <mcpd/tools/MCPNVSTool.h>
#include <mcpd/tools/MCPRelayTool.h>
#include <mcpd/tools/MCPSystemTool.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

mcpd::Server mcp("asset-tracker");

void setup() {
    Serial.begin(115200);
    Serial.println("\n🛰️ mcpd Asset Tracker starting...");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n📡 Connected: %s\n", WiFi.localIP().toString().c_str());

    // Configure relay channels
    // CH0: GPS module power (active-low, auto-off after 1 hour)
    mcpd::addRelayChannel(25, "gps_power", true, -1, 3600);
    // CH1: External sensor power (active-low, interlock group 1)
    mcpd::addRelayChannel(26, "sensor_power", true, -1, 0);

    // Register tools
    mcpd::addGPSTools(mcp, &Serial2, 9600);  // GPS on Serial2
    mcpd::addNVSTools(mcp, "tracker");         // Persistent storage
    mcpd::addRelayTools(mcp);                  // Relay control
    mcpd::addSystemTools(mcp);                 // System info

    // Add a custom resource for last known position
    mcp.addResource("tracker://position", "Last Known Position",
        "application/json", "Last GPS fix with timestamp",
        []() -> String {
            auto& d = mcpd::tools::GPSTool::data();
            if (!d.fix) return R"({"status":"no fix"})";
            char buf[128];
            snprintf(buf, sizeof(buf),
                "{\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,\"sats\":%d}",
                d.latitude, d.longitude, d.altitude, d.satellites);
            return buf;
        });

    // Add a prompt for the AI
    mcp.addPrompt("track", "Asset tracking assistant",
        {{"action", "string", false}},
        [](const std::map<String, String>& args) -> String {
            return "You are an asset tracking assistant. You can:\n"
                   "- Read GPS position (gps_read) and speed (gps_speed)\n"
                   "- Store/recall waypoints and settings (nvs_set/nvs_get)\n"
                   "- Calculate distances to saved locations (gps_distance)\n"
                   "- Control power to GPS and sensors (relay_set)\n"
                   "- Save location history by storing lat/lon in NVS\n\n"
                   "Power management: Turn off GPS when not needed to save battery. "
                   "Use relay_set with label 'gps_power' to control.\n\n"
                   "Always store important positions in NVS so they survive reboots.";
        });

    // Turn on GPS power by default
    mcpd::tools::RelayTool::setRelay(0, true);

    mcp.begin();
    Serial.println("🚀 MCP server ready! Tools: GPS, NVS, Relay, System");
}

void loop() {
    mcp.loop();
    mcpd::tools::RelayTool::checkTimers(); // Safety auto-off
}
