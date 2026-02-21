/**
 * mcpd — Advanced Tests
 *
 * Covers: test framework macros, resource template edge cases,
 * server API surface, content builders, rate limiter, JSON-RPC dispatch.
 */

#include "arduino_mock.h"
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "test_framework.h"
#include "../src/mcpd.h"
#include "../src/mcpd.cpp"

using namespace mcpd;

static struct _hdr1 { _hdr1() {
    printf("\n  mcpd — Advanced Tests\n  ════════════════════════════════════════\n\n");
} } _h1;

// ═══════════════════════════════════════════════════════════════════════
// Test Framework — New Assertion Macros
// ═══════════════════════════════════════════════════════════════════════

TEST(assert_true_false) {
    ASSERT_TRUE(1 == 1);
    ASSERT_FALSE(1 == 2);
}

TEST(assert_gt_lt_ge_le) {
    ASSERT_GT(10, 5);
    ASSERT_LT(5, 10);
    ASSERT_GE(10, 10);
    ASSERT_GE(11, 10);
    ASSERT_LE(10, 10);
    ASSERT_LE(9, 10);
}

TEST(assert_str_eq) {
    ASSERT_STR_EQ("hello", "hello");
    ASSERT_STR_EQ("", "");
    const char* p = "test";
    ASSERT_STR_EQ(p, "test");
}

TEST(assert_near) {
    ASSERT_NEAR(3.14, 3.14, 0.001);
    ASSERT_NEAR(1.0, 1.0001, 0.001);
    ASSERT_NEAR(0.0, 0.0, 0.0);
    ASSERT_NEAR(-1.5, -1.500001, 0.001);
}

// ═══════════════════════════════════════════════════════════════════════
// Resource Template Matching — Edge Cases
// ═══════════════════════════════════════════════════════════════════════

TEST(template_single_var) {
    MCPResourceTemplate tmpl("device://{id}", "Device", "A device", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_TRUE(tmpl.match("device://abc123", params));
    ASSERT_EQ(params[String("id")], String("abc123"));
}

TEST(template_multiple_vars) {
    MCPResourceTemplate tmpl("sensor://{type}/{id}/reading", "Reading", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_TRUE(tmpl.match("sensor://temperature/t1/reading", params));
    ASSERT_EQ(params[String("type")], String("temperature"));
    ASSERT_EQ(params[String("id")], String("t1"));
}

TEST(template_no_match_wrong_prefix) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "S", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_FALSE(tmpl.match("device://abc/reading", params));
}

TEST(template_no_match_empty_var) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "S", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_FALSE(tmpl.match("sensor:///reading", params));
}

