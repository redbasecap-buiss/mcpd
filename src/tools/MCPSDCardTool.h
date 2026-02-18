/**
 * mcpd — Built-in SD Card Tool
 *
 * Provides: sd_mount, sd_info, sd_list, sd_read, sd_write, sd_append, sd_delete
 *
 * File storage on SD/microSD cards for data logging, configuration files,
 * and persistent storage. Uses ESP32 SD library (SPI mode) or emulated
 * filesystem on other platforms.
 */

#ifndef MCPD_SD_CARD_TOOL_H
#define MCPD_SD_CARD_TOOL_H

#include "../mcpd.h"
#include <map>

#ifdef ESP32
#include <SD.h>
#include <SPI.h>
#endif

namespace mcpd {
namespace tools {

class SDCardTool {
public:
    static bool& mounted() {
        static bool m = false;
        return m;
    }

    static uint8_t& csPin() {
        static uint8_t p = 5;
        return p;
    }

    static unsigned long& totalReads() {
        static unsigned long r = 0;
        return r;
    }

    static unsigned long& totalWrites() {
        static unsigned long w = 0;
        return w;
    }

    struct EmulatedFile {
        String content;
        unsigned long size;
        bool isDir;
    };

    static std::map<String, EmulatedFile>& emulatedFS() {
        static std::map<String, EmulatedFile> fs;
        return fs;
    }

    static void attach(Server& server, uint8_t chipSelect = 5) {
        csPin() = chipSelect;

        // sd_mount
        server.addTool(
            MCPTool("sd_mount", "Mount/initialize the SD card",
                R"({"type":"object","properties":{"cs_pin":{"type":"integer","description":"SPI chip-select pin"}}})",
                [](const JsonObject& args) -> String {
                    uint8_t pin = args["cs_pin"] | csPin();
                    csPin() = pin;
#ifdef ESP32
                    if (SD.begin(pin)) {
                        mounted() = true;
                        uint8_t cardType = SD.cardType();
                        const char* typeStr = "UNKNOWN";
                        if (cardType == CARD_MMC) typeStr = "MMC";
                        else if (cardType == CARD_SD) typeStr = "SD";
                        else if (cardType == CARD_SDHC) typeStr = "SDHC";
                        uint64_t totalBytes = SD.totalBytes();
                        uint64_t usedBytes = SD.usedBytes();
                        return String(R"({"mounted":true,"card_type":")") + typeStr +
                               R"(","total_mb":)" + (totalBytes / (1024*1024)) +
                               R"(,"used_mb":)" + (usedBytes / (1024*1024)) +
                               R"(,"cs_pin":)" + pin + "}";
                    }
                    return R"({"mounted":false,"error":"Failed to mount SD card"})";
#else
                    mounted() = true;
                    return String(R"({"mounted":true,"card_type":"emulated","total_mb":1024,"used_mb":0,"cs_pin":)") + pin + "}";
#endif
                }
            )
        );

        // sd_info
        server.addTool(
            MCPTool("sd_info", "Get SD card information (type, capacity, usage)",
                R"({"type":"object","properties":{}})",
                [](const JsonObject&) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted. Call sd_mount first"})";
#ifdef ESP32
                    uint8_t cardType = SD.cardType();
                    const char* typeStr = "UNKNOWN";
                    if (cardType == CARD_MMC) typeStr = "MMC";
                    else if (cardType == CARD_SD) typeStr = "SD";
                    else if (cardType == CARD_SDHC) typeStr = "SDHC";
                    uint64_t totalBytes = SD.totalBytes();
                    uint64_t usedBytes = SD.usedBytes();
                    return String(R"({"card_type":")") + typeStr +
                           R"(","total_mb":)" + (totalBytes / (1024*1024)) +
                           R"(,"used_mb":)" + (usedBytes / (1024*1024)) +
                           R"(,"free_mb":)" + ((totalBytes - usedBytes) / (1024*1024)) +
                           R"(,"total_reads":)" + totalReads() +
                           R"(,"total_writes":)" + totalWrites() + "}";
#else
                    int totalSize = 0;
                    for (auto& kv : emulatedFS()) totalSize += (int)kv.second.size;
                    return String(R"({"card_type":"emulated","total_mb":1024,"used_mb":)") +
                           (int)(totalSize / (1024*1024)) +
                           R"(,"free_mb":)" + (int)(1024 - totalSize / (1024*1024)) +
                           R"(,"files":)" + (int)emulatedFS().size() +
                           R"(,"total_reads":)" + totalReads() +
                           R"(,"total_writes":)" + totalWrites() + "}";
#endif
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );

