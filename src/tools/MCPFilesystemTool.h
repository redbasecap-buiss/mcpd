/**
 * mcpd — Filesystem Built-in Tool
 *
 * Provides access to on-chip filesystem (SPIFFS/LittleFS) through MCP tools.
 * Allows AI clients to read/write/list files on the microcontroller's flash.
 *
 * Tools:
 *   - fs_list    — List files in a directory
 *   - fs_read    — Read file contents
 *   - fs_write   — Write/create a file
 *   - fs_delete  — Delete a file
 *   - fs_info    — Filesystem usage info (total/used/free)
 *   - fs_exists  — Check if a file exists
 *
 * Usage:
 *   #include <SPIFFS.h>  // or <LittleFS.h>
 *   MCPFilesystemTool::registerAll(server, SPIFFS);
 */

#ifndef MCP_FILESYSTEM_TOOL_H
#define MCP_FILESYSTEM_TOOL_H

#include "../MCPTool.h"
#include <FS.h>

namespace mcpd {

class MCPFilesystemTool {
public:
    /**
     * Register all filesystem tools with the server.
     * @param server  The MCP server instance
     * @param fs      The filesystem instance (SPIFFS, LittleFS, etc.)
     */
    static void registerAll(Server& server, fs::FS& fs) {
        _fs = &fs;

        // fs_list — List files
        MCPTool listTool("fs_list", "List files in a directory on the MCU filesystem",
            R"({"type":"object","properties":{"path":{"type":"string","description":"Directory path (default: /)"}},"required":[]})",
            [](const JsonObject& args) -> String {
                String path = "/";
                if (args.containsKey("path")) {
                    const char* p = args["path"].as<const char*>();
                    if (p) path = p;
                }

                File dir = _fs->open(path);
                if (!dir || !dir.isDirectory()) {
                    return String("Error: cannot open directory: ") + path;
                }

                JsonDocument doc;
                JsonArray files = doc["files"].to<JsonArray>();
                int count = 0;

                File entry = dir.openNextFile();
                while (entry) {
                    JsonObject f = files.add<JsonObject>();
                    f["name"] = String(entry.name());
                    f["size"] = entry.size();
                    f["isDir"] = entry.isDirectory();
                    count++;
                    entry = dir.openNextFile();
                }
                doc["count"] = count;
                doc["path"] = path;

                String result;
                serializeJson(doc, result);
                return result;
            });
        listTool.annotations.readOnlyHint = true;
        listTool.annotations.hasAnnotations = true;
        server.addTool(listTool);

        // fs_read — Read file
        MCPTool readTool("fs_read", "Read contents of a file from the MCU filesystem",
            R"({"type":"object","properties":{"path":{"type":"string","description":"File path to read"},"maxBytes":{"type":"integer","description":"Maximum bytes to read (default: 4096)"}},"required":["path"]})",
            [](const JsonObject& args) -> String {
                const char* path = args["path"].as<const char*>();
                if (!path) return "Error: missing path";

                int maxBytes = 4096;
                if (args.containsKey("maxBytes")) {
                    maxBytes = args["maxBytes"].as<int>();
                    if (maxBytes <= 0) maxBytes = 4096;
                }

                File file = _fs->open(path, "r");
                if (!file) {
                    return String("Error: cannot open file: ") + path;
                }

                size_t fileSize = file.size();
                size_t readSize = (fileSize < (size_t)maxBytes) ? fileSize : (size_t)maxBytes;

                String content;
                content.reserve(readSize);
                while (file.available() && content.length() < readSize) {
                    content += (char)file.read();
                }
                file.close();

                JsonDocument doc;
                doc["path"] = path;
                doc["size"] = fileSize;
                doc["bytesRead"] = content.length();
                doc["content"] = content;
                doc["truncated"] = (fileSize > readSize);

                String result;
                serializeJson(doc, result);
                return result;
            });
        readTool.annotations.readOnlyHint = true;
        readTool.annotations.hasAnnotations = true;
        server.addTool(readTool);

        // fs_write — Write file
        MCPTool writeTool("fs_write", "Write content to a file on the MCU filesystem",
            R"({"type":"object","properties":{"path":{"type":"string","description":"File path to write"},"content":{"type":"string","description":"Content to write"},"append":{"type":"boolean","description":"Append to file instead of overwriting (default: false)"}},"required":["path","content"]})",
            [](const JsonObject& args) -> String {
                const char* path = args["path"].as<const char*>();
                const char* content = args["content"].as<const char*>();
                if (!path || !content) return "Error: missing path or content";

                bool append = false;
                if (args.containsKey("append")) {
                    append = args["append"].as<bool>();
                }

                File file = _fs->open(path, append ? "a" : "w");
                if (!file) {
                    return String("Error: cannot open file for writing: ") + path;
                }

                size_t written = file.print(content);
                file.close();

                JsonDocument doc;
                doc["path"] = path;
                doc["bytesWritten"] = written;
                doc["mode"] = append ? "append" : "write";

                String result;
                serializeJson(doc, result);
                return result;
            });
        writeTool.annotations.destructiveHint = true;
        writeTool.annotations.hasAnnotations = true;
        server.addTool(writeTool);

        // fs_delete — Delete file
        MCPTool deleteTool("fs_delete", "Delete a file from the MCU filesystem",
            R"({"type":"object","properties":{"path":{"type":"string","description":"File path to delete"}},"required":["path"]})",
            [](const JsonObject& args) -> String {
                const char* path = args["path"].as<const char*>();
                if (!path) return "Error: missing path";

                bool existed = _fs->exists(path);
                bool removed = _fs->remove(path);

                JsonDocument doc;
                doc["path"] = path;
                doc["existed"] = existed;
                doc["deleted"] = removed;

                String result;
                serializeJson(doc, result);
                return result;
            });
        deleteTool.annotations.destructiveHint = true;
        deleteTool.annotations.hasAnnotations = true;
        server.addTool(deleteTool);

        // fs_info — Filesystem info
        MCPTool infoTool("fs_info", "Get filesystem usage information (total/used/free bytes)",
            R"({"type":"object","properties":{}})",
            [](const JsonObject& args) -> String {
                size_t total = 0;
                size_t used = 0;

                #if defined(ESP32)
                    total = SPIFFS.totalBytes();
                    used = SPIFFS.usedBytes();
                #elif defined(ESP8266)
                    FSInfo info;
                    SPIFFS.info(info);
                    total = info.totalBytes;
                    used = info.usedBytes;
                #else
                    // Generic fallback
                    total = 0;
                    used = 0;
                #endif

                JsonDocument doc;
                doc["totalBytes"] = total;
                doc["usedBytes"] = used;
                doc["freeBytes"] = (total > used) ? (total - used) : 0;
                doc["usagePercent"] = total > 0 ? (float)used / total * 100.0f : 0;

                String result;
                serializeJson(doc, result);
                return result;
            });
        infoTool.annotations.readOnlyHint = true;
        infoTool.annotations.hasAnnotations = true;
        server.addTool(infoTool);

        // fs_exists — Check if file exists
        MCPTool existsTool("fs_exists", "Check if a file or directory exists on the MCU filesystem",
            R"({"type":"object","properties":{"path":{"type":"string","description":"Path to check"}},"required":["path"]})",
            [](const JsonObject& args) -> String {
                const char* path = args["path"].as<const char*>();
                if (!path) return "Error: missing path";

                bool exists = _fs->exists(path);

                JsonDocument doc;
                doc["path"] = path;
                doc["exists"] = exists;

                // If it exists, get more info
                if (exists) {
                    File f = _fs->open(path);
                    if (f) {
                        doc["isDirectory"] = f.isDirectory();
                        doc["size"] = f.size();
                        f.close();
                    }
                }

                String result;
                serializeJson(doc, result);
                return result;
            });
        existsTool.annotations.readOnlyHint = true;
        existsTool.annotations.hasAnnotations = true;
        server.addTool(existsTool);
    }

private:
    static fs::FS* _fs;
};

// Static member initialization
fs::FS* MCPFilesystemTool::_fs = nullptr;

} // namespace mcpd

#endif // MCP_FILESYSTEM_TOOL_H
