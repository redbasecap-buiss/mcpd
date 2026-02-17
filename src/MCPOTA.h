/**
 * mcpd — OTA (Over-The-Air) Updates
 *
 * ArduinoOTA integration for mcpd. Enables firmware updates over WiFi.
 *
 * Usage:
 *   mcpd::OTA ota;
 *   ota.begin("mcpd-device");  // call after WiFi connected
 *   // In loop():
 *   ota.loop();
 */

#ifndef MCPD_OTA_H
#define MCPD_OTA_H

#include <Arduino.h>
#include <ArduinoOTA.h>

namespace mcpd {

class OTA {
public:
    OTA() = default;

    /**
     * Start OTA service.
     *
     * @param hostname  mDNS hostname for OTA discovery
     * @param password  Optional OTA password (NULL for none)
     * @param port      OTA port (default 3232)
     */
    void begin(const char* hostname = "mcpd", const char* password = NULL,
               uint16_t port = 3232) {
        ArduinoOTA.setHostname(hostname);
        ArduinoOTA.setPort(port);

        if (password && strlen(password) > 0) {
            ArduinoOTA.setPassword(password);
        }

        ArduinoOTA.onStart([this]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH)
                              ? "firmware"
                              : "filesystem";
            Serial.printf("[mcpd] OTA update started: %s\n", type.c_str());
            if (_onStart) _onStart();
        });

        ArduinoOTA.onEnd([this]() {
            Serial.println("\n[mcpd] OTA update complete!");
            if (_onEnd) _onEnd();
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            unsigned int pct = progress / (total / 100);
            Serial.printf("\r[mcpd] OTA progress: %u%%", pct);
            if (_onProgress) _onProgress(pct);
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            const char* msg = "Unknown";
            switch (error) {
                case OTA_AUTH_ERROR:    msg = "Auth Failed"; break;
                case OTA_BEGIN_ERROR:   msg = "Begin Failed"; break;
                case OTA_CONNECT_ERROR: msg = "Connect Failed"; break;
                case OTA_RECEIVE_ERROR: msg = "Receive Failed"; break;
                case OTA_END_ERROR:     msg = "End Failed"; break;
            }
            Serial.printf("[mcpd] OTA Error: %s\n", msg);
            if (_onError) _onError(error, msg);
        });

        ArduinoOTA.begin();
        _enabled = true;
        Serial.printf("[mcpd] OTA enabled on port %d\n", port);
    }

    /**
     * Process OTA. Call in loop().
     */
    void loop() {
        if (_enabled) {
            ArduinoOTA.handle();
        }
    }

    bool isEnabled() const { return _enabled; }

    // Callbacks
    void onStart(std::function<void()> cb) { _onStart = cb; }
    void onEnd(std::function<void()> cb) { _onEnd = cb; }
    void onProgress(std::function<void(unsigned int pct)> cb) { _onProgress = cb; }
    void onError(std::function<void(ota_error_t, const char*)> cb) { _onError = cb; }

private:
    bool _enabled = false;
    std::function<void()> _onStart;
    std::function<void()> _onEnd;
    std::function<void(unsigned int)> _onProgress;
    std::function<void(ota_error_t, const char*)> _onError;
};

} // namespace mcpd

#endif // MCPD_OTA_H
