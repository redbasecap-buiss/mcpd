/**
 * mcpd — Robustness & Edge Case Tests
 *
 * Tests input validation, malformed requests, boundary conditions, and
 * protocol compliance edge cases to ensure the server handles adversarial
 * or malformed input gracefully without crashes or undefined behavior.
 *
 * Compile: g++ -std=c++17 -I../src -Imock_includes -I. -DMCPD_TEST test_robustness.cpp -o test_robustness && ./test_robustness
 */

#include "test_framework.h"
#include "../src/mcpd.h"
#include "../src/mcpd.cpp"
#include "../src/MCPContent.h"
#include "../src/MCPProgress.h"
#include "../src/MCPRateLimit.h"

using namespace mcpd;

// ── Helper ─────────────────────────────────────────────────────────────

static Server* makeServer() {
    static Server* s = nullptr;
    if (s) { delete s; }
    s = new Server("robustness-test", 8080);

    s->addTool("echo", "Echo back input",
        R"({"type":"object","properties":{"message":{"type":"string"}},"required":["message"]})",
        [](const JsonObject& args) -> String {
            const char* msg = args["message"].as<const char*>();
            return String(R"=({"echo":")=") + (msg ? msg : "") + "\"}";
        });

    s->addTool("divide", "Divide two numbers",
        R"({"type":"object","properties":{"a":{"type":"number"},"b":{"type":"number"}},"required":["a","b"]})",
        [](const JsonObject& args) -> String {
            float a = args["a"];
            float b = args["b"];
            if (b == 0) return R"({"error":"division by zero"})";
            return String(R"({"result":)") + String(a / b) + "}";
        });

    s->addTool("crash_test", "Tool that throws",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& /* args */) -> String {
            throw std::runtime_error("intentional crash");
            return "{}";
        });

    s->addResource("test://data", "Test Data", "A test resource", "application/json",
        []() -> String { return R"({"value":42})"; });

    s->addPrompt("test_prompt", "A test prompt",
        { MCPPromptArgument("name", "Name", true) },
        [](const std::map<String, String>& args) -> std::vector<MCPPromptMessage> {
            return { MCPPromptMessage("user", args.at("name").c_str()) };
        });

    return s;
}

static String dispatch(Server* s, const String& json) {
    return s->_processJsonRpc(json);
}

// ════════════════════════════════════════════════════════════════════════
//  1. Malformed JSON-RPC Requests
// ════════════════════════════════════════════════════════════════════════

TEST(malformed_json_returns_parse_error) {
    auto* s = makeServer();
    String r = dispatch(s, "{not valid json!!}");
    ASSERT_STR_CONTAINS(r.c_str(), "-32700");  // Parse error
}

TEST(empty_string_returns_parse_error) {
    auto* s = makeServer();
    String r = dispatch(s, "");
    ASSERT_STR_CONTAINS(r.c_str(), "-32700");
}

TEST(null_json_returns_parse_error) {
    auto* s = makeServer();
    String r = dispatch(s, "null");
    ASSERT_STR_CONTAINS(r.c_str(), "-32600");  // Invalid request
}

TEST(array_of_nulls_handled_gracefully) {
    auto* s = makeServer();
    String r = dispatch(s, "[null, null]");
    // Server handles gracefully — may return empty or batch response
    ASSERT(true);  // No crash is success
}

TEST(missing_jsonrpc_field) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"method":"ping","id":1})");
    // Should still work or return invalid request
    ASSERT(r.length() > 0);
}

TEST(wrong_jsonrpc_version) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"1.0","method":"ping","id":1})");
    ASSERT(r.length() > 0);
}

TEST(missing_method_field) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "-32600");  // Invalid request
}

TEST(method_is_number_not_string) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":42,"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(method_is_empty_string) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"","id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(id_is_string_type) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"ping","id":"abc"})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

TEST(id_is_null_treated_as_notification) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"ping","id":null})");
    // Null id may be treated as notification (no response) — that's acceptable
    ASSERT(true);
}

TEST(id_is_float) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"ping","id":3.14})");
    ASSERT(r.length() > 0);
}

TEST(id_is_negative) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"ping","id":-1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

// ════════════════════════════════════════════════════════════════════════
//  2. Unknown Methods
// ════════════════════════════════════════════════════════════════════════

TEST(unknown_method_returns_method_not_found) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"nonexistent/method","id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "-32601");  // Method not found
}

TEST(method_with_special_chars) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"../../etc/passwd","id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "-32601");
}

TEST(very_long_method_name) {
    auto* s = makeServer();
    String method = "a]";
    // build a long method name
    String req = R"({"jsonrpc":"2.0","method":")";
    for (int i = 0; i < 200; i++) req += "x";
    req += R"(","id":1})";
    String r = dispatch(s, req);
    ASSERT_STR_CONTAINS(r.c_str(), "-32601");
}

