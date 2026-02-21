/**
 * mcpd — Output Schema & Annotations Tests
 *
 * Tests for tool outputSchema, resource annotations, resource template
 * annotations, and structuredContent in tool results.
 */

#include "arduino_mock.h"
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "test_framework.h"
#include "../src/mcpd.h"
#include "../src/mcpd.cpp"

using namespace mcpd;

static struct _hdr_oa { _hdr_oa() {
    printf("\n  mcpd — Output Schema & Annotations Tests\n  ════════════════════════════════════════\n\n");
} } _h_oa;

// Helper: initialize server so tools/call works
static void initServer(Server& srv) {
    srv._processJsonRpc(R"({"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2025-03-26","clientInfo":{"name":"test"}}})");
}

// ════════════════════════════════════════════════════════════════════════
// Tool Output Schema Tests
// ════════════════════════════════════════════════════════════════════════

TEST(tool_output_schema_in_list) {
    Server srv("test", 80);
    MCPTool tool("get_reading", "Get sensor reading",
                 R"({"type":"object","properties":{"sensor":{"type":"string"}}})",
                 [](const JsonObject&) { return String(R"({"temp":22.5})"); });
    tool.setOutputSchema(R"({"type":"object","properties":{"temp":{"type":"number"}}})");
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "outputSchema");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"temp\"");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"number\"");
}

TEST(tool_without_output_schema_omits_field) {
    Server srv("test", 80);
    srv.addTool("simple", "Simple tool",
                R"({"type":"object"})",
                [](const JsonObject&) { return String("ok"); });
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/list","id":1})");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "outputSchema");
}

TEST(tool_output_schema_builder_chain) {
    MCPTool tool("t", "d", R"({"type":"object"})",
                 [](const JsonObject&) { return String("ok"); });
    MCPTool& ref = tool.setOutputSchema(R"({"type":"string"})");
    ASSERT(&ref == &tool);
    ASSERT(tool.outputSchemaJson == R"({"type":"string"})");
}

TEST(structured_content_simple_handler) {
    Server srv("test", 80);
    MCPTool tool("json_tool", "Returns JSON",
                 R"({"type":"object"})",
                 [](const JsonObject&) { return String(R"({"value":42,"label":"answer"})"); });
    tool.setOutputSchema(R"({"type":"object","properties":{"value":{"type":"number"},"label":{"type":"string"}}})");
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"json_tool","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "structuredContent");
    ASSERT_STR_CONTAINS(resp.c_str(), "\"content\"");
}

TEST(structured_content_not_added_for_non_json) {
    Server srv("test", 80);
    MCPTool tool("text_tool", "Returns plain text",
                 R"({"type":"object"})",
                 [](const JsonObject&) { return String("hello world"); });
    tool.setOutputSchema(R"({"type":"string"})");
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"text_tool","arguments":{}},"id":1})");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "structuredContent");
}

TEST(structured_content_not_added_on_error) {
    Server srv("test", 80);
    MCPTool tool("err_tool", "Throws",
                 R"({"type":"object"})",
                 [](const JsonObject&) -> String { throw std::runtime_error("fail"); });
    tool.setOutputSchema(R"({"type":"object"})");
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"err_tool","arguments":{}},"id":1})");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "structuredContent");
}

TEST(structured_content_without_output_schema) {
    Server srv("test", 80);
    srv.addTool("plain", "No schema",
                R"({"type":"object"})",
                [](const JsonObject&) { return String(R"({"key":"val"})"); });
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"plain","arguments":{}},"id":1})");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "structuredContent");
}

TEST(structured_content_array_output) {
    Server srv("test", 80);
    MCPTool tool("arr_tool", "Returns array",
                 R"({"type":"object"})",
                 [](const JsonObject&) { return String(R"([1,2,3])"); });
    tool.setOutputSchema(R"({"type":"array","items":{"type":"number"}})");
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"arr_tool","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "structuredContent");
    ASSERT_STR_CONTAINS(resp.c_str(), "[1,2,3]");
}

