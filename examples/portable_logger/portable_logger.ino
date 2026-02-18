/**
 * Portable Data Logger — mcpd Example
 *
 * Demonstrates SD card + battery monitoring + DHT sensor for
 * portable environmental data logging.
 *
 * Hardware:
 *   - ESP32 (any dev board)
 *   - MicroSD card module (SPI, CS=5)
 *   - DHT22 sensor (GPIO 4)
 *   - Battery via voltage divider (GPIO 34, 100k+100k)
 *   - Optional: charging detection (GPIO 35)
 *
 * AI can:
 *   - Read sensor data and log to SD card
 *   - Query battery status for power management
 *   - Browse and download logged data files
 *   - Manage storage (list, delete old logs)
 *   - Calibrate battery monitoring
 */

#include <mcpd.h>
#include <mcpd/tools/MCPSDCardTool.h>
#include <mcpd/tools/MCPBatteryTool.h>
#include <mcpd/tools/MCPDHTTool.h>
#include <mcpd/tools/MCPSystemTool.h>
#include <mcpd/tools/MCPWiFiTool.h>

// WiFi credentials
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

// Pin definitions
#define SD_CS_PIN       5
#define DHT_PIN         4
#define BATTERY_ADC_PIN 34
#define CHARGING_PIN    35

mcpd::Server mcp("portable-logger");

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Portable Data Logger ===");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Register tools
    mcpd::tools::addSDCardTools(mcp, SD_CS_PIN);
    mcpd::tools::addBatteryTools(mcp, BATTERY_ADC_PIN, 2.0f);
    mcpd::tools::addDHTTools(mcp, DHT_PIN);
    mcpd::tools::addSystemTools(mcp);
    mcpd::tools::addWiFiTools(mcp);

    // Custom resource: current logger status
    mcp.addResource("logger://status", "Logger Status",
        "Current portable logger status including battery, storage, and sensor readings",
        "application/json",
        [](const String&) -> String {
            auto battery = mcpd::tools::BatteryTool::readBattery();
            bool sdReady = mcpd::tools::SDCardTool::mounted();

            return String(R"({"battery":{"voltage":)") + String(battery.voltage, 2) +
                   R"(,"percentage":)" + battery.percentage +
                   R"(,"charging":)" + (battery.charging ? "true" : "false") +
                   R"(},"sd_mounted":)" + (sdReady ? "true" : "false") +
                   R"(,"uptime_s":)" + (millis() / 1000) +
                   R"(,"wifi_rssi":)" + WiFi.RSSI() + "}";
        }
    );

    // Prompt for data logging workflow
    mcp.addPrompt("log_session", "Start a data logging session",
        {{"interval_sec", "Logging interval in seconds", true},
         {"filename", "Log file name", false}},
        [](const std::map<String, String>& args) -> std::vector<mcpd::MCPPromptMessage> {
            String interval = "60";
            String filename = "/log.csv";
            auto it = args.find("interval_sec");
            if (it != args.end()) interval = it->second;
            it = args.find("filename");
            if (it != args.end()) filename = it->second;

            return {{
                "user",
                "Start logging temperature and humidity to SD card file '" + filename +
                "' every " + interval + " seconds. First check battery level - if below 20%, "
                "warn about limited runtime. Create the CSV with headers if it doesn't exist. "
                "Report current conditions and estimated storage capacity."
            }};
        }
    );

    mcp.begin();
    Serial.printf("MCP server ready on port 80 (%d tools)\n", mcp.toolCount());
}

void loop() {
    mcp.loop();
}