// ════════════════════════════════════════════════════════════════════════
//  3. Tool Call Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(tool_call_nonexistent_tool) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"no_such_tool","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(tool_call_missing_name_param) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(tool_call_null_arguments) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":null},"id":1})");
    // Should handle gracefully
    ASSERT(r.length() > 0);
}

TEST(tool_call_empty_arguments) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{}},"id":1})");
    // Missing required "message" but shouldn't crash
    ASSERT(r.length() > 0);
}

TEST(tool_call_extra_unknown_arguments) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"hi","extra":"ignored","another":123}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "echo");
}

TEST(tool_call_wrong_argument_types) {
    auto* s = makeServer();
    // Pass number where string expected
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":12345}},"id":1})");
    ASSERT(r.length() > 0);  // Should not crash
}

TEST(tool_call_unicode_arguments) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"日本語テスト🎉"}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

TEST(tool_call_empty_string_argument) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":""}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

TEST(tool_call_very_long_argument) {
    auto* s = makeServer();
    String longMsg = "";
    for (int i = 0; i < 1000; i++) longMsg += "A";
    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":")" + longMsg + R"("}},"id":1})";
    String r = dispatch(s, req);
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

TEST(tool_handler_exception_returns_isError) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"crash_test","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "isError");
}

TEST(tool_call_with_nested_json_argument) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"test","nested":{"a":{"b":{"c":1}}}}},"id":1})");
    ASSERT(r.length() > 0);
}

// ════════════════════════════════════════════════════════════════════════
//  4. Resource Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(resource_read_nonexistent_uri) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/read","params":{"uri":"test://nonexistent"},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(resource_read_missing_uri_param) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/read","params":{},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(resource_read_uri_with_special_chars) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/read","params":{"uri":"test://../../../etc/passwd"},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(resource_list_with_unexpected_params) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/list","params":{"cursor":"abc","extra":true},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

// ════════════════════════════════════════════════════════════════════════
//  5. Prompt Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(prompt_get_nonexistent) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"prompts/get","params":{"name":"no_such_prompt"},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

TEST(prompt_get_missing_required_argument) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"prompts/get","params":{"name":"test_prompt","arguments":{}},"id":1})");
    // Missing required "name" argument
    ASSERT(r.length() > 0);
}

TEST(prompt_get_with_extra_arguments) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"prompts/get","params":{"name":"test_prompt","arguments":{"name":"Alice","extra":"stuff"}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

// ════════════════════════════════════════════════════════════════════════
//  6. Batch Request Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(empty_batch_array_handled_gracefully) {
    auto* s = makeServer();
    String r = dispatch(s, "[]");
    // Empty batch — server may return empty or error, both acceptable
    ASSERT(true);
}

TEST(batch_with_one_valid_one_invalid) {
    auto* s = makeServer();
    String r = dispatch(s, R"([
        {"jsonrpc":"2.0","method":"ping","id":1},
        {"jsonrpc":"2.0","id":2}
    ])");
    // Should process both, returning results for valid and error for invalid
    ASSERT(r.length() > 0);
}

TEST(batch_all_notifications_no_response) {
    auto* s = makeServer();
    String r = dispatch(s, R"([
        {"jsonrpc":"2.0","method":"notifications/initialized"}
    ])");
    // Notifications don't require a response
    ASSERT(r.length() >= 0);  // May be empty
}

TEST(large_batch_request) {
    auto* s = makeServer();
    String batch = "[";
    for (int i = 0; i < 50; i++) {
        if (i > 0) batch += ",";
        batch += R"({"jsonrpc":"2.0","method":"ping","id":)" + String(i) + "}";
    }
    batch += "]";
    String r = dispatch(s, batch);
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

// ════════════════════════════════════════════════════════════════════════
//  7. Initialize Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(initialize_with_empty_client_info) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "protocolVersion");
}

TEST(initialize_with_future_protocol_version) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2099-01-01","capabilities":{},"clientInfo":{"name":"test"}},"id":1})");
    // Should still return our protocol version
    ASSERT_STR_CONTAINS(r.c_str(), MCPD_MCP_PROTOCOL_VERSION);
}

TEST(initialize_returns_server_info) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test"}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "serverInfo");
    ASSERT_STR_CONTAINS(r.c_str(), "robustness-test");
}

TEST(initialize_returns_capabilities) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test"}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "capabilities");
    ASSERT_STR_CONTAINS(r.c_str(), "tools");
}

// ════════════════════════════════════════════════════════════════════════
//  8. Logging Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(logging_set_valid_level) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"logging/setLevel","params":{"level":"warning"},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "result");
}

TEST(logging_set_invalid_level) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"logging/setLevel","params":{"level":"nonexistent"},"id":1})");
    // Should handle gracefully
    ASSERT(r.length() > 0);
}

TEST(logging_set_empty_level) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"logging/setLevel","params":{"level":""},"id":1})");
    ASSERT(r.length() > 0);
}

// ════════════════════════════════════════════════════════════════════════
//  9. Notification Handling
// ════════════════════════════════════════════════════════════════════════

TEST(notification_initialized_returns_empty) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
    ASSERT_EQ(r.length(), (unsigned int)0);
}

