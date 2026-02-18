/**
 * mcpd — Smart Parking Garage Example
 *
 * Demonstrates: Ultrasonic distance sensors for bay occupancy,
 * NeoPixel status indicators, buzzer for proximity alerts,
 * DAC output for analog level indicator.
 *
 * Hardware:
 *   - 2x HC-SR04 ultrasonic sensors (parking bays)
 *   - 1x NeoPixel strip (8 LEDs for status)
 *   - 1x Piezo buzzer for proximity warning
 *   - DAC output for analog occupancy gauge
 *
 * The AI can query bay status, trigger alerts, and control indicators.
 */

#include <mcpd.h>
#include <mcpd/tools/MCPUltrasonicTool.h>
#include <mcpd/tools/MCPNeoPixelTool.h>
#include <mcpd/tools/MCPBuzzerTool.h>
#include <mcpd/tools/MCPDACTool.h>
#include <mcpd/tools/MCPWiFiTool.h>
#include <mcpd/tools/MCPSystemTool.h>
#include <mcpd/MCPDiagnostics.h>

// ── Pin Configuration ──────────────────────────────────────────
#define BAY1_TRIG     12
#define BAY1_ECHO     13
#define BAY2_TRIG     14
#define BAY2_ECHO     27
#define NEOPIXEL_PIN  15
#define BUZZER_PIN    4
#define DAC_PIN       25   // Analog occupancy output

#define TOTAL_BAYS    2
#define OCCUPIED_THRESHOLD_CM 50.0  // Car present if < 50cm

// ── WiFi ───────────────────────────────────────────────────────
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

mcpd::Server mcp("parking-garage");

// Track bay status for the resource
bool bayOccupied[TOTAL_BAYS] = {false, false};
float bayDistances[TOTAL_BAYS] = {0, 0};

void setup() {
    Serial.begin(115200);
    Serial.println("\n🅿️ Smart Parking Garage — mcpd");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Register tools
    mcpd::addUltrasonicTools(mcp, BAY1_TRIG, BAY1_ECHO, 300.0f, "Bay 1");
    mcpd::addUltrasonicTools(mcp, BAY2_TRIG, BAY2_ECHO, 300.0f, "Bay 2");
    mcpd::addBuzzerTools(mcp, BUZZER_PIN, 0);
    mcpd::addDACTools(mcp);

    // NeoPixel for visual status (8 LEDs: 4 per bay)
    // Uses existing NeoPixel tool from mcpd

    // ── Custom resource: parking status ────────────────────────
    mcp.addResource("parking://status", "Parking Status",
        "Real-time parking bay occupancy and distances",
        "application/json",
        []() -> String {
            int occupied = 0;
            String bays = "[";
            for (int i = 0; i < TOTAL_BAYS; i++) {
                if (i > 0) bays += ",";
                bays += String("{\"bay\":") + (i + 1) +
                        ",\"occupied\":" + (bayOccupied[i] ? "true" : "false") +
                        ",\"distance_cm\":" + String(bayDistances[i], 1) + "}";
                if (bayOccupied[i]) occupied++;
            }
            bays += "]";

            int availPct = (int)((TOTAL_BAYS - occupied) * 100.0 / TOTAL_BAYS);
            return String("{\"total_bays\":") + TOTAL_BAYS +
                   ",\"occupied\":" + occupied +
                   ",\"available\":" + (TOTAL_BAYS - occupied) +
                   ",\"availability_pct\":" + availPct +
                   ",\"bays\":" + bays + "}";
        });

    // ── Custom resource: subscribe to occupancy changes ────────
    mcp.addResourceTemplate("parking://bay/{bay_id}/status",
        "Bay Status", "Individual bay occupancy status",
        "application/json",
        [](const std::map<String, String>& params) -> String {
            int bay = params.at("bay_id").toInt() - 1;
            if (bay < 0 || bay >= TOTAL_BAYS)
                return R"({"error":"Invalid bay ID"})";

            return String("{\"bay\":") + (bay + 1) +
                   ",\"occupied\":" + (bayOccupied[bay] ? "true" : "false") +
                   ",\"distance_cm\":" + String(bayDistances[bay], 1) +
                   ",\"threshold_cm\":" + String(OCCUPIED_THRESHOLD_CM, 0) + "}";
        });

    // ── Prompt: parking analysis ───────────────────────────────
    mcp.addPrompt("analyze_parking", "Analyze parking garage status and suggest actions",
        {},
        [](const std::map<String, String>& args) -> std::vector<mcpd::MCPPromptMessage> {
            int occupied = 0;
            String status;
            for (int i = 0; i < TOTAL_BAYS; i++) {
                if (bayOccupied[i]) occupied++;
                status += "Bay " + String(i + 1) + ": " +
                          (bayOccupied[i] ? "OCCUPIED" : "EMPTY") +
                          " (" + String(bayDistances[i], 1) + " cm)\n";
            }

            String prompt = "Parking garage status:\n" + status +
                            "\nTotal: " + String(occupied) + "/" + String(TOTAL_BAYS) + " occupied.\n"
                            "Analyze the current state. If bays are nearly full, suggest alerts. "
                            "If a sensor reads abnormally, flag potential issues.";

            return {{
                "user",
                {{mcpd::ContentType::TEXT, prompt}}
            }};
        });

    // WiFi, System, Diagnostics
    mcpd::tools::WiFiTool::attach(mcp);
    mcpd::tools::SystemTool::attach(mcp);
    mcpd::addDiagnosticsTool(mcp);

    mcp.begin();
    Serial.println("🅿️ MCP server ready — parking garage online");
}

void loop() {
    mcp.loop();

    // Update bay readings every 500ms
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 500) {
        lastUpdate = millis();

        for (int i = 0; i < TOTAL_BAYS; i++) {
            float d = mcpd::tools::UltrasonicTool::measureCm(
                mcpd::tools::UltrasonicTool::configs[i]);
            if (d >= 0) {
                bayDistances[i] = d;
                bayOccupied[i] = (d < OCCUPIED_THRESHOLD_CM);
            }
        }

        // Update DAC output: 0V = empty, 3.3V = full
        int occupied = 0;
        for (int i = 0; i < TOTAL_BAYS; i++)
            if (bayOccupied[i]) occupied++;

        uint8_t dacVal = (uint8_t)(occupied * 255 / TOTAL_BAYS);
        dacWrite(DAC_PIN, dacVal);
    }
}