TEST(template_no_vars_literal) {
    MCPResourceTemplate tmpl("sensor://static", "S", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_TRUE(tmpl.match("sensor://static", params));
    ASSERT_EQ(params.size(), (size_t)0);
}

TEST(template_variables_extraction) {
    MCPResourceTemplate tmpl("a://{x}/{y}/{z}", "", "", "", nullptr);
    auto vars = tmpl.variables();
    ASSERT_EQ(vars.size(), (size_t)3);
    ASSERT_EQ(vars[0], String("x"));
    ASSERT_EQ(vars[1], String("y"));
    ASSERT_EQ(vars[2], String("z"));
}

TEST(template_adjacent_vars) {
    MCPResourceTemplate tmpl("data://{a}/{b}", "D", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_TRUE(tmpl.match("data://foo/bar", params));
    ASSERT_EQ(params[String("a")], String("foo"));
    ASSERT_EQ(params[String("b")], String("bar"));
}

TEST(template_json_serialization) {
    MCPResourceTemplate tmpl("sensor://{id}/temp", "Temp Sensor", "Reads temp", "application/json", nullptr);
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    tmpl.toJson(obj);
    ASSERT_STR_CONTAINS(obj["uriTemplate"].as<const char*>(), "sensor://{id}/temp");
    ASSERT_STR_CONTAINS(obj["name"].as<const char*>(), "Temp Sensor");
}

TEST(template_no_match_shorter_uri) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "S", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_FALSE(tmpl.match("sensor://abc", params));
}

TEST(template_vars_with_numbers) {
    MCPResourceTemplate tmpl("gpio://{pin_number}", "GPIO", "", "text/plain", nullptr);
    std::map<String, String> params;
    ASSERT_TRUE(tmpl.match("gpio://42", params));
    ASSERT_EQ(params[String("pin_number")], String("42"));
}

// ═══════════════════════════════════════════════════════════════════════
// MCPContent Factories
// ═══════════════════════════════════════════════════════════════════════

TEST(content_make_text) {
    auto c = MCPContent::makeText("hello");
    ASSERT_EQ(c.type, MCPContent::TEXT);
    ASSERT_EQ(c.text, String("hello"));
}

TEST(content_make_text_empty) {
    auto c = MCPContent::makeText("");
    ASSERT_EQ(c.type, MCPContent::TEXT);
    ASSERT_EQ(c.text, String(""));
}

TEST(content_make_image) {
    auto c = MCPContent::makeImage("iVBORw==", "image/png");
    ASSERT_EQ(c.type, MCPContent::IMAGE);
    ASSERT_EQ(c.data, String("iVBORw=="));
    ASSERT_EQ(c.mimeType, String("image/png"));
}

TEST(content_make_audio) {
    auto c = MCPContent::makeAudio("AAAA", "audio/wav");
    ASSERT_EQ(c.type, MCPContent::AUDIO);
    ASSERT_EQ(c.data, String("AAAA"));
    ASSERT_EQ(c.mimeType, String("audio/wav"));
}

TEST(content_make_resource) {
    auto c = MCPContent::makeResource("file://test.txt", "text/plain", "hello");
    ASSERT_EQ(c.type, MCPContent::RESOURCE);
    ASSERT_EQ(c.uri, String("file://test.txt"));
    ASSERT_EQ(c.text, String("hello"));
}

TEST(content_make_resource_blob) {
    auto c = MCPContent::makeResourceBlob("file://img.png", "image/png", "base64==");
    ASSERT_EQ(c.type, MCPContent::RESOURCE);
    ASSERT_EQ(c.blob, String("base64=="));
}

// ═══════════════════════════════════════════════════════════════════════
// MCPToolResult
// ═══════════════════════════════════════════════════════════════════════

TEST(tool_result_text) {
    auto r = MCPToolResult::text("hello");
    ASSERT_EQ(r.content.size(), (size_t)1);
    ASSERT_EQ(r.content[0].text, String("hello"));
    ASSERT_FALSE(r.isError);
}

TEST(tool_result_error) {
    auto r = MCPToolResult::error("broken");
    ASSERT_TRUE(r.isError);
    ASSERT_EQ(r.content[0].text, String("broken"));
}

TEST(tool_result_add_chain) {
    auto r = MCPToolResult::text("a")
        .add(MCPContent::makeText("b"))
        .add(MCPContent::makeImage("data", "image/png"));
    ASSERT_EQ(r.content.size(), (size_t)3);
    ASSERT_EQ(r.content[0].type, MCPContent::TEXT);
    ASSERT_EQ(r.content[2].type, MCPContent::IMAGE);
}

TEST(tool_result_image_with_alt) {
    auto r = MCPToolResult::image("data", "image/jpeg", "A photo");
    ASSERT_GE(r.content.size(), (size_t)2);
    ASSERT_EQ(r.content[0].text, String("A photo"));
    ASSERT_EQ(r.content[1].type, MCPContent::IMAGE);
}

TEST(tool_result_audio_with_desc) {
    auto r = MCPToolResult::audio("data", "audio/mp3", "A recording");
    ASSERT_GE(r.content.size(), (size_t)2);
    ASSERT_EQ(r.content[0].text, String("A recording"));
    ASSERT_EQ(r.content[1].type, MCPContent::AUDIO);
}

TEST(tool_result_json) {
    auto r = MCPToolResult::error("fail");
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    r.toJson(obj);
    ASSERT_TRUE(obj["isError"].as<bool>());
    ASSERT_EQ(obj["content"].as<JsonArray>().size(), (size_t)1);
}

// ═══════════════════════════════════════════════════════════════════════
// Tool Annotations
// ═══════════════════════════════════════════════════════════════════════

TEST(annotations_defaults_per_spec) {
    MCPToolAnnotations ann;
    ASSERT_FALSE(ann.readOnlyHint);
    ASSERT_TRUE(ann.destructiveHint);  // default true per spec
    ASSERT_FALSE(ann.idempotentHint);
    ASSERT_TRUE(ann.openWorldHint);    // default true per spec
}

TEST(annotations_readonly_clears_destructive) {
    MCPToolAnnotations ann;
    ann.setReadOnlyHint(true);
    ASSERT_TRUE(ann.readOnlyHint);
    ASSERT_FALSE(ann.destructiveHint);
}

TEST(annotations_fluent_chain) {
    MCPToolAnnotations ann;
    ann.setIdempotentHint(true).setOpenWorldHint(false);
    ASSERT_TRUE(ann.idempotentHint);
    ASSERT_FALSE(ann.openWorldHint);
}

// ═══════════════════════════════════════════════════════════════════════
// Server API Surface
// ═══════════════════════════════════════════════════════════════════════

TEST(server_name_port) {
    Server srv("test-device", 8080);
    ASSERT_STR_EQ(srv.getName(), "test-device");
    ASSERT_EQ(srv.getPort(), (uint16_t)8080);
}

TEST(server_default_port) {
    Server srv("default-port");
    ASSERT_EQ(srv.getPort(), (uint16_t)80);
}

TEST(server_add_remove_tool) {
    Server srv("test");
    MCPTool tool;
    tool.name = "led";
    tool.description = "LED";
    tool.inputSchemaJson = "{}";
    tool.handler = [](const JsonObject&) -> String { return "ok"; };
    srv.addTool(tool);
    ASSERT_EQ(srv._tools.size(), (size_t)1);
    ASSERT_TRUE(srv.removeTool("led"));
    ASSERT_EQ(srv._tools.size(), (size_t)0);
    ASSERT_FALSE(srv.removeTool("nonexistent"));
}

TEST(server_add_remove_resource) {
    Server srv("test");
    srv.addResource("file://test", "Test", "A test", "text/plain",
                    []() { return String("content"); });
    ASSERT_EQ(srv._resources.size(), (size_t)1);
    ASSERT_TRUE(srv.removeResource("file://test"));
    ASSERT_EQ(srv._resources.size(), (size_t)0);
    ASSERT_FALSE(srv.removeResource("file://nope"));
}

TEST(server_add_remove_prompt) {
    Server srv("test");
    srv.addPrompt("greet", "A greeting", {},
                  [](const std::map<String, String>&) {
                      return std::vector<MCPPromptMessage>();
                  });
    ASSERT_EQ(srv._prompts.size(), (size_t)1);
    ASSERT_TRUE(srv.removePrompt("greet"));
    ASSERT_EQ(srv._prompts.size(), (size_t)0);
}

TEST(server_add_remove_root) {
    Server srv("test");
    srv.addRoot("file:///home", "Home");
    ASSERT_EQ(srv._roots.size(), (size_t)1);
    ASSERT_TRUE(srv.removeRoot("file:///home"));
    ASSERT_EQ(srv._roots.size(), (size_t)0);
}

TEST(server_add_remove_resource_template) {
    Server srv("test");
    srv.addResourceTemplate("sensor://{id}/temp", "Temp", "Temperature", "text/plain",
                            [](const std::map<String, String>&) { return String("25.0"); });
    ASSERT_EQ(srv._resourceTemplates.size(), (size_t)1);
    ASSERT_TRUE(srv.removeResourceTemplate("sensor://{id}/temp"));
    ASSERT_EQ(srv._resourceTemplates.size(), (size_t)0);
}

TEST(server_set_endpoint) {
    Server srv("test");
    srv.setEndpoint("/api/mcp");
    ASSERT_STR_EQ(srv._endpoint, "/api/mcp");
}

TEST(server_set_mdns) {
    Server srv("test");
    srv.setMDNS(false);
    ASSERT_FALSE(srv._mdnsEnabled);
    srv.setMDNS(true);
    ASSERT_TRUE(srv._mdnsEnabled);
}

TEST(server_set_page_size) {
    Server srv("test");
    ASSERT_EQ(srv._pageSize, (size_t)0);
    srv.setPageSize(10);
    ASSERT_EQ(srv._pageSize, (size_t)10);
}

TEST(server_multiple_tools) {
    Server srv("test");
    auto makeTool = [](const char* n) {
        MCPTool t; t.name = n; t.description = n;
        t.inputSchemaJson = "{}";
        t.handler = [](const JsonObject&) -> String { return "ok"; };
        return t;
    };
    srv.addTool(makeTool("a"));
    srv.addTool(makeTool("b"));
    srv.addTool(makeTool("c"));
    ASSERT_EQ(srv._tools.size(), (size_t)3);
    srv.removeTool("b");
    ASSERT_EQ(srv._tools.size(), (size_t)2);
}

// ═══════════════════════════════════════════════════════════════════════
// Logging
// ═══════════════════════════════════════════════════════════════════════

TEST(logging_level_ordering) {
    ASSERT_LT((int)LogLevel::DEBUG, (int)LogLevel::INFO);
    ASSERT_LT((int)LogLevel::INFO, (int)LogLevel::NOTICE);
    ASSERT_LT((int)LogLevel::NOTICE, (int)LogLevel::WARNING);
    ASSERT_LT((int)LogLevel::WARNING, (int)LogLevel::ERROR);
    ASSERT_LT((int)LogLevel::ERROR, (int)LogLevel::CRITICAL);
    ASSERT_LT((int)LogLevel::CRITICAL, (int)LogLevel::ALERT);
    ASSERT_LT((int)LogLevel::ALERT, (int)LogLevel::EMERGENCY);
}

// ═══════════════════════════════════════════════════════════════════════
// RateLimiter
// ═══════════════════════════════════════════════════════════════════════

TEST(rate_limiter_disabled_by_default) {
    RateLimiter rl;
    ASSERT_FALSE(rl.isEnabled());
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(rl.tryAcquire());
    }
}

