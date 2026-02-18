/**
 * mcpd Example — Industrial CAN Bus Monitor
 *
 * Demonstrates:
 *   - CAN bus tools for reading/writing CAN frames
 *   - Rotary encoder for physical input (e.g., parameter adjustment)
 *   - Server diagnostics for remote monitoring
 *   - Heap monitoring for long-running industrial deployments
 *   - Session management for multi-operator access
 *
 * Hardware:
 *   - ESP32 with CAN transceiver (e.g., SN65HVD230) on GPIO 4/5
 *   - Rotary encoder on GPIO 32/33 with button on GPIO 25
 *
 * Usage:
 *   Claude: "Initialize CAN bus at 500kbps and show me the traffic"
 *   Claude: "Send a CAN frame with ID 0x123 and data [0x01, 0x02, 0x03]"
 *   Claude: "What's the encoder position?"
 *   Claude: "Show me the server diagnostics"
 */

#include <WiFi.h>
#include <mcpd.h>
#include <mcpd/tools/MCPCANTool.h>
#include <mcpd/tools/MCPEncoderTool.h>
#include <mcpd/MCPDiagnostics.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

// CAN bus pins (connect to CAN transceiver)
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

// Rotary encoder pins
#define ENC_A_PIN    32
#define ENC_B_PIN    33
#define ENC_BTN_PIN  25

mcpd::Server mcp("industrial-canbus");

// Ring buffer for CAN traffic logging
struct CANLogEntry {
    uint32_t timestamp;
    uint32_t id;
    uint8_t data[8];
    uint8_t dlc;
    bool tx;  // true = sent, false = received
};

static CANLogEntry canLog[100];
static int canLogHead = 0;
static int canLogCount = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Configure server
    mcp.setMaxSessions(2);  // Max 2 operators
    mcp.setSessionTimeout(60 * 60 * 1000);  // 1 hour timeout for industrial use
    mcp.setRateLimit(5, 20);  // Protect against runaway AI queries

    // Register CAN bus tools (TX on GPIO 5, RX on GPIO 4)
    mcpd::addCANTools(mcp, CAN_TX_PIN, CAN_RX_PIN);

    // Register rotary encoder (A=32, B=33, Button=25)
    mcpd::addEncoderTools(mcp, ENC_A_PIN, ENC_B_PIN, ENC_BTN_PIN);

    // Register diagnostics tool
    mcpd::addDiagnosticsTool(mcp);

    // Add a CAN traffic log resource
    mcp.addResource("can://traffic/log", "CAN Traffic Log",
        "Recent CAN bus frames (last 100)", "application/json",
        [](const String& uri) -> String {
            JsonDocument doc;
            JsonArray frames = doc["frames"].to<JsonArray>();
            int start = (canLogCount >= 100) ? canLogHead : 0;
            int count = min(canLogCount, 100);
            for (int i = 0; i < count; i++) {
                int idx = (start + i) % 100;
                JsonObject f = frames.add<JsonObject>();
                f["ts"] = canLog[idx].timestamp;
                f["id"] = String("0x") + String(canLog[idx].id, HEX);
                f["dlc"] = canLog[idx].dlc;
                f["dir"] = canLog[idx].tx ? "TX" : "RX";
                String dataStr;
                for (int j = 0; j < canLog[idx].dlc; j++) {
                    if (j > 0) dataStr += " ";
                    if (canLog[idx].data[j] < 0x10) dataStr += "0";
                    dataStr += String(canLog[idx].data[j], HEX);
                }
                f["data"] = dataStr;
            }
            doc["total_logged"] = canLogCount;
            String out;
            serializeJson(doc, out);
            return out;
        });

    // Add diagnostic prompt
    mcp.addPrompt("analyze_can_traffic", "Analyze recent CAN bus traffic for anomalies",
        {},
        [](const JsonObject& args) -> String {
            return R"([{"role":"user","content":{"type":"text","text":"Analyze the CAN bus traffic log. Look for:\n1. Unusual frame IDs or data patterns\n2. Error frames or bus-off conditions\n3. Message frequency anomalies\n4. Missing expected periodic messages\n5. Potential security issues (replay attacks, ID spoofing)\n\nRead the can://traffic/log resource and use can_status tool, then provide your analysis."}}])";
        });

    // Lifecycle hooks
    mcp.onInitialize([](const String& clientName) {
        Serial.printf("[CAN] Client connected: %s\n", clientName.c_str());
    });

    mcp.begin();
    Serial.println("[CAN] Industrial CAN Bus Monitor ready");
    Serial.printf("[CAN] Tools: CAN bus (5), Encoder (3), Diagnostics (1)\n");
}

void loop() {
    mcp.loop();

    // Sample heap periodically
    static unsigned long lastSample = 0;
    if (millis() - lastSample > 30000) {
        mcp.heap().sample();
        lastSample = millis();
    }
}
