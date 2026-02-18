/**
 * mcpd Example: Data Logger
 *
 * Demonstrates ADC + UART tools for data acquisition:
 * - Multi-channel ADC sampling with voltage conversion
 * - UART communication with serial peripherals (GPS, sensors)
 * - In-memory ring buffer data logging with MCP resource access
 * - Configurable sampling intervals
 * - Roots for describing data domains
 *
 * Hardware:
 * - ESP32 (or RP2040)
 * - Analog sensors on GPIO 34, 35, 36 (ESP32 ADC1 channels)
 * - GPS module or sensor on Serial2 (GPIO 16=RX, 17=TX)
 *
 * The AI can:
 * - Read live sensor values via ADC tools
 * - Communicate with serial peripherals via UART tools
 * - Access logged data via resources
 * - Configure sampling via prompts
 */

#include <WiFi.h>
#include <mcpd.h>
#include <mcpd/tools/MCPADCTool.h>
#include <mcpd/tools/MCPUARTTool.h>
#include <mcpd/tools/MCPGPIOTool.h>
#include <mcpd/tools/MCPSystemTool.h>

// ── Config ─────────────────────────────────────────────────────────────

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASS";

// ADC channels to log
const int ADC_PINS[] = {34, 35, 36};
const int NUM_CHANNELS = 3;
const char* CHANNEL_NAMES[] = {"temperature", "humidity", "light"};

// Ring buffer for data logging
const int LOG_SIZE = 100;  // entries per channel
struct LogEntry {
    unsigned long timestamp;
    int values[3];
};
LogEntry logBuffer[LOG_SIZE];
int logHead = 0;
int logCount = 0;

unsigned long lastSample = 0;
unsigned long sampleIntervalMs = 5000;  // default 5 seconds

// ── Server ─────────────────────────────────────────────────────────────

mcpd::Server mcp("data-logger");