TEST(rate_limiter_burst_exact) {
    RateLimiter rl;
    rl.configure(1.0f, 3);
    ASSERT_TRUE(rl.isEnabled());
    ASSERT_TRUE(rl.tryAcquire());
    ASSERT_TRUE(rl.tryAcquire());
    ASSERT_TRUE(rl.tryAcquire());
    ASSERT_FALSE(rl.tryAcquire());
}

TEST(rate_limiter_stats) {
    RateLimiter rl;
    rl.configure(10.0f, 2);
    rl.tryAcquire();
    rl.tryAcquire();
    rl.tryAcquire(); // denied
    ASSERT_EQ(rl.totalAllowed(), 2UL);
    ASSERT_EQ(rl.totalDenied(), 1UL);
}

TEST(rate_limiter_disable) {
    RateLimiter rl;
    rl.configure(1.0f, 1);
    rl.disable();
    ASSERT_FALSE(rl.isEnabled());
    ASSERT_TRUE(rl.tryAcquire());
    ASSERT_TRUE(rl.tryAcquire());
}

TEST(rate_limiter_config_values) {
    RateLimiter rl;
    rl.configure(5.0f, 10);
    ASSERT_NEAR(rl.requestsPerSecond(), 5.0f, 0.01);
    ASSERT_EQ(rl.burstCapacity(), (size_t)10);
}

