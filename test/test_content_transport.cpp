/**
 * mcpd — Content Types, Transport & API Surface Tests
 *
 * Tests for MCPContent, MCPToolResult, transport constants,
 * tool annotations, logging, and server integration with rich tools.
 *
 * 55 tests covering content serialization, rich tool results,
 * annotations builder, transport headers, rate limiting, and
 * dynamic tool/resource lifecycle.
 */

#include "test_framework.h"
#include "../src/mcpd.h"
#include "../src/mcpd.cpp"

#include <cstring>

using namespace mcpd;

// ═══════════════════════════════════════════════════════════════════════
// MCPContent Factory Tests
// ═══════════════════════════════════════════════════════════════════════

TEST(ct_make_text) {
    auto c = MCPContent::makeText("hello world");
    ASSERT_EQ((int)c.type, (int)MCPContent::TEXT);
    ASSERT(strcmp(c.text.c_str(), "hello world") == 0);
}

TEST(ct_make_text_empty) {
    auto c = MCPContent::makeText("");
    ASSERT_EQ((int)c.type, (int)MCPContent::TEXT);
    ASSERT(c.text.length() == 0);
}

TEST(ct_make_text_unicode) {
    auto c = MCPContent::makeText("Hello 世界");
    ASSERT_STR_CONTAINS(c.text.c_str(), "Hello");
}

TEST(ct_make_image) {
    auto c = MCPContent::makeImage("aGVsbG8=", "image/png");
    ASSERT_EQ((int)c.type, (int)MCPContent::IMAGE);
    ASSERT(strcmp(c.data.c_str(), "aGVsbG8=") == 0);
    ASSERT(strcmp(c.mimeType.c_str(), "image/png") == 0);
}

TEST(ct_make_image_jpeg) {
    auto c = MCPContent::makeImage("/9j/4AAQ", "image/jpeg");
    ASSERT(strcmp(c.mimeType.c_str(), "image/jpeg") == 0);
}

TEST(ct_make_audio) {
    auto c = MCPContent::makeAudio("UklGR", "audio/wav");
    ASSERT_EQ((int)c.type, (int)MCPContent::AUDIO);
    ASSERT(strcmp(c.mimeType.c_str(), "audio/wav") == 0);
}

TEST(ct_make_audio_mp3) {
    auto c = MCPContent::makeAudio("//uQ", "audio/mpeg");
    ASSERT(strcmp(c.mimeType.c_str(), "audio/mpeg") == 0);
}

TEST(ct_make_resource_text) {
    auto c = MCPContent::makeResource("file:///data.txt", "text/plain", "content");
    ASSERT_EQ((int)c.type, (int)MCPContent::RESOURCE);
    ASSERT(strcmp(c.uri.c_str(), "file:///data.txt") == 0);
    ASSERT(c.blob.isEmpty());
}

