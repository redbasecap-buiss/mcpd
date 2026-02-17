/**
 * mcpd — Built-in WiFi Tools
 *
 * Provides: wifi_status, wifi_scan
 */

#ifndef MCPD_WIFI_TOOL_H
#define MCPD_WIFI_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class WiFiTool {
public:
    static void attach(Server& server) {
        server.addTool("wifi_status",
            "Get current WiFi connection status",
            R"({"type":"object","properties":{}})",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
                doc["connected"] = (WiFi.status() == WL_CONNECTED);
                doc["ssid"] = WiFi.SSID();
                doc["ip"] = WiFi.localIP().toString();
                doc["gateway"] = WiFi.gatewayIP().toString();
                doc["subnet"] = WiFi.subnetMask().toString();
                doc["dns"] = WiFi.dnsIP().toString();
                doc["rssi"] = WiFi.RSSI();
                doc["mac"] = WiFi.macAddress();
                doc["channel"] = WiFi.channel();

                String result;
                serializeJson(doc, result);
                return result;
            });

        server.addTool("wifi_scan",
            "Scan for available WiFi networks",
            R"({"type":"object","properties":{}})",
            [](const JsonObject& args) -> String {
                int n = WiFi.scanNetworks();
                JsonDocument doc;
                JsonArray networks = doc["networks"].to<JsonArray>();

                for (int i = 0; i < n && i < 20; i++) {
                    JsonObject net = networks.add<JsonObject>();
                    net["ssid"] = WiFi.SSID(i);
                    net["rssi"] = WiFi.RSSI(i);
                    net["channel"] = WiFi.channel(i);
                    net["encryption"] = WiFi.encryptionType(i);
                }
                doc["count"] = n;

                WiFi.scanDelete();

                String result;
                serializeJson(doc, result);
                return result;
            });
    }
};

} // namespace tools
} // namespace mcpd

#endif // MCPD_WIFI_TOOL_H