// ═══════════════════════════════════════════════════════════════════════
// JSON-RPC Dispatch
// ═══════════════════════════════════════════════════════════════════════

TEST(jsonrpc_ping) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"ping"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"result\"");
}

TEST(jsonrpc_method_not_found) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":2,"method":"nonexistent/method"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"error\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "-32601");
}

TEST(jsonrpc_invalid_json) {
    Server srv("test");
    String resp = srv._processJsonRpc("not json");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"error\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "-32700");
}

TEST(jsonrpc_initialize) {
    Server srv("init-test");
    String req = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{
        "protocolVersion":"2025-03-26",
        "capabilities":{},
        "clientInfo":{"name":"test-client","version":"1.0"}
    }})";
    String resp = srv._processJsonRpc(req);
    ASSERT_STR_CONTAINS(resp.c_str(), "\"serverInfo\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "init-test");
    ASSERT_STR_CONTAINS(resp.c_str(), "2025-03-26");
}

TEST(jsonrpc_tools_list_empty) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"tools\"");
}

TEST(jsonrpc_tools_list_with_tool) {
    Server srv("test");
    MCPTool tool;
    tool.name = "led";
    tool.description = "Control LED";
    tool.inputSchemaJson = R"({"type":"object"})";
    tool.handler = [](const JsonObject&) -> String { return "{\"ok\":true}"; };
    srv.addTool(tool);
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"led\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "Control LED");
}