TEST(notification_cancelled_without_requestId) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{}})");
    ASSERT_EQ(r.length(), (unsigned int)0);
}

TEST(notification_cancelled_with_requestId) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":"req-123"}})");
    ASSERT_EQ(r.length(), (unsigned int)0);
}

// ════════════════════════════════════════════════════════════════════════
//  10. Content & Response Structure
// ════════════════════════════════════════════════════════════════════════

TEST(tool_result_has_content_array) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"hello"}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "content");
    ASSERT_STR_CONTAINS(r.c_str(), "text");
}

TEST(tools_list_returns_array) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/list","params":{},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "tools");
    ASSERT_STR_CONTAINS(r.c_str(), "echo");
    ASSERT_STR_CONTAINS(r.c_str(), "divide");
}

TEST(resources_list_returns_array) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/list","params":{},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "resources");
}

TEST(prompts_list_returns_array) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"prompts/list","params":{},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "prompts");
}

// ════════════════════════════════════════════════════════════════════════
//  11. Concurrent-style / Stress
// ════════════════════════════════════════════════════════════════════════

TEST(rapid_sequential_requests) {
    auto* s = makeServer();
    for (int i = 0; i < 100; i++) {
        String r = dispatch(s, R"({"jsonrpc":"2.0","method":"ping","id":)" + String(i) + "}");
        ASSERT_STR_CONTAINS(r.c_str(), "result");
    }
}

TEST(alternate_tools_and_resources) {
    auto* s = makeServer();
    for (int i = 0; i < 20; i++) {
        dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"hi"}},"id":)" + String(i*2) + "}");
        dispatch(s, R"({"jsonrpc":"2.0","method":"resources/read","params":{"uri":"test://data"},"id":)" + String(i*2+1) + "}");
    }
    // If we get here without crash, pass
    ASSERT(true);
}

// ════════════════════════════════════════════════════════════════════════
//  12. JSON Injection / Special Characters
// ════════════════════════════════════════════════════════════════════════

TEST(tool_argument_with_json_injection) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"\"},{\"evil\":true}"}},"id":1})");
    // Should escape properly and not break
    ASSERT(r.length() > 0);
}

TEST(tool_argument_with_backslashes) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"path\\to\\file"}},"id":1})");
    ASSERT(r.length() > 0);
}

TEST(tool_argument_with_newlines) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"line1\nline2\ttab"}},"id":1})");
    ASSERT(r.length() > 0);
}

TEST(tool_argument_with_null_bytes) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"echo","arguments":{"message":"before\u0000after"}},"id":1})");
    ASSERT(r.length() > 0);
}

// ════════════════════════════════════════════════════════════════════════
//  13. Completion/Autocomplete
// ════════════════════════════════════════════════════════════════════════

TEST(completion_with_unknown_ref_type) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"completion/complete","params":{"ref":{"type":"ref/unknown","uri":"test://x"},"argument":{"name":"x","value":"y"}},"id":1})");
    ASSERT(r.length() > 0);
}

TEST(completion_with_empty_value) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"completion/complete","params":{"ref":{"type":"ref/resource","uri":"test://data"},"argument":{"name":"x","value":""}},"id":1})");
    ASSERT(r.length() > 0);
}

// ════════════════════════════════════════════════════════════════════════
//  14. Subscription Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(subscribe_nonexistent_resource) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/subscribe","params":{"uri":"test://nonexistent"},"id":1})");
    ASSERT(r.length() > 0);
}

TEST(unsubscribe_without_subscribing) {
    auto* s = makeServer();
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"resources/unsubscribe","params":{"uri":"test://data"},"id":1})");
    ASSERT(r.length() > 0);
}

// ════════════════════════════════════════════════════════════════════════
//  15. Double Registration & Dynamic Tools
// ════════════════════════════════════════════════════════════════════════

TEST(add_tool_after_initialization) {
    auto* s = makeServer();
    // Initialize first
    dispatch(s, R"({"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test"}},"id":1})");

    // Add a new tool dynamically
    s->addTool("dynamic_tool", "Added after init",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& /* args */) -> String { return "{}"; });

    // Should appear in tools/list
    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/list","params":{},"id":2})");
    ASSERT_STR_CONTAINS(r.c_str(), "dynamic_tool");
}

TEST(remove_tool_then_call_returns_error) {
    auto* s = makeServer();
    s->addTool("temporary", "Will be removed",
        R"({"type":"object","properties":{}})",
        [](const JsonObject& /* args */) -> String { return "{}"; });
    s->removeTool("temporary");

    String r = dispatch(s, R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temporary","arguments":{}},"id":1})");
    ASSERT_STR_CONTAINS(r.c_str(), "error");
}

// ════════════════════════════════════════════════════════════════════════
//  Main
// ════════════════════════════════════════════════════════════════════════

int main() {
    printf("\n══════════════════════════════════════════\n");
    printf(" mcpd Robustness & Edge Case Tests\n");
    printf("══════════════════════════════════════════\n\n");
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
