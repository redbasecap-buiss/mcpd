/**
 * mcpd Robot Arm Example
 *
 * Claude controls a servo-based robot arm via MCP tools.
 * Supports 4-DOF: base rotation, shoulder, elbow, gripper.
 *
 * Hardware:
 *   - Base servo   → GPIO 13
 *   - Shoulder     → GPIO 12
 *   - Elbow        → GPIO 14
 *   - Gripper      → GPIO 27
 *
 * Claude can say: "Pick up the object at position (90, 45, 30)"
 * and the arm will move accordingly.
 */

#include <WiFi.h>
#include <mcpd.h>
#include <ESP32Servo.h>

// ── Config ─────────────────────────────────────────────────────────
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// ── Servo pins ─────────────────────────────────────────────────────
#define PIN_BASE     13
#define PIN_SHOULDER 12
#define PIN_ELBOW    14
#define PIN_GRIPPER  27

// ── Servo limits (degrees) ─────────────────────────────────────────
struct JointConfig {
    const char* name;
    uint8_t pin;
    int minAngle;
    int maxAngle;
    int homeAngle;
    int currentAngle;
    Servo servo;
};

JointConfig joints[] = {
    {"base",     PIN_BASE,     0, 180, 90, 90, Servo()},
    {"shoulder", PIN_SHOULDER, 15, 165, 90, 90, Servo()},
    {"elbow",    PIN_ELBOW,    0, 135, 45, 45, Servo()},
    {"gripper",  PIN_GRIPPER,  10, 73, 73, 73, Servo()},  // 73=open, 10=closed
};
const int NUM_JOINTS = sizeof(joints) / sizeof(joints[0]);

mcpd::Server mcp("robot-arm");

// ── Smooth move (interpolate to target) ────────────────────────────
void smoothMove(JointConfig& joint, int targetAngle, int speedMs = 15) {
    targetAngle = constrain(targetAngle, joint.minAngle, joint.maxAngle);
    int current = joint.currentAngle;
    int step = (targetAngle > current) ? 1 : -1;

    while (current != targetAngle) {
        current += step;
        joint.servo.write(current);
        delay(speedMs);
    }
    joint.currentAngle = targetAngle;
}

JointConfig* findJoint(const char* name) {
    for (int i = 0; i < NUM_JOINTS; i++) {
        if (strcmp(joints[i].name, name) == 0) return &joints[i];
    }
    return nullptr;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[robot_arm] Starting...");

    // Attach servos
    for (int i = 0; i < NUM_JOINTS; i++) {
        joints[i].servo.attach(joints[i].pin);
        joints[i].servo.write(joints[i].homeAngle);
        joints[i].currentAngle = joints[i].homeAngle;
    }
    Serial.println("[robot_arm] Servos initialized at home position");

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\n[robot_arm] IP: %s\n", WiFi.localIP().toString().c_str());

    // ── MCP Tool: move a single joint ──────────────────────────────

    mcp.addTool(
        "move_joint",
        "Move a single joint to a target angle. Joints: base (0-180), shoulder (15-165), elbow (0-135), gripper (10=closed, 73=open)",
        R"({"type":"object","properties":{"joint":{"type":"string","enum":["base","shoulder","elbow","gripper"],"description":"Joint name"},"angle":{"type":"integer","description":"Target angle in degrees"},"speed":{"type":"integer","description":"Movement speed (ms per degree, default 15)"}},"required":["joint","angle"]})",
        [](const JsonObject& args) -> String {
            const char* name = args["joint"];
            int angle = args["angle"];
            int speed = args["speed"] | 15;

            JointConfig* joint = findJoint(name);
            if (!joint) return "{\"error\":\"Unknown joint: " + String(name) + "\"}";

            smoothMove(*joint, angle, speed);
            return "{\"joint\":\"" + String(name) + "\",\"angle\":" + String(joint->currentAngle) +
                   ",\"status\":\"ok\"}";
        }
    );

    // ── MCP Tool: move all joints at once ──────────────────────────

    mcp.addTool(
        "move_arm",
        "Move all arm joints simultaneously. Omit a joint to keep its current position.",
        R"({"type":"object","properties":{"base":{"type":"integer","description":"Base rotation (0-180)"},"shoulder":{"type":"integer","description":"Shoulder angle (15-165)"},"elbow":{"type":"integer","description":"Elbow angle (0-135)"},"gripper":{"type":"integer","description":"Gripper (10=closed, 73=open)"},"speed":{"type":"integer","description":"Movement speed (ms/deg, default 15)"}}})",
        [](const JsonObject& args) -> String {
            int speed = args["speed"] | 15;
            String result = "{\"positions\":{";
            bool first = true;

            for (int i = 0; i < NUM_JOINTS; i++) {
                if (args.containsKey(joints[i].name)) {
                    int angle = args[joints[i].name].as<int>();
                    smoothMove(joints[i], angle, speed);
                }
                if (!first) result += ",";
                result += "\"" + String(joints[i].name) + "\":" + String(joints[i].currentAngle);
                first = false;
            }
            result += "},\"status\":\"ok\"}";
            return result;
        }
    );

    // ── MCP Tool: home position ────────────────────────────────────

    mcp.addTool(
        "home",
        "Move all joints to their home/neutral position",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            for (int i = 0; i < NUM_JOINTS; i++) {
                smoothMove(joints[i], joints[i].homeAngle);
            }
            return "{\"status\":\"ok\",\"position\":\"home\"}";
        }
    );

    // ── MCP Tool: gripper shortcuts ────────────────────────────────

    mcp.addTool(
        "grip",
        "Open or close the gripper",
        R"({"type":"object","properties":{"action":{"type":"string","enum":["open","close"],"description":"Open or close the gripper"}},"required":["action"]})",
        [](const JsonObject& args) -> String {
            const char* action = args["action"];
            JointConfig* gripper = findJoint("gripper");
            if (strcmp(action, "close") == 0) {
                smoothMove(*gripper, gripper->minAngle, 10);
            } else {
                smoothMove(*gripper, gripper->maxAngle, 10);
            }
            return "{\"gripper\":\"" + String(action) + "\",\"angle\":" +
                   String(gripper->currentAngle) + "}";
        }
    );

    // ── MCP Resource: current arm state ────────────────────────────

    mcp.addResource(
        "robot://arm/state", "Arm State",
        "Current position of all robot arm joints",
        "application/json",
        []() {
            String json = "{\"joints\":{";
            for (int i = 0; i < NUM_JOINTS; i++) {
                if (i > 0) json += ",";
                json += "\"" + String(joints[i].name) + "\":{";
                json += "\"angle\":" + String(joints[i].currentAngle);
                json += ",\"min\":" + String(joints[i].minAngle);
                json += ",\"max\":" + String(joints[i].maxAngle);
                json += "}";
            }
            json += "}}";
            return json;
        }
    );

    mcp.begin();
    Serial.println("[robot_arm] MCP server ready! Claude can now control the arm.");
}

void loop() {
    mcp.loop();
}
