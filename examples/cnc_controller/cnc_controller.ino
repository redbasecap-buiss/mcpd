/**
 * mcpd Example — CNC Controller
 *
 * AI-controlled CNC/3D-printer style motion with:
 *   - 3-axis stepper motors (X, Y, Z) with homing
 *   - Pulse counter for spindle RPM measurement
 *   - Touch sensors for material detection / tool probe
 *   - Server diagnostics for remote monitoring
 *
 * Hardware:
 *   - 3x stepper drivers (A4988/DRV8825/TMC2208) on STEP/DIR pins
 *   - 3x endstop switches (normally open, pulled high)
 *   - Hall-effect sensor on spindle for RPM
 *   - Capacitive touch pads for tool/material probe
 *
 * Claude/GPT can:
 *   - Move axes to positions, run homing sequences
 *   - Monitor spindle RPM in real-time
 *   - Use touch probes for surface detection
 *   - Diagnose machine health and memory usage
 */

#include <WiFi.h>
#include <mcpd.h>
#include <mcpd/tools/MCPStepperTool.h>
#include <mcpd/tools/MCPPulseCounterTool.h>
#include <mcpd/tools/MCPTouchTool.h>

// ── WiFi ───────────────────────────────────────────────────────────────

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

// ── Pin Assignments ────────────────────────────────────────────────────

// X-axis stepper
#define X_STEP  26
#define X_DIR   25
#define X_EN    33
#define X_END   34  // endstop

// Y-axis stepper
#define Y_STEP  27
#define Y_DIR   14
#define Y_EN    12
#define Y_END   35

// Z-axis stepper
#define Z_STEP  16
#define Z_DIR   17
#define Z_EN    5
#define Z_END   36

// Spindle RPM sensor
#define SPINDLE_SENSOR  39

// Touch probe pads
#define TOUCH_PROBE1  4   // T0 — tool length probe
#define TOUCH_PROBE2  15  // T3 — material surface probe

// ── Server ─────────────────────────────────────────────────────────────

mcpd::Server mcp("cnc-controller");

void setup() {
    Serial.begin(115200);
    Serial.println("\n[CNC Controller] Starting...");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[CNC] Connected: %s\n", WiFi.localIP().toString().c_str());

    // Register 3-axis steppers
    mcpd::addStepperTools(mcp, X_STEP, X_DIR, X_EN, X_END);  // index 0 = X
    mcpd::addStepperTools(mcp, Y_STEP, Y_DIR, Y_EN, Y_END);  // index 1 = Y
    mcpd::addStepperTools(mcp, Z_STEP, Z_DIR, Z_EN, Z_END);  // index 2 = Z

    // Spindle RPM counter (1 pulse per revolution)
    mcpd::addPulseCounterTools(mcp, SPINDLE_SENSOR, 1.0f, "revolutions", 1);

    // Touch probes
    int touchPins[] = { TOUCH_PROBE1, TOUCH_PROBE2 };
    const char* touchLabels[] = { "tool_probe", "surface_probe" };
    mcpd::addTouchTools(mcp, touchPins, 2, touchLabels);

    // Diagnostics
    mcpd::addDiagnosticsTool(mcp);

    // Session management
    mcp.setMaxSessions(2);
    mcp.setSessionTimeout(600000);  // 10 min idle timeout

    // CNC-specific resource: machine position
    mcp.addResource("cnc://position", "Machine Position",
        "Current XYZ position of all axes", "application/json",
        []() -> String {
            JsonDocument doc;
            doc["x"]["position"] = (long)mcpd::_steppers[0].currentPos;
            doc["x"]["running"] = mcpd::_steppers[0].running;
            doc["y"]["position"] = (long)mcpd::_steppers[1].currentPos;
            doc["y"]["running"] = mcpd::_steppers[1].running;
            doc["z"]["position"] = (long)mcpd::_steppers[2].currentPos;
            doc["z"]["running"] = mcpd::_steppers[2].running;
            doc["any_moving"] = mcpd::_steppers[0].running ||
                                mcpd::_steppers[1].running ||
                                mcpd::_steppers[2].running;
            String out;
            serializeJson(doc, out);
            return out;
        });

    // Prompt for AI-driven machining
    mcp.addPrompt("machine_status", "Analyze CNC machine state and suggest actions",
        {},
        [](const std::map<String, String>&) -> std::vector<mcpd::MCPPromptMessage> {
            return {
                mcpd::MCPPromptMessage("user",
                    "You are controlling a 3-axis CNC machine via MCP tools. "
                    "Read the current position, spindle RPM, and touch probe states. "
                    "Report the machine status and suggest any needed actions "
                    "(homing, tool changes, safety checks). "
                    "Always check endstops before large moves.")
            };
        });

    mcp.begin();
    Serial.println("[CNC] MCP server ready — tools: stepper, pulse counter, touch, diagnostics");
}

void loop() {
    mcp.loop();
    mcpd::stepperLoop();  // update stepper motion
}