        // sd_list
        server.addTool(
            MCPTool("sd_list", "List files and directories on the SD card",
                R"({"type":"object","properties":{"path":{"type":"string","description":"Directory path"},"recursive":{"type":"boolean"}}})",
                [](const JsonObject& args) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted"})";
                    const char* path = args["path"] | "/";
                    totalReads()++;
#ifdef ESP32
                    File dir = SD.open(path);
                    if (!dir || !dir.isDirectory()) {
                        return String(R"({"error":"Cannot open directory: )") + path + R"("})";
                    }
                    String result = R"({"path":")" + String(path) + R"(","entries":[)";
                    bool first = true;
                    File entry;
                    while ((entry = dir.openNextFile())) {
                        if (!first) result += ",";
                        first = false;
                        result += String(R"({"name":")") + entry.name() +
                                  R"(","size":)" + entry.size() +
                                  R"(,"is_dir":)" + (entry.isDirectory() ? "true" : "false") + "}";
                        entry.close();
                    }
                    dir.close();
                    result += "]}";
                    return result;
#else
                    String result = String(R"({"path":")") + path + R"(","entries":[)";
                    bool first = true;
                    for (auto& kv : emulatedFS()) {
                        if (!first) result += ",";
                        first = false;
                        result += String(R"({"name":")") + kv.first +
                                  R"(","size":)" + kv.second.size +
                                  R"(,"is_dir":)" + (kv.second.isDir ? "true" : "false") + "}";
                    }
                    result += "]}";
                    return result;
#endif
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );

        // sd_read
        server.addTool(
            MCPTool("sd_read", "Read contents of a file from SD card",
                R"({"type":"object","properties":{"path":{"type":"string","description":"File path"},"offset":{"type":"integer"},"length":{"type":"integer"}},"required":["path"]})",
                [](const JsonObject& args) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted"})";
                    const char* path = args["path"] | "";
                    if (!path[0]) return R"({"error":"Path required"})";
                    int offset = args["offset"] | 0;
                    int length = args["length"] | 4096;
                    if (length > 4096) length = 4096;
                    totalReads()++;
#ifdef ESP32
                    File file = SD.open(path);
                    if (!file || file.isDirectory()) {
                        return String(R"({"error":"Cannot open file: )") + path + R"("})";
                    }
                    size_t fileSize = file.size();
                    if (offset > 0) file.seek(offset);
                    char* buf = new char[length + 1];
                    size_t bytesRead = file.readBytes(buf, length);
                    buf[bytesRead] = '\0';
                    file.close();
                    String content = buf;
                    delete[] buf;
                    content.replace("\\", "\\\\");
                    content.replace("\"", "\\\"");
                    content.replace("\n", "\\n");
                    content.replace("\r", "\\r");
                    return String(R"({"path":")") + path +
                           R"(","size":)" + fileSize +
                           R"(,"offset":)" + offset +
                           R"(,"bytes_read":)" + bytesRead +
                           R"(,"content":")" + content + R"("})";
#else
                    auto it = emulatedFS().find(String(path));
                    if (it == emulatedFS().end()) {
                        return String(R"({"error":"File not found: )") + path + R"("})";
                    }
                    String content = it->second.content.substring(offset, offset + length);
                    content.replace("\\", "\\\\");
                    content.replace("\"", "\\\"");
                    content.replace("\n", "\\n");
                    return String(R"({"path":")") + path +
                           R"(","size":)" + it->second.size +
                           R"(,"offset":)" + offset +
                           R"(,"bytes_read":)" + content.length() +
                           R"(,"content":")" + content + R"("})";
#endif
                }
            ).annotate(MCPToolAnnotations().setReadOnlyHint(true))
        );

