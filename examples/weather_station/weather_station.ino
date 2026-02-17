/**
 * mcpd Weather Station Example
 *
 * Exposes DHT22 temperature/humidity and BMP280 pressure/altitude
 * as MCP resources that Claude can read.
 *
 * Hardware:
 *   - DHT22 on GPIO 4
 *   - BMP280 on I2C (SDA=21, SCL=22)
 *
 * Claude can ask: "What's the current temperature?" and read
 * the sensor resources directly.
 */

#include <WiFi.h>
#include <mcpd.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// ── Config ─────────────────────────────────────────────────────────
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

#define DHT_PIN    4
#define DHT_TYPE   DHT22

// ── Globals ────────────────────────────────────────────────────────
mcpd::Server mcp("weather-station");
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP280 bmp;

bool bmpAvailable = false;

// ── Sensor reading cache (avoid reading too frequently) ────────────
struct SensorData {
    float temperature = NAN;
    float humidity = NAN;
    float pressure = NAN;
    float altitude = NAN;
    unsigned long lastUpdate = 0;
};
SensorData sensors;

void updateSensors() {
    if (millis() - sensors.lastUpdate < 2000) return; // min 2s between reads

    sensors.temperature = dht.readTemperature();
    sensors.humidity = dht.readHumidity();

    if (bmpAvailable) {
        sensors.pressure = bmp.readPressure() / 100.0F; // hPa
        sensors.altitude = bmp.readAltitude(1013.25);    // standard pressure
    }

    sensors.lastUpdate = millis();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[weather_station] Starting...");

    // Init sensors
    dht.begin();
    Wire.begin();
    bmpAvailable = bmp.begin(0x76); // or 0x77
    if (!bmpAvailable) {
        Serial.println("[weather_station] BMP280 not found, continuing without pressure");
    }

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[weather_station] IP: %s\n", WiFi.localIP().toString().c_str());

    // ── MCP Resources ──────────────────────────────────────────────

    mcp.addResource(
        "weather://temperature", "Temperature",
        "Current temperature in °C from DHT22 sensor",
        "application/json",
        []() {
            updateSensors();
            return "{\"value\":" + String(sensors.temperature, 1) +
                   ",\"unit\":\"°C\",\"sensor\":\"DHT22\"}";
        }
    );

    mcp.addResource(
        "weather://humidity", "Humidity",
        "Current relative humidity from DHT22 sensor",
        "application/json",
        []() {
            updateSensors();
            return "{\"value\":" + String(sensors.humidity, 1) +
                   ",\"unit\":\"%\",\"sensor\":\"DHT22\"}";
        }
    );

    if (bmpAvailable) {
        mcp.addResource(
            "weather://pressure", "Barometric Pressure",
            "Current barometric pressure in hPa from BMP280",
            "application/json",
            []() {
                updateSensors();
                return "{\"value\":" + String(sensors.pressure, 1) +
                       ",\"unit\":\"hPa\",\"sensor\":\"BMP280\"}";
            }
        );

        mcp.addResource(
            "weather://altitude", "Altitude",
            "Estimated altitude based on pressure (reference: 1013.25 hPa)",
            "application/json",
            []() {
                updateSensors();
                return "{\"value\":" + String(sensors.altitude, 1) +
                       ",\"unit\":\"m\",\"sensor\":\"BMP280\"}";
            }
        );
    }

    // ── MCP Tool: get all readings at once ─────────────────────────

    mcp.addTool(
        "get_weather",
        "Get all weather station readings (temperature, humidity, pressure, altitude)",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& args) -> String {
            updateSensors();
            String json = "{";
            json += "\"temperature_c\":" + String(sensors.temperature, 1);
            json += ",\"humidity_pct\":" + String(sensors.humidity, 1);
            if (bmpAvailable) {
                json += ",\"pressure_hpa\":" + String(sensors.pressure, 1);
                json += ",\"altitude_m\":" + String(sensors.altitude, 1);
            }
            json += ",\"timestamp_ms\":" + String(millis());
            json += "}";
            return json;
        }
    );

    // ── MCP Tool: set altitude reference pressure ──────────────────

    if (bmpAvailable) {
        mcp.addTool(
            "set_reference_pressure",
            "Set the sea-level reference pressure for altitude calculation",
            R"({"type":"object","properties":{"pressure_hpa":{"type":"number","description":"Sea-level pressure in hPa (default 1013.25)"}},"required":["pressure_hpa"]})",
            [](const JsonObject& args) -> String {
                // Note: Adafruit_BMP280 uses the parameter in readAltitude()
                // In a real implementation you'd store this value
                float ref = args["pressure_hpa"].as<float>();
                return "{\"status\":\"ok\",\"reference_hpa\":" + String(ref, 2) + "}";
            }
        );
    }

    mcp.begin();
    Serial.println("[weather_station] MCP server ready!");
}

void loop() {
    mcp.loop();
}
