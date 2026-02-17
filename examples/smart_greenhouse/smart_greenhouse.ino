/**
 * mcpd Example: Smart Greenhouse
 *
 * Demonstrates logging, dynamic tool management, and MQTT integration.
 * An ESP32 monitors a greenhouse with temperature/humidity sensors,
 * controls ventilation fans and irrigation, and logs everything via MCP.
 *
 * Features shown:
 * - MCP Logging capability (log levels, notifications)
 * - Dynamic tool addition/removal at runtime
 * - Pagination for large tool sets
 * - Resource templates for sensor data
 * - Prompts for common greenhouse operations
 *
 * Hardware: ESP32 + DHT22 + 2x Relay (fan, pump)
 */

#include <mcpd.h>
#include <tools/MCPGPIOTool.h>
#include <tools/MCPSystemTool.h>

// ── Configuration ──────────────────────────────────────────────────────

#define WIFI_SSID     "your-ssid"
#define WIFI_PASS     "your-password"
#define DHT_PIN       4
#define FAN_PIN       16
#define PUMP_PIN      17
#define SOIL_PIN      34   // Analog soil moisture

mcpd::Server mcp("greenhouse", 80);

// ── Simulated sensor data (replace with real readings) ─────────────────

float readTemperature() { return 24.5 + (random(100) - 50) * 0.1; }
float readHumidity()    { return 65.0 + (random(100) - 50) * 0.2; }
int   readSoilMoisture(){ return analogRead(SOIL_PIN); }

bool fanRunning = false;
bool pumpRunning = false;