TEST(jsonrpc_tools_call) {
    Server srv("test");
    MCPTool tool;
    tool.name = "echo";
    tool.description = "Echo";
    tool.inputSchemaJson = R"({"type":"object"})";
    tool.handler = [](const JsonObject&) -> String { return "{\"echo\":\"hello\"}"; };
    srv.addTool(tool);
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"echo","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"result\"");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "\"error\"");
}

TEST(jsonrpc_tools_call_not_found) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"missing","arguments":{}}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"error\"");
}

TEST(jsonrpc_resources_list_empty) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"resources/list"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"resources\"");
}

TEST(jsonrpc_prompts_list_empty) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"prompts/list"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"prompts\"");
}

TEST(jsonrpc_logging_set_level) {
    Server srv("test");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"logging/setLevel","params":{"level":"warning"}})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"result\"");
}

TEST(jsonrpc_roots_list) {
    Server srv("test");
    srv.addRoot("file:///project", "Project");
    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","id":1,"method":"roots/list"})");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"roots\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "file:///project");
}

// ═══════════════════════════════════════════════════════════════════════
// HeapMonitor
// ═══════════════════════════════════════════════════════════════════════

TEST(heap_monitor_initial) {
    HeapMonitor hm;
    ASSERT_FALSE(hm.isLow());
    ASSERT_EQ(hm.sampleCount(), (size_t)0);
}

TEST(heap_monitor_threshold) {
    HeapMonitor hm;
    hm.setWarningThreshold(50000);
    // On non-ESP32, isLow() always returns false
    ASSERT_FALSE(hm.isLow());
}

TEST(heap_monitor_usage) {
    HeapMonitor hm;
    // Without sampling, usage should be 0
    ASSERT_NEAR(hm.usagePercent(), 0.0f, 0.01);
}

// ═══════════════════════════════════════════════════════════════════════
// Version Constants
// ═══════════════════════════════════════════════════════════════════════

TEST(version_semver_format) {
    String v(MCPD_VERSION);
    int dots = 0;
    for (size_t i = 0; i < v.length(); i++) {
        if (v.charAt(i) == '.') dots++;
    }
    ASSERT_EQ(dots, 2);
}

TEST(protocol_version_date_format) {
    String pv(MCPD_MCP_PROTOCOL_VERSION);
    ASSERT_EQ(pv.length(), (size_t)10);
    ASSERT_EQ(pv.charAt(4), '-');
    ASSERT_EQ(pv.charAt(7), '-');
}

// ═══════════════════════════════════════════════════════════════════════

int main() {
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
