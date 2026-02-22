/**
 * mcpd — Async Tasks Example
 *
 * Demonstrates the MCP Tasks feature (MCP 2025-11-25 experimental):
 *   - Long-running firmware update with progress tracking
 *   - Async sensor calibration with input-required flow
 *   - Task lifecycle: working → completed/failed/cancelled
 *
 * Claude or any MCP client can start async operations and poll for status.
 */

#include <mcpd.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

mcpd::Server mcp("async-demo");

// ── Simulated firmware update state ──────────────────────────────
struct FirmwareUpdate {
    String taskId;
    int progress = 0;       // 0-100
    bool active = false;
    unsigned long lastStep = 0;
};
FirmwareUpdate fwUpdate;

// ── Simulated calibration state ──────────────────────────────────
struct Calibration {
    String taskId;
    bool waitingForInput = false;
    bool active = false;
    float baseline = 0.0f;
};
Calibration calibration;

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

    // Enable Tasks capability
    mcp.enableTasks();

    // ── Async firmware update tool ───────────────────────────────
    // TaskSupport::Required means this tool MUST be called as a task
    mcp.addTaskTool(
        "firmware_update",
        "Start an OTA firmware update. This is a long-running operation "
        "that reports progress. The task completes when the update is "
        "finished or fails on error.",
        R"({"type":"object","properties":{"url":{"type":"string","description":"Firmware binary URL"}},"required":["url"]})",
        [](const String& taskId, JsonVariant params) {
            // Start the async firmware update
            fwUpdate.taskId = taskId;
            fwUpdate.progress = 0;
            fwUpdate.active = true;
            fwUpdate.lastStep = millis();

            Serial.printf("Starting firmware update, task: %s\n", taskId.c_str());
        },
        mcpd::TaskSupport::Required  // Must be invoked as a task
    );

    // ── Async calibration tool (with input-required flow) ────────
    // TaskSupport::Optional means it can be called normally or as a task
    mcp.addTaskTool(
        "calibrate_sensor",
        "Calibrate the main sensor. Takes a baseline reading, then asks "
        "the user to confirm or provide an offset adjustment.",
        R"({"type":"object","properties":{"sensor":{"type":"string","enum":["temperature","humidity","pressure"],"description":"Which sensor to calibrate"}},"required":["sensor"]})",
        [](const String& taskId, JsonVariant params) {
            const char* sensor = params["sensor"] | "temperature";
            Serial.printf("Starting calibration for %s, task: %s\n",
                          sensor, taskId.c_str());

            // Simulate taking a baseline reading
            calibration.taskId = taskId;
            calibration.baseline = 22.5f + random(-20, 20) / 10.0f;
            calibration.waitingForInput = true;
            calibration.active = true;

            // Move task to input_required state via TaskManager
            mcp.tasks().updateStatus(taskId, mcpd::TaskStatus::InputRequired,
                String("Baseline reading: ") + String(calibration.baseline, 1) +
                "°C. Please confirm or provide offset adjustment.");
        },
        mcpd::TaskSupport::Optional  // Can be sync or async
    );

    // ── Simple sync tool (for comparison) ────────────────────────
    mcp.addTool(
        "read_temperature",
        "Read current temperature (synchronous).",
        "{}",
        [](JsonVariant) -> String {
            float temp = 22.5f + random(-10, 10) / 10.0f;
            return String("{\"temperature\":") + String(temp, 1) + "}";
        }
    );

    mcp.setInstructions(
        "This device demonstrates async tasks. Use 'firmware_update' to "
        "start a long-running update (must be a task). Use 'calibrate_sensor' "
        "for an interactive calibration flow. 'read_temperature' is a normal "
        "synchronous tool for comparison."
    );

    mcp.begin();
    Serial.println("MCP server with Tasks ready!");
}

void loop() {
    mcp.loop();

    // ── Simulate firmware update progress ────────────────────────
    if (fwUpdate.active && millis() - fwUpdate.lastStep > 500) {
        fwUpdate.lastStep = millis();
        fwUpdate.progress += random(3, 12);

        if (fwUpdate.progress >= 100) {
            fwUpdate.progress = 100;
            fwUpdate.active = false;

            // Complete the task with a result
            mcp.taskComplete(fwUpdate.taskId,
                R"({"content":[{"type":"text","text":"Firmware update complete! Device will reboot in 5 seconds."}]})");
            Serial.println("Firmware update complete!");
        } else {
            // Update status with progress
            mcp.tasks().updateStatus(fwUpdate.taskId, mcpd::TaskStatus::Working,
                String("Downloading... ") + String(fwUpdate.progress) + "%");
        }
    }

    // ── Handle calibration confirmation ──────────────────────────
    // In a real app, the client would send a follow-up message or
    // the user would press a physical button. Here we auto-confirm
    // after a short delay for demonstration.
    if (calibration.active && calibration.waitingForInput) {
        // In production, you'd wait for actual input.
        // For demo, auto-confirm after 3 seconds:
        static unsigned long waitStart = millis();
        if (millis() - waitStart > 3000) {
            calibration.waitingForInput = false;
            calibration.active = false;

            // Apply calibration and complete the task
            mcp.taskComplete(calibration.taskId,
                String(R"({"content":[{"type":"text","text":"Sensor calibrated. Baseline: )") +
                String(calibration.baseline, 1) +
                R"(°C, offset applied: 0.0°C"}]})");
            Serial.println("Calibration complete!");
        }
    }
}