TEST(ct_make_resource_blob) {
    auto c = MCPContent::makeResourceBlob("file:///bin", "application/octet-stream", "AQID");
    ASSERT(strcmp(c.blob.c_str(), "AQID") == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// MCPContent JSON Serialization
// ═══════════════════════════════════════════════════════════════════════

TEST(ct_text_json) {
    auto c = MCPContent::makeText("test");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    c.toJson(obj);
    ASSERT(strcmp(obj["type"].as<const char*>(), "text") == 0);
    ASSERT(strcmp(obj["text"].as<const char*>(), "test") == 0);
}

TEST(ct_image_json) {
    auto c = MCPContent::makeImage("abc=", "image/gif");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    c.toJson(obj);
    ASSERT(strcmp(obj["type"].as<const char*>(), "image") == 0);
    ASSERT(strcmp(obj["data"].as<const char*>(), "abc=") == 0);
    ASSERT(strcmp(obj["mimeType"].as<const char*>(), "image/gif") == 0);
}

TEST(ct_audio_json) {
    auto c = MCPContent::makeAudio("xyz=", "audio/ogg");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    c.toJson(obj);
    ASSERT(strcmp(obj["type"].as<const char*>(), "audio") == 0);
}

TEST(ct_resource_text_json) {
    auto c = MCPContent::makeResource("res://x", "text/csv", "a,b,c");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    c.toJson(obj);
    ASSERT(strcmp(obj["type"].as<const char*>(), "resource") == 0);
    ASSERT(strcmp(obj["resource"]["uri"].as<const char*>(), "res://x") == 0);
    ASSERT(strcmp(obj["resource"]["text"].as<const char*>(), "a,b,c") == 0);
    ASSERT(obj["resource"]["blob"].isNull());
}

TEST(ct_resource_blob_json) {
    auto c = MCPContent::makeResourceBlob("res://b", "application/octet-stream", "DEAD");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    c.toJson(obj);
    ASSERT(strcmp(obj["resource"]["blob"].as<const char*>(), "DEAD") == 0);
    ASSERT(obj["resource"]["text"].isNull());
}

// ═══════════════════════════════════════════════════════════════════════
// MCPToolResult Tests
// ═══════════════════════════════════════════════════════════════════════

TEST(tr_text) {
    auto r = MCPToolResult::text("ok");
    ASSERT_EQ(r.content.size(), (size_t)1);
    ASSERT(!r.isError);
}

TEST(tr_error) {
    auto r = MCPToolResult::error("broke");
    ASSERT(r.isError);
    ASSERT(strcmp(r.content[0].text.c_str(), "broke") == 0);
}

TEST(tr_image_with_alt) {
    auto r = MCPToolResult::image("abc=", "image/png", "Photo");
    ASSERT_EQ(r.content.size(), (size_t)2);
    ASSERT_EQ((int)r.content[0].type, (int)MCPContent::TEXT);
    ASSERT_EQ((int)r.content[1].type, (int)MCPContent::IMAGE);
}

TEST(tr_image_no_alt) {
    auto r = MCPToolResult::image("abc=", "image/png");
    ASSERT_EQ(r.content.size(), (size_t)1);
}

TEST(tr_audio_with_desc) {
    auto r = MCPToolResult::audio("wav=", "audio/wav", "Rec");
    ASSERT_EQ(r.content.size(), (size_t)2);
}

TEST(tr_audio_no_desc) {
    auto r = MCPToolResult::audio("wav=", "audio/wav");
    ASSERT_EQ(r.content.size(), (size_t)1);
}

TEST(tr_chaining) {
    MCPToolResult r;
    r.add(MCPContent::makeText("a"))
     .add(MCPContent::makeText("b"))
     .add(MCPContent::makeImage("i", "image/png"));
    ASSERT_EQ(r.content.size(), (size_t)3);
}

TEST(tr_empty) {
    MCPToolResult r;
    ASSERT_EQ(r.content.size(), (size_t)0);
    ASSERT(!r.isError);
}

TEST(tr_to_json_no_error) {
    auto r = MCPToolResult::text("hello");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    r.toJson(obj);
    ASSERT_EQ(obj["content"].as<JsonArray>().size(), (size_t)1);
    ASSERT(obj["isError"].isNull());
}

TEST(tr_error_json) {
    auto r = MCPToolResult::error("fail");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    r.toJson(obj);
    ASSERT_EQ(obj["isError"].as<bool>(), true);
}

TEST(tr_multi_json) {
    MCPToolResult r;
    r.add(MCPContent::makeText("desc"));
    r.add(MCPContent::makeImage("img", "image/png"));
    r.add(MCPContent::makeResource("r://x", "text/plain", "d"));
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    r.toJson(obj);
    auto arr = obj["content"].as<JsonArray>();
    ASSERT_EQ(arr.size(), (size_t)3);
    ASSERT(strcmp(arr[0]["type"].as<const char*>(), "text") == 0);
    ASSERT(strcmp(arr[1]["type"].as<const char*>(), "image") == 0);
    ASSERT(strcmp(arr[2]["type"].as<const char*>(), "resource") == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// Tool Annotations
// ═══════════════════════════════════════════════════════════════════════

TEST(ann_defaults) {
    MCPToolAnnotations ann;
    ASSERT(!ann.hasAnnotations);
    ASSERT(!ann.readOnlyHint);
    ASSERT(ann.destructiveHint);  // default true per MCP spec
    ASSERT(!ann.idempotentHint);
    ASSERT(ann.openWorldHint);
}

TEST(ann_readonly) {
    MCPTool t("t", "d", "{}", [](const JsonObject&) -> String { return "{}"; });
    t.markReadOnly();
    ASSERT(t.annotations.readOnlyHint);
    ASSERT(!t.annotations.destructiveHint);
    ASSERT(t.annotations.hasAnnotations);
}

TEST(ann_idempotent) {
    MCPTool t("t", "d", "{}", [](const JsonObject&) -> String { return "{}"; });
    t.markIdempotent();
    ASSERT(t.annotations.idempotentHint);
}

TEST(ann_local_only) {
    MCPTool t("t", "d", "{}", [](const JsonObject&) -> String { return "{}"; });
    t.markLocalOnly();
    ASSERT(!t.annotations.openWorldHint);
}

TEST(ann_set_custom) {
    MCPTool t("t", "d", "{}", [](const JsonObject&) -> String { return "{}"; });
    MCPToolAnnotations ann;
    ann.readOnlyHint = true;
    ann.destructiveHint = true;
    t.setAnnotations(ann);
    ASSERT(t.annotations.readOnlyHint);
    ASSERT(t.annotations.destructiveHint);
}

TEST(ann_in_json) {
    MCPTool t("t", "d", R"({"type":"object"})", [](const JsonObject&) -> String { return "{}"; });
    t.markReadOnly();
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    t.toJson(obj);
    ASSERT(!obj["annotations"].isNull());
}

TEST(ann_absent_json) {
    MCPTool t("t", "d", "{}", [](const JsonObject&) -> String { return "{}"; });
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    t.toJson(obj);
    ASSERT(obj["annotations"].isNull());
}

TEST(ann_fluent) {
    MCPToolAnnotations ann;
    ann.setReadOnlyHint(true).setIdempotentHint(true).setOpenWorldHint(false);
    ASSERT(ann.readOnlyHint);
    ASSERT(ann.idempotentHint);
    ASSERT(!ann.openWorldHint);
}

// ═══════════════════════════════════════════════════════════════════════
// Transport Constants
// ═══════════════════════════════════════════════════════════════════════

TEST(tp_content_types) {
    ASSERT(strcmp(transport::CONTENT_TYPE_JSON, "application/json") == 0);
    ASSERT(strcmp(transport::CONTENT_TYPE_SSE, "text/event-stream") == 0);
}

TEST(tp_headers) {
    ASSERT(strcmp(transport::HEADER_SESSION_ID, "Mcp-Session-Id") == 0);
    ASSERT(strcmp(transport::HEADER_ACCEPT, "Accept") == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// RateLimit
// ═══════════════════════════════════════════════════════════════════════

TEST(rl_default_allows) {
    RateLimiter rl;
    ASSERT(rl.tryAcquire());
}

TEST(rl_burst_limit) {
    RateLimiter rl;
    rl.configure(1.0, 2);
    ASSERT(rl.tryAcquire());
    ASSERT(rl.tryAcquire());
    ASSERT(!rl.tryAcquire());
}

// ═══════════════════════════════════════════════════════════════════════
// Tool JSON
// ═══════════════════════════════════════════════════════════════════════

TEST(tool_json_name) {
    MCPTool t("gpio_read", "Read GPIO", R"({"type":"object"})",
              [](const JsonObject&) -> String { return "{}"; });
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    t.toJson(obj);
    ASSERT(strcmp(obj["name"].as<const char*>(), "gpio_read") == 0);
    ASSERT(strcmp(obj["description"].as<const char*>(), "Read GPIO") == 0);
}

TEST(tool_json_schema) {
    MCPTool t("t", "d", R"({"type":"object","properties":{"pin":{"type":"integer"}}})",
              [](const JsonObject&) -> String { return "{}"; });
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    t.toJson(obj);
    ASSERT(!obj["inputSchema"].isNull());
}

// ═══════════════════════════════════════════════════════════════════════
// Resource
// ═══════════════════════════════════════════════════════════════════════

TEST(res_json) {
    MCPResource res("sensor://temp", "Temperature", "Temp", "application/json",
                    []() -> String { return "{\"temp\":22.5}"; });
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    res.toJson(obj);
    ASSERT(strcmp(obj["uri"].as<const char*>(), "sensor://temp") == 0);
}

TEST(res_handler) {
    MCPResource res("t://d", "D", "D", "text/plain",
                    []() -> String { return "hello"; });
    ASSERT(strcmp(res.handler().c_str(), "hello") == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// Prompt
// ═══════════════════════════════════════════════════════════════════════

TEST(prompt_json) {
    MCPPrompt p;
    p.name = "greet";
    p.description = "Greeting";
    MCPPromptArgument arg;
    arg.name = "name";
    arg.description = "Who";
    arg.required = true;
    p.arguments.push_back(arg);
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    p.toJson(obj);
    ASSERT(strcmp(obj["name"].as<const char*>(), "greet") == 0);
    ASSERT_EQ(obj["arguments"].as<JsonArray>().size(), (size_t)1);
}

// ═══════════════════════════════════════════════════════════════════════
// Logging
// ═══════════════════════════════════════════════════════════════════════

TEST(log_ordering) {
    ASSERT((int)LogLevel::DEBUG < (int)LogLevel::INFO);
    ASSERT((int)LogLevel::INFO < (int)LogLevel::WARNING);
    ASSERT((int)LogLevel::WARNING < (int)LogLevel::ERROR);
    ASSERT((int)LogLevel::ERROR < (int)LogLevel::CRITICAL);
}

TEST(log_to_string) {
    ASSERT(strcmp(logLevelToString(LogLevel::DEBUG), "debug") == 0);
    ASSERT(strcmp(logLevelToString(LogLevel::INFO), "info") == 0);
    ASSERT(strcmp(logLevelToString(LogLevel::WARNING), "warning") == 0);
    ASSERT(strcmp(logLevelToString(LogLevel::ERROR), "error") == 0);
    ASSERT(strcmp(logLevelToString(LogLevel::CRITICAL), "critical") == 0);
}

TEST(log_from_string) {
    ASSERT_EQ((int)logLevelFromString("debug"), (int)LogLevel::DEBUG);
    ASSERT_EQ((int)logLevelFromString("info"), (int)LogLevel::INFO);
    ASSERT_EQ((int)logLevelFromString("warning"), (int)LogLevel::WARNING);
    ASSERT_EQ((int)logLevelFromString("error"), (int)LogLevel::ERROR);
    ASSERT_EQ((int)logLevelFromString("critical"), (int)LogLevel::CRITICAL);
}

TEST(log_from_string_invalid) {
    LogLevel l = logLevelFromString("nonsense");
    ASSERT((int)l >= 0);
}

// ═══════════════════════════════════════════════════════════════════════
// Server Rich Tool Integration
// ═══════════════════════════════════════════════════════════════════════

static const char* INIT_REQ = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}})";

TEST(srv_rich_tool_list) {
    Server* s = new Server("test", 8080);
    s->addRichTool("camera", "Capture",
        R"({"type":"object","properties":{}})",
        [](const JsonObject&) -> MCPToolResult {
            return MCPToolResult::image("iVBOR", "image/png", "Cam");
        });
    s->_processJsonRpc(INIT_REQ);
    String resp = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "camera");
    delete s;
}

TEST(srv_rich_tool_multi) {
    Server* s = new Server("test", 8080);
    s->addRichTool("snap", "Snapshot",
        R"({"type":"object","properties":{}})",
        [](const JsonObject&) -> MCPToolResult {
            MCPToolResult r;
            r.add(MCPContent::makeText("Taken"));
            r.add(MCPContent::makeImage("AAAA", "image/jpeg"));
            return r;
        });
    s->_processJsonRpc(INIT_REQ);
    String resp = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"snap","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"type\":\"text\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"type\":\"image\"");
    delete s;
}

TEST(srv_rich_tool_error) {
    Server* s = new Server("test", 8080);
    s->addRichTool("fail", "Fails",
        R"({"type":"object","properties":{}})",
        [](const JsonObject&) -> MCPToolResult {
            return MCPToolResult::error("disconnected");
        });
    s->_processJsonRpc(INIT_REQ);
    String resp = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"fail","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "isError");
    ASSERT_STR_CONTAINS(resp.c_str(), "disconnected");
    delete s;
}

TEST(srv_rich_tool_audio) {
    Server* s = new Server("test", 8080);
    s->addRichTool("mic", "Record",
        R"({"type":"object","properties":{}})",
        [](const JsonObject&) -> MCPToolResult {
            return MCPToolResult::audio("UklGRg==", "audio/wav", "5s");
        });
    s->_processJsonRpc(INIT_REQ);
    String resp = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"mic","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"type\":\"audio\"");
    delete s;
}

TEST(srv_rich_tool_resource) {
    Server* s = new Server("test", 8080);
    s->addRichTool("cfg", "Config",
        R"({"type":"object","properties":{}})",
        [](const JsonObject&) -> MCPToolResult {
            MCPToolResult r;
            r.add(MCPContent::makeResource("cfg://wifi", "application/json", "{\"ssid\":\"x\"}"));
            return r;
        });
    s->_processJsonRpc(INIT_REQ);
    String resp = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"cfg","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"type\":\"resource\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "cfg://wifi");
    delete s;
}

// ═══════════════════════════════════════════════════════════════════════
// Dynamic Tool/Resource Lifecycle
// ═══════════════════════════════════════════════════════════════════════

TEST(srv_add_remove_readd) {
    Server* s = new Server("test", 8080);
    s->_processJsonRpc(INIT_REQ);
    s->addTool("tmp", "Temp", "{}", [](const JsonObject&) -> String { return "{}"; });
    String l1 = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
    ASSERT_STR_CONTAINS(l1.c_str(), "tmp");
    ASSERT(s->removeTool("tmp"));
    String l2 = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":3,"method":"tools/list","params":{}})");
    ASSERT_STR_NOT_CONTAINS(l2.c_str(), "\"tmp\"");
    s->addTool("tmp", "Temp v2", "{}", [](const JsonObject&) -> String { return "{\"v\":2}"; });
    String l3 = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":4,"method":"tools/list","params":{}})");
    ASSERT_STR_CONTAINS(l3.c_str(), "Temp v2");
    delete s;
}

