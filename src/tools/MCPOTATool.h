/**
 * mcpd — OTA (Over-The-Air) Update Tool
 *
 * Provides firmware update status and management for ESP32 devices.
 * Supports checking partition info, rollback, and update progress.
 *
 * Tools:
 *   - ota_info       — Current firmware info (version, partition, update state)
 *   - ota_partitions — List OTA partition layout
 *   - ota_rollback   — Roll back to previous firmware (if available)
 *   - ota_mark_valid — Mark current firmware as valid (prevent auto-rollback)
 */

#ifndef MCPD_OTA_TOOL_H
#define MCPD_OTA_TOOL_H

#include "../MCPTool.h"
#include "../mcpd.h"

namespace mcpd {

// ── OTA partition tracking (mock-friendly) ─────────────────────────────

struct OTAPartitionInfo {
    const char* label;
    uint32_t address;
    uint32_t size;
    bool isRunning;
    bool isValid;
};

struct OTAState {
    bool updateInProgress = false;
    uint32_t bytesWritten = 0;
    uint32_t totalBytes = 0;
    bool firmwareValid = true;
    bool rollbackAvailable = false;
    const char* currentPartition = "ota_0";
    const char* nextPartition = "ota_1";
    const char* firmwareVersion = MCPD_VERSION;
    const char* buildDate = __DATE__;
    const char* buildTime = __TIME__;
};

static OTAState _otaState;

/**
 * Register OTA management tools on the server.
 */
inline void registerOTATools(Server& server) {

    // ── ota_info ───────────────────────────────────────────────────
    server.addTool(
        MCPTool("ota_info",
            "Get current firmware and OTA update information",
            R"=({"type":"object","properties":{}})=",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
                doc["firmware_version"] = _otaState.firmwareVersion;
                doc["build_date"] = _otaState.buildDate;
                doc["build_time"] = _otaState.buildTime;
                doc["current_partition"] = _otaState.currentPartition;
                doc["next_partition"] = _otaState.nextPartition;
                doc["firmware_valid"] = _otaState.firmwareValid;
                doc["rollback_available"] = _otaState.rollbackAvailable;
                doc["update_in_progress"] = _otaState.updateInProgress;
                doc["uptime_s"] = millis() / 1000;

                if (_otaState.updateInProgress) {
                    doc["bytes_written"] = _otaState.bytesWritten;
                    doc["total_bytes"] = _otaState.totalBytes;
                    if (_otaState.totalBytes > 0) {
                        doc["progress_percent"] =
                            (int)(100.0f * _otaState.bytesWritten / _otaState.totalBytes);
                    }
                }

                String output;
                serializeJson(doc, output);
                return output;
            }
        ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
    );

    // ── ota_partitions ─────────────────────────────────────────────
    server.addTool(
        MCPTool("ota_partitions",
            "List OTA partition layout and status",
            R"=({"type":"object","properties":{}})=",
            [](const JsonObject& args) -> String {
                JsonDocument doc;
                JsonArray parts = doc["partitions"].to<JsonArray>();

                // OTA_0
                {
                    JsonObject p = parts.add<JsonObject>();
                    p["label"] = "ota_0";
                    p["address"] = "0x10000";
                    p["size_kb"] = 1536;
                    p["running"] = (strcmp(_otaState.currentPartition, "ota_0") == 0);
                }
                // OTA_1
                {
                    JsonObject p = parts.add<JsonObject>();
                    p["label"] = "ota_1";
                    p["address"] = "0x190000";
                    p["size_kb"] = 1536;
                    p["running"] = (strcmp(_otaState.currentPartition, "ota_1") == 0);
                }

                doc["active_partition"] = _otaState.currentPartition;
                doc["count"] = 2;

                String output;
                serializeJson(doc, output);
                return output;
            }
        ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
    );

    // ── ota_rollback ───────────────────────────────────────────────
    server.addTool(
        MCPTool("ota_rollback",
            "Roll back to the previous firmware version. WARNING: Device will restart.",
            R"=({"type":"object","properties":{
                "confirm":{"type":"boolean","description":"Must be true to proceed"}
            },"required":["confirm"]})=",
            [](const JsonObject& args) -> String {
                bool confirm = args["confirm"] | false;
                if (!confirm) {
                    return R"=({"error":"Set confirm=true to proceed with rollback"})=";
                }

                if (!_otaState.rollbackAvailable) {
                    return R"=({"error":"No previous firmware available for rollback"})=";
                }

                // In real firmware, this would call esp_ota_mark_app_invalid_rollback_and_reboot()
                JsonDocument doc;
                doc["status"] = "rolling_back";
                doc["from"] = _otaState.currentPartition;
                doc["to"] = _otaState.nextPartition;
                doc["message"] = "Device will restart with previous firmware";

                String output;
                serializeJson(doc, output);
                return output;
            }
        ).annotate(MCPToolAnnotations().setReadOnlyHint(false).setDestructiveHint(true))
    );

    // ── ota_mark_valid ─────────────────────────────────────────────
    server.addTool(
        MCPTool("ota_mark_valid",
            "Mark the current firmware as valid, preventing automatic rollback on next boot",
            R"=({"type":"object","properties":{}})=",
            [](const JsonObject& args) -> String {
                _otaState.firmwareValid = true;

                JsonDocument doc;
                doc["status"] = "marked_valid";
                doc["partition"] = _otaState.currentPartition;
                doc["firmware_version"] = _otaState.firmwareVersion;

                String output;
                serializeJson(doc, output);
                return output;
            }
        ).annotate(MCPToolAnnotations().setReadOnlyHint(false).setIdempotentHint(true))
    );
}

} // namespace mcpd

#endif // MCPD_OTA_TOOL_H
