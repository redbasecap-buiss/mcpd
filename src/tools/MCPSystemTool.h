/**
 * mcpd — Built-in System Info Tool
 *
 * Provides: system_info
 */

#ifndef MCPD_SYSTEM_TOOL_H
#define MCPD_SYSTEM_TOOL_H

#include "../mcpd.h"

namespace mcpd {
namespace tools {

class SystemTool {
public:
    static void attach(Server& server) {
        server.addTool("system_info",
            "Get system information: free heap, uptime, chip model, WiFi IP",
            R"({"type":"object","properties":{}})",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
                doc["freeHeap"] = ESP.getFreeHeap();
                doc["heapSize"] = ESP.getHeapSize();
                doc["uptimeMs"] = millis();
                doc["chipModel"] = ESP.getChipModel();
                doc["chipRevision"] = ESP.getChipRevision();
                doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
                doc["flashSize"] = ESP.getFlashChipSize();
                doc["ip"] = WiFi.localIP().toString();
                doc["mac"] = WiFi.macAddress();
                doc["rssi"] = WiFi.RSSI();

                String result;
                serializeJson(doc, result);
                return result;
            });
    }
};

} // namespace tools
} // namespace mcpd

#endif // MCPD_SYSTEM_TOOL_H