void setup() {
    Serial.begin(115200);
    Serial.println("\n[data-logger] Starting...");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[data-logger] WiFi connected: %s\n",
                  WiFi.localIP().toString().c_str());

    // ── Attach built-in tools ──────────────────────────────────────────

    mcpd::tools::ADCTool::attach(mcp);
    mcpd::tools::UARTTool::attach(mcp);
    mcpd::tools::GPIOTool::attach(mcp);
    mcpd::tools::SystemTool::attach(mcp);

    // ── Custom tools ───────────────────────────────────────────────────

    // Set sampling interval
    mcp.addTool("set_sample_interval", "Set the data logging sample interval",
        R"({"type":"object","properties":{"intervalMs":{"type":"integer","description":"Sample interval in milliseconds (100-60000)","minimum":100,"maximum":60000}},"required":["intervalMs"]})",
        [](const JsonObject& args) -> String {
            sampleIntervalMs = args["intervalMs"].as<unsigned long>();
            return String("{\"intervalMs\":") + sampleIntervalMs +
                   ",\"intervalSec\":" + String(sampleIntervalMs / 1000.0, 1) + "}";
        });

    // Get latest readings
    mcp.addTool("get_latest", "Get the most recent logged sensor readings",
        R"({"type":"object","properties":{"count":{"type":"integer","description":"Number of recent entries (1-100, default 10)","minimum":1,"maximum":100}},"required":[]})",
        [](const JsonObject& args) -> String {
            int count = args["count"] | 10;
            if (count > logCount) count = logCount;

            String result = "{\"entries\":[";
            for (int i = 0; i < count; i++) {
                int idx = (logHead - 1 - i + LOG_SIZE) % LOG_SIZE;
                if (i > 0) result += ",";
                result += "{\"t\":" + String(logBuffer[idx].timestamp);
                result += ",\"values\":{";
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    if (ch > 0) result += ",";
                    result += "\"" + String(CHANNEL_NAMES[ch]) + "\":" +
                              String(logBuffer[idx].values[ch]);
                }
                result += "}}";
            }
            result += "],\"total\":" + String(logCount) +
                      ",\"intervalMs\":" + String(sampleIntervalMs) + "}";
            return result;
        });

    // Clear log
    mcp.addTool("clear_log", "Clear the data log buffer",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            int cleared = logCount;
            logHead = 0;
            logCount = 0;
            return String("{\"cleared\":") + cleared + "}";
        });

    // ── Resources ──────────────────────────────────────────────────────

    mcp.addResource("data://logger/status", "Logger Status",
        "Current data logger status and statistics", "application/json",
        []() -> String {
            return String("{\"running\":true") +
                   ",\"entries\":" + logCount +
                   ",\"capacity\":" + LOG_SIZE +
                   ",\"intervalMs\":" + sampleIntervalMs +
                   ",\"uptimeMs\":" + millis() +
                   ",\"channels\":" + NUM_CHANNELS + "}";
        });

    mcp.addResourceTemplate("data://logger/channel/{channel}",
        "Channel Data", "Read data for a specific channel", "application/json",
        [](const std::map<String, String>& params) -> String {
            String channel = params.at("channel");
            int chIdx = -1;
            for (int i = 0; i < NUM_CHANNELS; i++) {
                if (channel == CHANNEL_NAMES[i]) { chIdx = i; break; }
            }
            if (chIdx < 0) return "{\"error\":\"Unknown channel\"}";

            String result = "{\"channel\":\"" + channel + "\",\"values\":[";
            int count = min(logCount, LOG_SIZE);
            for (int i = 0; i < count; i++) {
                int idx = (logHead - count + i + LOG_SIZE) % LOG_SIZE;
                if (i > 0) result += ",";
                result += String(logBuffer[idx].values[chIdx]);
            }
            result += "],\"count\":" + String(count) + "}";
            return result;
        });

    // ── Roots ──────────────────────────────────────────────────────────

    mcp.addRoot("data://logger/", "Data Logger");
    mcp.addRoot("sensor://adc/", "ADC Sensors");
    mcp.addRoot("serial://uart/", "UART Peripherals");

    // ── Prompts ────────────────────────────────────────────────────────

    mcp.addPrompt("analyze_data", "Analyze logged sensor data",
        {
            mcpd::MCPPromptArgument("channel", "Channel to analyze (temperature, humidity, light)", true),
            mcpd::MCPPromptArgument("focus", "Analysis focus: trend, anomalies, statistics", false)
        },
        [](const std::map<String, String>& args) -> std::vector<mcpd::MCPPromptMessage> {
            String channel = args.at("channel");
            String focus = "statistics";
            auto it = args.find("focus");
            if (it != args.end()) focus = it->second;

            String msg = "Analyze the " + channel + " sensor data from the data logger. "
                        "Focus on " + focus + ". "
                        "Use get_latest to retrieve recent readings, "
                        "and data://logger/channel/" + channel + " for the full dataset. "
                        "Provide insights and recommendations.";
            return { mcpd::MCPPromptMessage("user", msg.c_str()) };
        });

    // ── Completions ────────────────────────────────────────────────────

    mcp.completions().addPromptCompletion("analyze_data", "channel",
        [](const String& prefix, bool& hasMore) -> std::vector<String> {
            std::vector<String> results;
            for (int i = 0; i < NUM_CHANNELS; i++) {
                String name(CHANNEL_NAMES[i]);
                if (prefix.length() == 0 || name.startsWith(prefix)) {
                    results.push_back(name);
                }
            }
            hasMore = false;
            return results;
        });

    // ── Start ──────────────────────────────────────────────────────────

    mcp.begin();
    Serial.println("[data-logger] MCP server started. Tools: ADC, UART, GPIO, System + custom");
}

void loop() {
    mcp.loop();

    // Sample ADC channels at configured interval
    if (millis() - lastSample >= sampleIntervalMs) {
        lastSample = millis();

        LogEntry entry;
        entry.timestamp = millis();
        for (int i = 0; i < NUM_CHANNELS; i++) {
            // Average 4 samples per channel for noise reduction
            long sum = 0;
            for (int s = 0; s < 4; s++) sum += analogRead(ADC_PINS[i]);
            entry.values[i] = (int)(sum / 4);
        }

        logBuffer[logHead] = entry;
        logHead = (logHead + 1) % LOG_SIZE;
        if (logCount < LOG_SIZE) logCount++;
    }
}