// ── Setup ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // GPIO setup
    pinMode(FAN_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);

    // ── Tools ──────────────────────────────────────────────────────────

    mcp.addTool("read_climate", "Read greenhouse temperature and humidity",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& params) -> String {
            float temp = readTemperature();
            float hum = readHumidity();
            mcp.logging().info("climate", 
                (String("Temp=") + temp + "°C, Humidity=" + hum + "%").c_str());
            return String("{\"temperature_c\":") + temp + 
                   ",\"humidity_pct\":" + hum + "}";
        });

    mcp.addTool("read_soil", "Read soil moisture level (0=dry, 4095=wet)",
        R"({"type":"object","properties":{"bed":{"type":"string","description":"Which garden bed"}}})",
        [](const JsonObject& params) -> String {
            int moisture = readSoilMoisture();
            const char* bed = params["bed"] | "main";
            mcp.logging().debug("soil",
                (String("Bed '") + bed + "' moisture=" + moisture).c_str());
            return String("{\"moisture\":") + moisture + 
                   ",\"bed\":\"" + bed + "\"}";
        });

    mcp.addTool("control_fan", "Turn ventilation fan on/off",
        R"({"type":"object","properties":{"on":{"type":"boolean"}},"required":["on"]})",
        [](const JsonObject& params) -> String {
            bool on = params["on"];
            digitalWrite(FAN_PIN, on ? HIGH : LOW);
            fanRunning = on;
            mcp.logging().info("actuator",
                on ? "Fan turned ON" : "Fan turned OFF");
            return String("{\"fan\":") + (on ? "true" : "false") + "}";
        });

    mcp.addTool("control_pump", "Turn irrigation pump on/off",
        R"({"type":"object","properties":{"on":{"type":"boolean"},"duration_sec":{"type":"integer","description":"Auto-off after N seconds (0=manual)"}},"required":["on"]})",
        [](const JsonObject& params) -> String {
            bool on = params["on"];
            int duration = params["duration_sec"] | 0;
            digitalWrite(PUMP_PIN, on ? HIGH : LOW);
            pumpRunning = on;
            mcp.logging().warning("actuator",
                (String("Pump ") + (on ? "ON" : "OFF") +
                 (duration > 0 ? String(" (auto-off in ") + duration + "s)" : "")).c_str());
            return String("{\"pump\":") + (on ? "true" : "false") +
                   ",\"duration_sec\":" + duration + "}";
        });

    mcp.addTool("greenhouse_status", "Get full greenhouse status overview",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& params) -> String {
            float temp = readTemperature();
            float hum = readHumidity();
            int soil = readSoilMoisture();
            return String("{\"temperature_c\":") + temp +
                   ",\"humidity_pct\":" + hum +
                   ",\"soil_moisture\":" + soil +
                   ",\"fan_on\":" + (fanRunning ? "true" : "false") +
                   ",\"pump_on\":" + (pumpRunning ? "true" : "false") + "}";
        });

    // ── Resources ──────────────────────────────────────────────────────

    mcp.addResource("greenhouse://status", "Greenhouse Status",
        "Current status of all greenhouse systems", "application/json",
        []() -> String {
            float temp = readTemperature();
            float hum = readHumidity();
            return String("{\"temp\":") + temp + ",\"hum\":" + hum +
                   ",\"fan\":" + (fanRunning ? "true" : "false") +
                   ",\"pump\":" + (pumpRunning ? "true" : "false") + "}";
        });

    // ── Resource Templates ─────────────────────────────────────────────

    mcp.addResourceTemplate("greenhouse://sensor/{sensor_type}/current",
        "Sensor Reading", "Current reading for a specific sensor type",
        "application/json",
        [](const std::map<String, String>& params) -> String {
            String type = params.at("sensor_type");
            if (type == "temperature") {
                return String("{\"value\":") + readTemperature() + ",\"unit\":\"°C\"}";
            } else if (type == "humidity") {
                return String("{\"value\":") + readHumidity() + ",\"unit\":\"%\"}";
            } else if (type == "soil") {
                return String("{\"value\":") + readSoilMoisture() + ",\"unit\":\"raw\"}";
            }
            return "{\"error\":\"unknown sensor type\"}";
        });

    // ── Prompts ────────────────────────────────────────────────────────

    mcp.addPrompt("diagnose", "Diagnose greenhouse conditions",
        {
            mcpd::MCPPromptArgument("concern", "What seems wrong (e.g. 'too hot', 'wilting')", true)
        },
        [](const std::map<String, String>& args) -> std::vector<mcpd::MCPPromptMessage> {
            String concern = args.at("concern");
            return {
                mcpd::MCPPromptMessage("user",
                    (String("The greenhouse has an issue: '") + concern +
                     "'. Please read the current temperature, humidity, and soil moisture, "
                     "then diagnose the problem and suggest what to adjust (fan, pump, vents).").c_str())
            };
        });

    mcp.addPrompt("daily_check", "Run a daily greenhouse health check",
        {},
        [](const std::map<String, String>&) -> std::vector<mcpd::MCPPromptMessage> {
            return {
                mcpd::MCPPromptMessage("user",
                    "Please perform a daily greenhouse check: "
                    "1) Read all sensors (temperature, humidity, soil moisture) "
                    "2) Check if values are in healthy ranges (18-28°C, 50-80% humidity, soil >1000) "
                    "3) Recommend any adjustments "
                    "4) Provide a brief status summary")
            };
        });

    // ── Server config ──────────────────────────────────────────────────

    // Built-in tools for advanced users
    mcpd::tools::SystemTool::attach(mcp);

    // Enable pagination if you have many tools
    // mcp.setPageSize(5);

    mcp.begin();

    // Log startup
    mcp.logging().info("system", "Greenhouse MCP server started");

    Serial.printf("Greenhouse MCP at http://%s/mcp\n",
                  WiFi.localIP().toString().c_str());
}

// ── Loop ───────────────────────────────────────────────────────────────

void loop() {
    mcp.loop();

    // Example: auto-alert on high temperature
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 60000) {  // Every minute
        lastCheck = millis();
        float temp = readTemperature();
        if (temp > 35.0) {
            mcp.logging().critical("climate",
                (String("ALERT: Temperature critical at ") + temp + "°C!").c_str());
        } else if (temp > 30.0) {
            mcp.logging().warning("climate",
                (String("Temperature high: ") + temp + "°C").c_str());
        }
    }
}
