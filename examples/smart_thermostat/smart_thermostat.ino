/**
 * mcpd Example: Smart Thermostat with AI Sampling
 *
 * Demonstrates:
 *   - Filesystem tools for storing temperature logs
 *   - Sampling: the MCU asks the AI to analyze temperature patterns
 *   - Custom tools for thermostat control
 *
 * The ESP32 reads temperature, logs to SPIFFS, and can ask Claude
 * to analyze the data and recommend HVAC settings.
 */

#include <WiFi.h>
#include <SPIFFS.h>
#include <mcpd.h>
#include <mcpd/tools/MCPFilesystemTool.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

mcpd::Server mcp("smart-thermostat");

float currentTemp = 22.5;
float targetTemp = 22.0;
bool hvacOn = false;
String hvacMode = "auto";  // auto, heat, cool, off

void setup() {
    Serial.begin(115200);

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Init filesystem
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed!");
        return;
    }

    // Register filesystem tools (AI can read/write logs)
    mcpd::MCPFilesystemTool::registerAll(mcp, SPIFFS);

    // Custom thermostat tools
    mcp.addTool("thermostat_status", "Get current thermostat status",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            JsonDocument doc;
            doc["currentTemp"] = currentTemp;
            doc["targetTemp"] = targetTemp;
            doc["hvacOn"] = hvacOn;
            doc["mode"] = hvacMode;
            doc["uptime"] = millis() / 1000;
            String result;
            serializeJson(doc, result);
            return result;
        });

    mcp.addTool("thermostat_set", "Set target temperature and HVAC mode",
        R"({"type":"object","properties":{"target":{"type":"number","description":"Target temperature in °C"},"mode":{"type":"string","enum":["auto","heat","cool","off"],"description":"HVAC mode"}},"required":[]})",
        [](const JsonObject& args) -> String {
            if (args.containsKey("target")) {
                targetTemp = args["target"].as<float>();
            }
            if (args.containsKey("mode")) {
                const char* m = args["mode"].as<const char*>();
                if (m) hvacMode = m;
            }
            JsonDocument doc;
            doc["targetTemp"] = targetTemp;
            doc["mode"] = hvacMode;
            doc["status"] = "updated";
            String result;
            serializeJson(doc, result);
            return result;
        });

    // AI-powered analysis tool: asks Claude to analyze temperature history
    mcp.addTool("thermostat_analyze", "Ask AI to analyze temperature patterns and recommend settings",
        R"({"type":"object","properties":{"hours":{"type":"integer","description":"Hours of history to analyze (default: 24)"}},"required":[]})",
        [](const JsonObject& args) -> String {
            // Read temperature log from filesystem
            File logFile = SPIFFS.open("/temp_log.csv", "r");
            String logData = "";
            if (logFile) {
                logData = logFile.readString();
                logFile.close();
            }

            if (logData.length() == 0) {
                return "No temperature history available yet.";
            }

            // Request AI sampling to analyze the data
            mcpd::MCPSamplingRequest req;
            req.addUserMessage(
                (String("Analyze this temperature log from my smart thermostat and recommend ") +
                 "optimal settings. Current target: " + String(targetTemp) + "°C, " +
                 "mode: " + hvacMode + ". Data:\n" + logData).c_str()
            );
            req.maxTokens = 500;
            req.modelPreferences.intelligencePriority = 0.8;

            int id = mcp.requestSampling(req, [](const mcpd::MCPSamplingResponse& resp) {
                if (resp.valid) {
                    Serial.printf("[thermostat] AI analysis: %s\n", resp.text.c_str());
                    // Store analysis result
                    File f = SPIFFS.open("/last_analysis.txt", "w");
                    if (f) {
                        f.print(resp.text);
                        f.close();
                    }
                }
            });

            if (id < 0) {
                return "Sampling unavailable — connect via SSE to enable AI analysis.";
            }
            return String("Analysis requested (id: ") + String(id) + "). Check /last_analysis.txt for results.";
        });

    // Prompt for thermostat optimization
    mcp.addPrompt("optimize_comfort", "Generate a prompt for optimizing thermostat comfort",
        {
            mcpd::MCPPromptArgument("preferences", "User comfort preferences", false),
            mcpd::MCPPromptArgument("schedule", "Daily schedule description", false)
        },
        [](const std::map<String, String>& args) -> std::vector<mcpd::MCPPromptMessage> {
            String prefs = "default comfort";
            String schedule = "typical work schedule";
            auto it = args.find("preferences");
            if (it != args.end()) prefs = it->second;
            it = args.find("schedule");
            if (it != args.end()) schedule = it->second;

            String msg = String("I have a smart thermostat. My comfort preferences: ") + prefs +
                ". My schedule: " + schedule +
                ". Current temp: " + String(currentTemp) + "°C, target: " + String(targetTemp) +
                "°C. Please suggest an optimal temperature schedule for the next 24 hours.";
            return { mcpd::MCPPromptMessage("user", msg.c_str()) };
        });

    mcp.begin();
    Serial.printf("Smart Thermostat MCP server ready at http://%s/mcp\n",
                  WiFi.localIP().toString().c_str());
}

void loop() {
    mcp.loop();

    // Simulate temperature reading every 30 seconds
    static unsigned long lastReading = 0;
    if (millis() - lastReading > 30000) {
        lastReading = millis();

        // Simulate sensor reading with some noise
        currentTemp += random(-10, 10) * 0.01;

        // Simple HVAC control
        if (hvacMode != "off") {
            if (currentTemp < targetTemp - 0.5) {
                hvacOn = true;
                currentTemp += 0.1; // heating
            } else if (currentTemp > targetTemp + 0.5) {
                hvacOn = true;
                currentTemp -= 0.1; // cooling
            } else {
                hvacOn = false;
            }
        }

        // Log to filesystem
        File logFile = SPIFFS.open("/temp_log.csv", "a");
        if (logFile) {
            logFile.printf("%lu,%0.1f,%0.1f,%s,%d\n",
                millis() / 1000, currentTemp, targetTemp,
                hvacMode.c_str(), hvacOn ? 1 : 0);
            logFile.close();
        }
    }
}
