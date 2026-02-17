/**
 * mcpd — Configuration Persistence & Captive Portal
 *
 * Stores WiFi credentials and server config in NVS (Non-Volatile Storage).
 * Provides a captive portal for initial WiFi setup (WiFiManager-style).
 *
 * Usage:
 *   mcpd::Config config;
 *   if (!config.hasWiFiCredentials()) {
 *       config.startCaptivePortal("mcpd-setup");
 *       // blocks until configured or timeout
 *   }
 *   config.connectWiFi();
 */

#ifndef MCPD_CONFIG_H
#define MCPD_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

namespace mcpd {

struct ConfigData {
    String wifiSSID;
    String wifiPassword;
    String serverName;
    uint16_t serverPort = 80;
    String apiKey;
    String mcpEndpoint = "/mcp";
};

class Config {
public:
    static constexpr const char* NVS_NAMESPACE = "mcpd";
    static constexpr unsigned long PORTAL_TIMEOUT_MS = 180000; // 3 minutes

    Config() = default;

    /**
     * Load configuration from NVS.
     * @return true if valid config was found
     */
    bool load() {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, true); // read-only

        _data.wifiSSID = prefs.getString("wifi_ssid", "");
        _data.wifiPassword = prefs.getString("wifi_pass", "");
        _data.serverName = prefs.getString("srv_name", "mcpd");
        _data.serverPort = prefs.getUShort("srv_port", 80);
        _data.apiKey = prefs.getString("api_key", "");
        _data.mcpEndpoint = prefs.getString("endpoint", "/mcp");

        prefs.end();
        return _data.wifiSSID.length() > 0;
    }

    /**
     * Save current configuration to NVS.
     */
    void save() {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);

        prefs.putString("wifi_ssid", _data.wifiSSID);
        prefs.putString("wifi_pass", _data.wifiPassword);
        prefs.putString("srv_name", _data.serverName);
        prefs.putUShort("srv_port", _data.serverPort);
        prefs.putString("api_key", _data.apiKey);
        prefs.putString("endpoint", _data.mcpEndpoint);

        prefs.end();
        Serial.println("[mcpd] Config saved to NVS");
    }

    /**
     * Clear all stored configuration.
     */
    void clear() {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.clear();
        prefs.end();
        _data = ConfigData();
        Serial.println("[mcpd] Config cleared");
    }

    bool hasWiFiCredentials() const {
        return _data.wifiSSID.length() > 0;
    }

    /**
     * Connect to WiFi using stored credentials.
     * @param timeoutMs  Connection timeout
     * @return true if connected
     */
    bool connectWiFi(unsigned long timeoutMs = 15000) {
        if (!hasWiFiCredentials()) return false;

        WiFi.mode(WIFI_STA);
        WiFi.begin(_data.wifiSSID.c_str(), _data.wifiPassword.c_str());

        Serial.printf("[mcpd] Connecting to '%s'", _data.wifiSSID.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[mcpd] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }

        Serial.println("[mcpd] WiFi connection failed");
        return false;
    }

    /**
     * Start a captive portal for WiFi configuration.
     * Creates an AP and serves a setup page. Blocks until configured or timeout.
     *
     * @param apName    Access point name
     * @param apPass    Optional AP password (NULL for open)
     * @return true if WiFi was configured
     */
    bool startCaptivePortal(const char* apName = "mcpd-setup",
                            const char* apPass = NULL) {
        Serial.println("[mcpd] Starting captive portal...");

        WiFi.mode(WIFI_AP);
        if (apPass && strlen(apPass) >= 8) {
            WiFi.softAP(apName, apPass);
        } else {
            WiFi.softAP(apName);
        }

        IPAddress apIP = WiFi.softAPIP();
        Serial.printf("[mcpd] AP started: %s @ %s\n", apName, apIP.toString().c_str());

        DNSServer dnsServer;
        WebServer portalServer(80);

        // Capture all DNS queries → redirect to our IP
        dnsServer.start(53, "*", apIP);

        bool configured = false;

        // Serve the setup page
        portalServer.on("/", HTTP_GET, [&portalServer]() {
            portalServer.send(200, "text/html", _portalHTML());
        });

        portalServer.on("/save", HTTP_POST, [&]() {
            _data.wifiSSID = portalServer.arg("ssid");
            _data.wifiPassword = portalServer.arg("pass");
            String name = portalServer.arg("name");
            if (name.length() > 0) _data.serverName = name;
            String key = portalServer.arg("apikey");
            if (key.length() > 0) _data.apiKey = key;

            save();
            configured = true;

            portalServer.send(200, "text/html",
                "<html><body style='font-family:sans-serif;text-align:center;padding:40px'>"
                "<h2>&#10004; Saved!</h2><p>mcpd will now restart and connect to <b>"
                + _data.wifiSSID + "</b></p></body></html>");
        });

        // Captive portal detection endpoints
        portalServer.onNotFound([&portalServer, &apIP]() {
            portalServer.sendHeader("Location", "http://" + apIP.toString() + "/");
            portalServer.send(302);
        });

        portalServer.begin();

        unsigned long start = millis();
        while (!configured && millis() - start < PORTAL_TIMEOUT_MS) {
            dnsServer.processNextRequest();
            portalServer.handleClient();
            delay(10);
        }

        portalServer.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);

        return configured;
    }

    ConfigData& data() { return _data; }
    const ConfigData& data() const { return _data; }

private:
    ConfigData _data;

    static String _portalHTML() {
        return R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>mcpd Setup</title>
<style>
  body { font-family: -apple-system, sans-serif; max-width: 400px; margin: 40px auto; padding: 20px; background: #0d1117; color: #e6edf3; }
  h1 { color: #58a6ff; font-size: 1.5em; }
  .logo { font-size: 2em; margin-bottom: 10px; }
  input, select { width: 100%; padding: 10px; margin: 6px 0 16px; border: 1px solid #30363d; border-radius: 6px; background: #161b22; color: #e6edf3; box-sizing: border-box; font-size: 14px; }
  label { font-weight: 600; font-size: 0.9em; color: #8b949e; }
  button { width: 100%; padding: 12px; background: #238636; color: white; border: none; border-radius: 6px; cursor: pointer; font-size: 16px; font-weight: 600; }
  button:hover { background: #2ea043; }
  .info { font-size: 0.85em; color: #8b949e; margin-top: 20px; text-align: center; }
</style>
</head><body>
<div style="text-align:center"><div class="logo">⚡</div><h1>mcpd Setup</h1></div>
<form action="/save" method="POST">
  <label>WiFi Network (SSID)</label>
  <input name="ssid" required placeholder="Your WiFi name">
  <label>WiFi Password</label>
  <input name="pass" type="password" placeholder="WiFi password">
  <label>Device Name</label>
  <input name="name" placeholder="mcpd" value="mcpd">
  <label>API Key (optional)</label>
  <input name="apikey" placeholder="Leave empty to disable auth">
  <button type="submit">Save &amp; Connect</button>
</form>
<div class="info">mcpd — MCP Server for Microcontrollers</div>
</body></html>
)rawliteral";
    }
};

} // namespace mcpd

#endif // MCPD_CONFIG_H
