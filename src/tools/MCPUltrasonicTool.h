/**
 * mcpd — Built-in Ultrasonic Distance Sensor Tool (HC-SR04 / JSN-SR04T)
 *
 * Provides: distance_read, distance_read_multi, distance_config
 *
 * Supports HC-SR04 (2cm–400cm) and compatible sensors.
 * Uses pulse timing on echo pin for distance calculation.
 * Includes temperature-compensated speed of sound.
 */

#ifndef MCPD_ULTRASONIC_TOOL_H
#define MCPD_ULTRASONIC_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class UltrasonicTool {
public:
    struct Config {
        int trigPin = -1;
        int echoPin = -1;
        float tempC = 20.0f;        // ambient temperature for speed-of-sound correction
        float maxDistanceCm = 400.0f;
        unsigned long timeoutUs = 25000; // ~4.3m at 20°C
        const char* label = nullptr;
    };

    static Config configs[4];
    static int configCount;

    static float speedOfSound(float tempC) {
        // v = 331.3 + 0.606 * T (m/s)
        return 331.3f + 0.606f * tempC;
    }

    static float measureCm(const Config& cfg) {
        digitalWrite(cfg.trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(cfg.trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(cfg.trigPin, LOW);

        unsigned long duration = pulseIn(cfg.echoPin, HIGH, cfg.timeoutUs);
        if (duration == 0) return -1.0f; // timeout = no echo

        float v = speedOfSound(cfg.tempC); // m/s
        float distCm = (duration * v) / 20000.0f; // /2 for round-trip, /10000 for cm

        if (distCm > cfg.maxDistanceCm) return -1.0f;
        return distCm;
    }

    static void attach(Server& server, int trigPin, int echoPin,
                       float maxDistanceCm = 400.0f, const char* label = nullptr) {
        if (configCount >= 4) return;
        int idx = configCount++;
        configs[idx].trigPin = trigPin;
        configs[idx].echoPin = echoPin;
        configs[idx].maxDistanceCm = maxDistanceCm;
        configs[idx].timeoutUs = (unsigned long)(maxDistanceCm * 58.0f * 1.2f); // with margin
        configs[idx].label = label;

        pinMode(trigPin, OUTPUT);
        pinMode(echoPin, INPUT);

        if (idx == 0) registerTools(server);
    }

    static void registerTools(Server& server) {
        // distance_read — single distance measurement
        server.addTool("distance_read", "Read distance from ultrasonic sensor (HC-SR04). Returns cm, inches, and meters.",
            R"=({"type":"object","properties":{"index":{"type":"integer","minimum":0,"maximum":3,"description":"Sensor index (default 0)"},"samples":{"type":"integer","minimum":1,"maximum":10,"description":"Number of readings to average (default 3, filters outliers)"},"temperature":{"type":"number","description":"Ambient temperature °C for speed-of-sound correction (default 20)"}},"required":[]})=",
            [](const JsonObject& args) -> String {
                int idx = args["index"] | 0;
                int samples = args["samples"] | 3;
                float temp = args["temperature"] | -999.0f;

                if (idx < 0 || idx >= configCount)
                    return R"=({"error":"Invalid sensor index"})=";

                Config& cfg = configs[idx];
                if (temp > -273.0f) cfg.tempC = temp;

                // Collect samples, discard outliers
                float readings[10];
                int validCount = 0;
                for (int i = 0; i < samples && i < 10; i++) {
                    float d = measureCm(cfg);
                    if (d >= 0) readings[validCount++] = d;
                    if (i < samples - 1) delay(30); // min 30ms between triggers
                }

                if (validCount == 0)
                    return R"=({"error":"No echo received — object out of range or sensor disconnected"})=";

                // Simple average (could do median for more robustness)
                float sum = 0;
                float minD = 99999, maxD = 0;
                for (int i = 0; i < validCount; i++) {
                    sum += readings[i];
                    if (readings[i] < minD) minD = readings[i];
                    if (readings[i] > maxD) maxD = readings[i];
                }
                float avg = sum / validCount;

                char cmBuf[16], inBuf[16], mBuf[16];
                snprintf(cmBuf, sizeof(cmBuf), "%.1f", avg);
                snprintf(inBuf, sizeof(inBuf), "%.1f", avg / 2.54f);
                snprintf(mBuf, sizeof(mBuf), "%.3f", avg / 100.0f);

                String result = String("{\"index\":") + idx;
                if (cfg.label) result += String(",\"label\":\"") + cfg.label + "\"";
                result += String(",\"distance_cm\":") + cmBuf +
                          ",\"distance_in\":" + inBuf +
                          ",\"distance_m\":" + mBuf +
                          ",\"samples_taken\":" + samples +
                          ",\"samples_valid\":" + validCount;
                if (validCount > 1) {
                    char mnBuf[16], mxBuf[16];
                    snprintf(mnBuf, sizeof(mnBuf), "%.1f", minD);
                    snprintf(mxBuf, sizeof(mxBuf), "%.1f", maxD);
                    result += String(",\"min_cm\":") + mnBuf + ",\"max_cm\":" + mxBuf;
                }
                result += String(",\"speed_of_sound_mps\":") +
                          String(speedOfSound(cfg.tempC), 1) +
                          ",\"temperature_c\":" + String(cfg.tempC, 1) + "}";
                return result;
            });

        // distance_read_multi — read all registered sensors at once
        server.addTool("distance_read_multi", "Read distance from all registered ultrasonic sensors",
            R"=({"type":"object","properties":{"samples":{"type":"integer","minimum":1,"maximum":5,"description":"Samples per sensor (default 3)"}},"required":[]})=",
            [](const JsonObject& args) -> String {
                int samples = args["samples"] | 3;

                String result = "{\"sensors\":[";
                for (int i = 0; i < configCount; i++) {
                    if (i > 0) result += ",";
                    Config& cfg = configs[i];

                    float sum = 0;
                    int valid = 0;
                    for (int s = 0; s < samples; s++) {
                        float d = measureCm(cfg);
                        if (d >= 0) { sum += d; valid++; }
                        if (s < samples - 1) delay(30);
                    }

                    result += String("{\"index\":") + i;
                    if (cfg.label) result += String(",\"label\":\"") + cfg.label + "\"";
                    if (valid > 0) {
                        char cmBuf[16];
                        snprintf(cmBuf, sizeof(cmBuf), "%.1f", sum / valid);
                        result += String(",\"distance_cm\":") + cmBuf +
                                  ",\"valid\":" + valid;
                    } else {
                        result += ",\"distance_cm\":null,\"error\":\"no echo\"";
                    }
                    result += "}";
                }
                result += "],\"count\":" + String(configCount) + "}";
                return result;
            });

        // distance_config — update sensor configuration
        server.addTool("distance_config", "Configure ultrasonic sensor parameters",
            R"=({"type":"object","properties":{"index":{"type":"integer","minimum":0,"maximum":3,"description":"Sensor index (default 0)"},"temperature":{"type":"number","description":"Ambient temperature °C for speed-of-sound correction"},"max_distance_cm":{"type":"number","minimum":2,"maximum":600,"description":"Maximum detection distance in cm"}},"required":[]})=",
            [](const JsonObject& args) -> String {
                int idx = args["index"] | 0;
                if (idx < 0 || idx >= configCount)
                    return R"=({"error":"Invalid sensor index"})=";

                Config& cfg = configs[idx];
                bool changed = false;

                if (args.containsKey("temperature")) {
                    cfg.tempC = args["temperature"];
                    changed = true;
                }
                if (args.containsKey("max_distance_cm")) {
                    cfg.maxDistanceCm = args["max_distance_cm"];
                    cfg.timeoutUs = (unsigned long)(cfg.maxDistanceCm * 58.0f * 1.2f);
                    changed = true;
                }

                return String("{\"index\":") + idx +
                       ",\"configured\":" + (changed ? "true" : "false") +
                       ",\"temperature_c\":" + String(cfg.tempC, 1) +
                       ",\"max_distance_cm\":" + String(cfg.maxDistanceCm, 0) +
                       ",\"timeout_us\":" + cfg.timeoutUs + "}";
            });
    }
};

// Static member definitions
UltrasonicTool::Config UltrasonicTool::configs[4];
int UltrasonicTool::configCount = 0;

} // namespace tools

/**
 * Register ultrasonic distance sensor tools.
 *
 * @param server          The mcpd::Server instance
 * @param trigPin         Trigger pin (OUTPUT)
 * @param echoPin         Echo pin (INPUT)
 * @param maxDistanceCm   Maximum distance in cm (default 400)
 * @param label           Optional human-readable label
 */
inline void addUltrasonicTools(Server& server, int trigPin, int echoPin,
                                float maxDistanceCm = 400.0f,
                                const char* label = nullptr) {
    tools::UltrasonicTool::attach(server, trigPin, echoPin, maxDistanceCm, label);
}

} // namespace mcpd

#endif // MCPD_ULTRASONIC_TOOL_H
