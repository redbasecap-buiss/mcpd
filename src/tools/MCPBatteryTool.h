/**
 * mcpd — Built-in Battery Monitor Tool
 *
 * Provides: battery_read, battery_status, battery_calibrate, battery_history
 *
 * Monitor battery voltage, percentage, and charging state via ADC.
 * Supports common LiPo (3.7V), LiFePO4 (3.2V), and custom battery
 * configurations with voltage divider compensation.
 *
 * Use cases: portable IoT devices, solar-powered sensors, remote
 * monitoring, power management decisions.
 */

#ifndef MCPD_BATTERY_TOOL_H
#define MCPD_BATTERY_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class BatteryTool {
public:
    struct Config {
        uint8_t adcPin = 34;         // Default ADC pin (ESP32 ADC1)
        float dividerRatio = 2.0f;   // Voltage divider ratio (e.g., 100k+100k = 2.0)
        float vRef = 3.3f;           // ADC reference voltage
        uint16_t adcMax = 4095;      // ADC resolution (12-bit default)
        float vFull = 4.2f;          // Fully charged voltage
        float vEmpty = 3.0f;         // Empty voltage (cutoff)
        String chemistry = "LiPo";   // Battery chemistry label
        uint8_t chargingPin = 255;   // Pin to detect charging (255 = none)
        bool chargingActiveLow = true; // Charging pin active low?
    };

    struct Reading {
        float voltage;
        int percentage;
        bool charging;
        uint16_t rawADC;
        unsigned long timestampMs;
    };

    static Config& config() {
        static Config c;
        return c;
    }

    static std::vector<Reading>& history() {
        static std::vector<Reading> h;
        return h;
    }

    static unsigned long& lastReadMs() {
        static unsigned long l = 0;
        return l;
    }

    static Reading readBattery() {
        Reading r;
        r.timestampMs = millis();

#ifdef ESP32
        // Average multiple readings for stability
        uint32_t sum = 0;
        for (int i = 0; i < 16; i++) {
            sum += analogRead(config().adcPin);
        }
        r.rawADC = sum / 16;
#else
        r.rawADC = analogRead(config().adcPin);
#endif

        r.voltage = ((float)r.rawADC / config().adcMax) * config().vRef * config().dividerRatio;

        // Clamp and calculate percentage with non-linear curve for LiPo
        if (r.voltage >= config().vFull) {
            r.percentage = 100;
        } else if (r.voltage <= config().vEmpty) {
            r.percentage = 0;
        } else {
            // Simple linear mapping (good enough for most use cases)
            float range = config().vFull - config().vEmpty;
            r.percentage = (int)(((r.voltage - config().vEmpty) / range) * 100.0f);
        }

        // Check charging state
        if (config().chargingPin != 255) {
            int chgState = digitalRead(config().chargingPin);
            r.charging = config().chargingActiveLow ? (chgState == LOW) : (chgState == HIGH);
        } else {
            r.charging = false;
        }

        // Store in history (keep last 100 readings)
        history().push_back(r);
        if (history().size() > 100) {
            history().erase(history().begin());
        }
        lastReadMs() = r.timestampMs;

        return r;
    }

    static void attach(Server& server, uint8_t adcPin = 34, float dividerRatio = 2.0f) {
        config().adcPin = adcPin;
        config().dividerRatio = dividerRatio;

        // battery_read — read current voltage and percentage
        server.addTool(
            MCPTool("battery_read", "Read current battery voltage, percentage, and charging state",
                R"({"type":"object","properties":{}})",
                [](const JsonObject&) -> String {
                    Reading r = readBattery();

                    const char* level;
                    if (r.percentage > 75) level = "good";
                    else if (r.percentage > 40) level = "moderate";
                    else if (r.percentage > 15) level = "low";
                    else level = "critical";

                    return String(R"({"voltage":)") + String(r.voltage, 2) +
                           R"(,"percentage":)" + r.percentage +
                           R"(,"level":")" + level +
                           R"(","charging":)" + (r.charging ? "true" : "false") +
                           R"(,"raw_adc":)" + r.rawADC +
                           R"(,"chemistry":")" + config().chemistry +
                           R"(","pin":)" + config().adcPin + "}";
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );

        // battery_status — detailed status with health info
        server.addTool(
            MCPTool("battery_status", "Get detailed battery status including history trend",
                R"({"type":"object","properties":{}})",
                [](const JsonObject&) -> String {
                    Reading r = readBattery();

                    // Calculate trend from history
                    String trend = "stable";
                    if (history().size() >= 5) {
                        float oldV = history()[history().size() - 5].voltage;
                        float diff = r.voltage - oldV;
                        if (diff > 0.05f) trend = "rising";
                        else if (diff < -0.05f) trend = "falling";
                    }

                    // Estimate runtime (very rough)
                    String runtime = "unknown";
                    if (history().size() >= 10 && !r.charging) {
                        float oldV = history()[0].voltage;
                        unsigned long oldT = history()[0].timestampMs;
                        float drainRate = (oldV - r.voltage) / ((r.timestampMs - oldT) / 3600000.0f); // V/h
                        if (drainRate > 0.001f) {
                            float hoursLeft = (r.voltage - config().vEmpty) / drainRate;
                            if (hoursLeft > 0 && hoursLeft < 1000) {
                                runtime = String(hoursLeft, 1) + "h";
                            }
                        }
                    }

                    return String(R"({"voltage":)") + String(r.voltage, 2) +
                           R"(,"percentage":)" + r.percentage +
                           R"(,"charging":)" + (r.charging ? "true" : "false") +
                           R"(,"trend":")" + trend +
                           R"(","estimated_runtime":")" + runtime +
                           R"(","readings_stored":)" + history().size() +
                           R"(,"config":{"pin":)" + config().adcPin +
                           R"(,"divider_ratio":)" + String(config().dividerRatio, 1) +
                           R"(,"v_full":)" + String(config().vFull, 1) +
                           R"(,"v_empty":)" + String(config().vEmpty, 1) +
                           R"(,"chemistry":")" + config().chemistry + R"("}})";
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );

        // battery_calibrate — update voltage mapping
        server.addTool(
            MCPTool("battery_calibrate", "Calibrate battery monitoring parameters",
                R"({"type":"object","properties":{"v_full":{"type":"number","description":"Voltage at full charge"},"v_empty":{"type":"number","description":"Voltage at empty"},"divider_ratio":{"type":"number","description":"Voltage divider ratio"},"v_ref":{"type":"number","description":"ADC reference voltage"},"chemistry":{"type":"string","description":"Battery chemistry label"},"charging_pin":{"type":"integer","description":"GPIO pin for charging detection, 255 to disable"},"charging_active_low":{"type":"boolean","description":"Is charging pin active low"}}})",
                [](const JsonObject& args) -> String {
                    if (args["v_full"].is<float>()) config().vFull = args["v_full"].as<float>();
                    if (args["v_empty"].is<float>()) config().vEmpty = args["v_empty"].as<float>();
                    if (args["divider_ratio"].is<float>()) config().dividerRatio = args["divider_ratio"].as<float>();
                    if (args["v_ref"].is<float>()) config().vRef = args["v_ref"].as<float>();
                    if (args["chemistry"].is<const char*>()) config().chemistry = args["chemistry"].as<const char*>();
                    if (args["charging_pin"].is<int>()) {
                        config().chargingPin = args["charging_pin"].as<int>();
                        if (config().chargingPin != 255) {
                            pinMode(config().chargingPin, INPUT_PULLUP);
                        }
                    }
                    if (args["charging_active_low"].is<bool>()) config().chargingActiveLow = args["charging_active_low"].as<bool>();

                    // Clear history since calibration changed
                    history().clear();

                    return String(R"({"calibrated":true,"config":{)") +
                           R"("v_full":)" + String(config().vFull, 2) +
                           R"(,"v_empty":)" + String(config().vEmpty, 2) +
                           R"(,"divider_ratio":)" + String(config().dividerRatio, 2) +
                           R"(,"v_ref":)" + String(config().vRef, 2) +
                           R"(,"chemistry":")" + config().chemistry +
                           R"(","charging_pin":)" + config().chargingPin +
                           R"(,"charging_active_low":)" + (config().chargingActiveLow ? "true" : "false") +
                           "}}";
                }
            ).annotate(MCPToolAnnotations().setIdempotentHint(true))
        );

        // battery_history — recent readings
        server.addTool(
            MCPTool("battery_history", "Get recent battery reading history for trend analysis",
                R"({"type":"object","properties":{"count":{"type":"integer","description":"Number of recent readings, max 100","default":10}}})",
                [](const JsonObject& args) -> String {
                    int count = args["count"] | 10;
                    if (count > 100) count = 100;
                    if (count > (int)history().size()) count = history().size();

                    String result = R"({"readings":[)";
                    bool first = true;
                    int start = history().size() - count;
                    if (start < 0) start = 0;

                    for (int i = start; i < (int)history().size(); i++) {
                        if (!first) result += ",";
                        first = false;
                        result += String(R"({"v":)") + String(history()[i].voltage, 2) +
                                  R"(,"pct":)" + history()[i].percentage +
                                  R"(,"ms":)" + history()[i].timestampMs + "}";
                    }
                    result += String(R"(],"count":)") + count + "}";
                    return result;
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );
    }
};

inline void addBatteryTools(Server& server, uint8_t adcPin = 34, float dividerRatio = 2.0f) {
    BatteryTool::attach(server, adcPin, dividerRatio);
}

} // namespace tools
} // namespace mcpd

#endif // MCPD_BATTERY_TOOL_H