// ════════════════════════════════════════════════════════════════════════
// Resource Annotations Tests
// ════════════════════════════════════════════════════════════════════════

TEST(resource_annotations_audience) {
    Server srv("test", 80);
    MCPResource res("file://log", "Log", "System log", "text/plain",
                    []() { return String("log data"); });
    res.setAudience("user");
    srv.addResource(res);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"resources/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "annotations");
    ASSERT_STR_CONTAINS(resp.c_str(), "audience");
    ASSERT_STR_CONTAINS(resp.c_str(), "user");
}

TEST(resource_annotations_priority) {
    Server srv("test", 80);
    MCPResource res("file://important", "Important", "High priority", "text/plain",
                    []() { return String("data"); });
    res.setPriority(0.9f);
    srv.addResource(res);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"resources/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "annotations");
    ASSERT_STR_CONTAINS(resp.c_str(), "priority");
}

TEST(resource_annotations_both) {
    MCPResource res("file://x", "X", "Both", "text/plain", []() { return String(""); });
    res.setAudience("assistant").setPriority(0.5f);

    ASSERT(res.annotations.hasAnnotations);
    ASSERT(res.annotations.audience == "assistant");
    ASSERT(res.annotations.priority >= 0.49f && res.annotations.priority <= 0.51f);
}

TEST(resource_annotations_absent_by_default) {
    MCPResource res("file://plain", "Plain", "No extra hints", "text/plain",
                    []() { return String("data"); });

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    res.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_NOT_CONTAINS(json.c_str(), "annotations");
}

TEST(resource_annotations_annotate_builder) {
    MCPResourceAnnotations ann;
    ann.setAudience("user").setPriority(0.7f);

    MCPResource res("file://x", "X", "D", "text/plain", []() { return String(""); });
    res.annotate(ann);

    ASSERT(res.annotations.hasAnnotations);
    ASSERT(res.annotations.audience == "user");
}

TEST(resource_annotations_toJson) {
    MCPResource res("file://x", "X", "D", "text/plain", []() { return String(""); });
    res.setAudience("assistant").setPriority(1.0f);

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    res.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_CONTAINS(json.c_str(), "annotations");
    ASSERT_STR_CONTAINS(json.c_str(), "assistant");
    ASSERT_STR_CONTAINS(json.c_str(), "priority");
}

// ════════════════════════════════════════════════════════════════════════
// Resource Template Annotations Tests
// ════════════════════════════════════════════════════════════════════════

TEST(template_annotations_audience) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "Sensor", "Desc", "text/plain",
                             [](const std::map<String, String>&) { return String("42"); });
    tmpl.setAudience("user");

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    tmpl.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_CONTAINS(json.c_str(), "annotations");
    ASSERT_STR_CONTAINS(json.c_str(), "user");
}

TEST(template_annotations_priority) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "Sensor", "Desc", "text/plain",
                             [](const std::map<String, String>&) { return String("42"); });
    tmpl.setPriority(0.3f);

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    tmpl.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_CONTAINS(json.c_str(), "priority");
}

TEST(template_annotations_absent_by_default) {
    MCPResourceTemplate tmpl("sensor://{id}/reading", "Sensor", "Desc", "text/plain",
                             [](const std::map<String, String>&) { return String("42"); });

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    tmpl.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_NOT_CONTAINS(json.c_str(), "annotations");
}

TEST(template_annotations_builder_chain) {
    MCPResourceTemplate tmpl("s://{id}", "S", "D", "text/plain",
                             [](const std::map<String, String>&) { return String(""); });
    MCPResourceTemplate& ref = tmpl.setAudience("user");
    ASSERT(&ref == &tmpl);
    MCPResourceTemplate& ref2 = tmpl.setPriority(0.5f);
    ASSERT(&ref2 == &tmpl);
}

// ════════════════════════════════════════════════════════════════════════
// Resource Annotations Struct Tests
// ════════════════════════════════════════════════════════════════════════

TEST(resource_annotations_default_state) {
    MCPResourceAnnotations ann;
    ASSERT(!ann.hasAnnotations);
    ASSERT(ann.audience.isEmpty());
    ASSERT(ann.priority < 0.0f);
}