        // sd_write
        server.addTool(
            MCPTool("sd_write", "Write content to a file on SD card (overwrites existing)",
                R"({"type":"object","properties":{"path":{"type":"string","description":"File path"},"content":{"type":"string","description":"Content to write"}},"required":["path","content"]})",
                [](const JsonObject& args) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted"})";
                    const char* path = args["path"] | "";
                    const char* content = args["content"] | "";
                    if (!path[0]) return R"({"error":"Path required"})";
                    totalWrites()++;
#ifdef ESP32
                    File file = SD.open(path, FILE_WRITE);
                    if (!file) {
                        return String(R"({"error":"Cannot create file: )") + path + R"("})";
                    }
                    size_t written = file.print(content);
                    file.close();
                    return String(R"({"path":")") + path +
                           R"(","bytes_written":)" + written +
                           R"(,"mode":"overwrite"})";
#else
                    emulatedFS()[String(path)] = {String(content), strlen(content), false};
                    return String(R"({"path":")") + path +
                           R"(","bytes_written":)" + strlen(content) +
                           R"(,"mode":"overwrite"})";
#endif
                }
            ).annotate(MCPToolAnnotations().setDestructiveHint(true))
        );

        // sd_append
        server.addTool(
            MCPTool("sd_append", "Append content to a file on SD card (creates if not exists)",
                R"({"type":"object","properties":{"path":{"type":"string","description":"File path"},"content":{"type":"string","description":"Content to append"}},"required":["path","content"]})",
                [](const JsonObject& args) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted"})";
                    const char* path = args["path"] | "";
                    const char* content = args["content"] | "";
                    if (!path[0]) return R"({"error":"Path required"})";
                    totalWrites()++;
#ifdef ESP32
                    File file = SD.open(path, FILE_APPEND);
                    if (!file) {
                        return String(R"({"error":"Cannot open file for append: )") + path + R"("})";
                    }
                    size_t written = file.print(content);
                    size_t newSize = file.size();
                    file.close();
                    return String(R"({"path":")") + path +
                           R"(","bytes_appended":)" + written +
                           R"(,"new_size":)" + newSize +
                           R"(,"mode":"append"})";
#else
                    auto it = emulatedFS().find(String(path));
                    if (it != emulatedFS().end()) {
                        it->second.content += content;
                        it->second.size = it->second.content.length();
                    } else {
                        emulatedFS()[String(path)] = {String(content), strlen(content), false};
                    }
                    auto& f = emulatedFS()[String(path)];
                    return String(R"({"path":")") + path +
                           R"(","bytes_appended":)" + strlen(content) +
                           R"(,"new_size":)" + f.size +
                           R"(,"mode":"append"})";
#endif
                }
            )
        );

        // sd_delete
        server.addTool(
            MCPTool("sd_delete", "Delete a file from SD card",
                R"({"type":"object","properties":{"path":{"type":"string","description":"File path to delete"}},"required":["path"]})",
                [](const JsonObject& args) -> String {
                    if (!mounted()) return R"({"error":"SD card not mounted"})";
                    const char* path = args["path"] | "";
                    if (!path[0]) return R"({"error":"Path required"})";
#ifdef ESP32
                    if (SD.exists(path)) {
                        bool ok = SD.remove(path);
                        return String(R"({"path":")") + path +
                               R"(","deleted":)" + (ok ? "true" : "false") + "}";
                    }
                    return String(R"({"error":"File not found: )") + path + R"("})";
#else
                    auto it = emulatedFS().find(String(path));
                    if (it != emulatedFS().end()) {
                        emulatedFS().erase(it);
                        return String(R"({"path":")") + path + R"(","deleted":true})";
                    }
                    return String(R"({"error":"File not found: )") + path + R"("})";
#endif
                }
            ).annotate(MCPToolAnnotations().setDestructiveHint(true))
        );
    }
};

inline void addSDCardTools(Server& server, uint8_t csPin = 5) {
    SDCardTool::attach(server, csPin);
}

} // namespace tools
} // namespace mcpd

#endif // MCPD_SD_CARD_TOOL_H