TEST(srv_remove_resource_template) {
    Server* s = new Server("test", 8080);
    s->_processJsonRpc(INIT_REQ);
    MCPResourceTemplate tmpl;
    tmpl.uriTemplate = "sensor://{id}/reading";
    tmpl.name = "Sensor";
    tmpl.description = "Read sensor";
    tmpl.mimeType = "application/json";
    tmpl.handler = [](const std::map<String, String>&) -> String { return "{}"; };
    s->addResourceTemplate(tmpl);
    String l1 = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"resources/templates/list","params":{}})");
    ASSERT_STR_CONTAINS(l1.c_str(), "sensor://{id}/reading");
    ASSERT(s->removeResourceTemplate("sensor://{id}/reading"));
    String l2 = s->_processJsonRpc(R"({"jsonrpc":"2.0","id":3,"method":"resources/templates/list","params":{}})");
    ASSERT_STR_NOT_CONTAINS(l2.c_str(), "sensor://{id}/reading");
    delete s;
}

// ═══════════════════════════════════════════════════════════════════════
// Version Constants
// ═══════════════════════════════════════════════════════════════════════

TEST(version_valid) {
    String v = MCPD_VERSION;
    ASSERT(!v.isEmpty());
    ASSERT_STR_CONTAINS(v.c_str(), ".");
}

TEST(protocol_ver) {
    ASSERT(strcmp(MCPD_MCP_PROTOCOL_VERSION, "2025-11-25") == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// Heap Monitor
// ═══════════════════════════════════════════════════════════════════════

TEST(heap_no_crash) {
    HeapMonitor heap;
    heap.sample();
    heap.sample();
}

// ═══════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    printf("\n  mcpd — Content, Transport & API Surface Tests\n");
    printf("  ════════════════════════════════════════\n\n");
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