TEST(resource_annotations_set_audience_marks_has) {
    MCPResourceAnnotations ann;
    ann.setAudience("user");
    ASSERT(ann.hasAnnotations);
    ASSERT(ann.audience == "user");
}

TEST(resource_annotations_set_priority_marks_has) {
    MCPResourceAnnotations ann;
    ann.setPriority(0.5f);
    ASSERT(ann.hasAnnotations);
    ASSERT(ann.priority >= 0.49f);
}

TEST(resource_annotations_toJson_audience_only) {
    MCPResourceAnnotations ann;
    ann.setAudience("assistant");

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    ann.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_CONTAINS(json.c_str(), "audience");
    ASSERT_STR_CONTAINS(json.c_str(), "assistant");
    ASSERT_STR_NOT_CONTAINS(json.c_str(), "priority");
}

TEST(resource_annotations_toJson_priority_only) {
    MCPResourceAnnotations ann;
    ann.setPriority(0.8f);

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    ann.toJson(obj);

    String json;
    serializeJson(doc, json);
    ASSERT_STR_CONTAINS(json.c_str(), "priority");
    ASSERT_STR_NOT_CONTAINS(json.c_str(), "audience");
}

// ════════════════════════════════════════════════════════════════════════
// Tool Annotations Title Tests (deeper coverage)
// ════════════════════════════════════════════════════════════════════════

TEST(tool_annotations_title_in_list) {
    Server srv("test", 80);
    MCPTool tool("my_tool", "My tool", R"({"type":"object"})",
                 [](const JsonObject&) { return String("ok"); });
    tool.annotations.title = "My Custom Title";
    tool.annotations.hasAnnotations = true;
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "My Custom Title");
}

// ════════════════════════════════════════════════════════════════════════
// Integration: Output Schema + Annotations combined
// ════════════════════════════════════════════════════════════════════════

TEST(tool_output_schema_with_annotations) {
    Server srv("test", 80);
    MCPTool tool("combo", "Has everything",
                 R"({"type":"object","properties":{"x":{"type":"number"}}})",
                 [](const JsonObject&) { return String(R"({"result":"ok"})"); });
    tool.setOutputSchema(R"({"type":"object","properties":{"result":{"type":"string"}}})");
    tool.markReadOnly();
    srv.addTool(tool);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "outputSchema");
    ASSERT_STR_CONTAINS(resp.c_str(), "annotations");
    ASSERT_STR_CONTAINS(resp.c_str(), "readOnlyHint");
}

TEST(structured_content_rich_handler) {
    Server srv("test", 80);
    MCPTool tool("rich_json", "Rich JSON output",
                 R"({"type":"object"})", nullptr);
    tool.setOutputSchema(R"({"type":"object","properties":{"status":{"type":"string"}}})");
    srv.addTool(tool);
    srv.addRichTool("rich_json", "Rich JSON output",
                    R"({"type":"object"})",
                    [](const JsonObject&) {
                        return MCPToolResult::text(R"({"status":"healthy"})");
                    });
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"rich_json","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "structuredContent");
    ASSERT_STR_CONTAINS(resp.c_str(), "healthy");
}

TEST(resource_template_with_annotations_in_server) {
    Server srv("test", 80);
    MCPResourceTemplate tmpl("device://{id}/status", "Status", "Device status", "application/json",
                             [](const std::map<String, String>&) { return String(R"({"online":true})"); });
    tmpl.setAudience("assistant").setPriority(0.6f);
    srv.addResourceTemplate(tmpl);
    initServer(srv);

    String resp = srv._processJsonRpc(R"({"jsonrpc":"2.0","method":"resources/templates/list","id":1})");
    ASSERT_STR_CONTAINS(resp.c_str(), "annotations");
    ASSERT_STR_CONTAINS(resp.c_str(), "assistant");
    ASSERT_STR_CONTAINS(resp.c_str(), "priority");
}

// ── Main ───────────────────────────────────────────────────────────────

int main() {
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
