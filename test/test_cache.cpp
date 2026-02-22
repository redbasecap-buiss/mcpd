/**
 * mcpd — Tool Result Cache tests
 */

#include "arduino_mock.h"
#include "test_framework.h"

// Include mcpd implementation
#include "mcpd.cpp"

using namespace mcpd;

// ── Unit Tests: ToolResultCache ────────────────────────────────────────

TEST(cache_default_disabled) {
    ToolResultCache cache;
    ASSERT(!cache.isEnabled());
    ASSERT_EQ((int)cache.size(), 0);
}

TEST(cache_enable_disable) {
    ToolResultCache cache;
    cache.setEnabled(true);
    ASSERT(cache.isEnabled());
    cache.setEnabled(false);
    ASSERT(!cache.isEnabled());
}

TEST(cache_set_tool_ttl) {
    ToolResultCache cache;
    cache.setToolTTL("temperature_read", 2000);
    ASSERT_EQ((int)cache.getToolTTL("temperature_read"), 2000);
    ASSERT(cache.isToolCached("temperature_read"));
    ASSERT(!cache.isToolCached("gpio_write"));
}

TEST(cache_remove_ttl_with_zero) {
    ToolResultCache cache;
    cache.setToolTTL("temp", 1000);
    ASSERT(cache.isToolCached("temp"));
    cache.setToolTTL("temp", 0);
    ASSERT(!cache.isToolCached("temp"));
}

TEST(cache_miss_when_disabled) {
    ToolResultCache cache;
    cache.setToolTTL("temp", 5000);
    String result;
    bool isError;
    ASSERT(!cache.get("temp", "{}", result, isError));
}

TEST(cache_miss_when_empty) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 5000);
    String result;
    bool isError;
    ASSERT(!cache.get("temp", "{}", result, isError));
}

TEST(cache_put_and_get) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);

    cache.put("temp", "{}", "{\"value\":22.5}", false);

    String result;
    bool isError;
    ASSERT(cache.get("temp", "{}", result, isError));
    ASSERT_STR_CONTAINS(result.c_str(), "22.5");
    ASSERT(!isError);
}

TEST(cache_put_ignores_unconfigured_tool) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.put("gpio", "{\"pin\":5}", "HIGH", false);
    ASSERT_EQ((int)cache.size(), 0);
}

TEST(cache_put_ignores_when_disabled) {
    ToolResultCache cache;
    cache.setToolTTL("temp", 5000);
    cache.put("temp", "{}", "result", false);
    ASSERT_EQ((int)cache.size(), 0);
}

TEST(cache_different_args_different_entries) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("gpio_read", 50000);

    cache.put("gpio_read", "{\"pin\":5}", "HIGH", false);
    cache.put("gpio_read", "{\"pin\":6}", "LOW", false);

    ASSERT_EQ((int)cache.size(), 2);

    String result;
    bool isError;
    ASSERT(cache.get("gpio_read", "{\"pin\":5}", result, isError));
    ASSERT_STR_CONTAINS(result.c_str(), "HIGH");

    ASSERT(cache.get("gpio_read", "{\"pin\":6}", result, isError));
    ASSERT_STR_CONTAINS(result.c_str(), "LOW");
}

TEST(cache_error_results) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("sensor", 50000);

    cache.put("sensor", "{}", "Sensor timeout", true);

    String result;
    bool isError;
    ASSERT(cache.get("sensor", "{}", result, isError));
    ASSERT(isError);
    ASSERT_STR_CONTAINS(result.c_str(), "timeout");
}

TEST(cache_invalidate_tool) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);
    cache.setToolTTL("gpio", 50000);

    cache.put("temp", "{\"id\":1}", "20", false);
    cache.put("temp", "{\"id\":2}", "21", false);
    cache.put("gpio", "{\"pin\":5}", "HIGH", false);

    ASSERT_EQ((int)cache.size(), 3);

    cache.invalidateTool("temp");
    ASSERT_EQ((int)cache.size(), 1);

    String result;
    bool isError;
    ASSERT(!cache.get("temp", "{\"id\":1}", result, isError));
    ASSERT(cache.get("gpio", "{\"pin\":5}", result, isError));
}

TEST(cache_invalidate_specific) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);

    cache.put("temp", "{\"id\":1}", "20", false);
    cache.put("temp", "{\"id\":2}", "21", false);

    cache.invalidate("temp", "{\"id\":1}");
    ASSERT_EQ((int)cache.size(), 1);

    String result;
    bool isError;
    ASSERT(!cache.get("temp", "{\"id\":1}", result, isError));
    ASSERT(cache.get("temp", "{\"id\":2}", result, isError));
}

TEST(cache_clear) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);

    cache.put("temp", "{}", "20", false);
    cache.put("temp", "{\"id\":1}", "21", false);

    cache.clear();
    ASSERT_EQ((int)cache.size(), 0);
    ASSERT_EQ((int)cache.hits(), 0);
    ASSERT_EQ((int)cache.misses(), 0);
}

TEST(cache_stats) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);

    cache.put("temp", "{}", "20", false);
    String result;
    bool isError;
    cache.get("temp", "{}", result, isError);   // hit
    cache.get("temp", "{}", result, isError);   // hit
    cache.get("temp", "{\"x\":1}", result, isError);  // miss (not stored)

    ASSERT_EQ((int)cache.hits(), 2);
    ASSERT_EQ((int)cache.misses(), 1);
    ASSERT(cache.hitRate() > 0.6f);
}

TEST(cache_stats_json) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 50000);

    String json = cache.statsJson();
    ASSERT_STR_CONTAINS(json.c_str(), "\"enabled\":true");
    ASSERT_STR_CONTAINS(json.c_str(), "\"entries\":0");
    ASSERT_STR_CONTAINS(json.c_str(), "\"toolCount\":1");
}

TEST(cache_max_entries_eviction) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setMaxEntries(3);
    cache.setToolTTL("temp", 500000);

    cache.put("temp", "{\"id\":1}", "v1", false);
    cache.put("temp", "{\"id\":2}", "v2", false);
    cache.put("temp", "{\"id\":3}", "v3", false);
    ASSERT_EQ((int)cache.size(), 3);

    cache.put("temp", "{\"id\":4}", "v4", false);
    ASSERT_EQ((int)cache.size(), 3);

    String result;
    bool isError;
    ASSERT(cache.get("temp", "{\"id\":4}", result, isError));
}

TEST(cache_ttl_expiry) {
    ToolResultCache cache;
    cache.setEnabled(true);
    cache.setToolTTL("temp", 100);

    cache.put("temp", "{}", "20", false);

    String result;
    bool isError;
    ASSERT(cache.get("temp", "{}", result, isError));

    // Simulate time passing
    _mockMillis() += 150;

    ASSERT(!cache.get("temp", "{}", result, isError));
}

TEST(cache_entry_is_valid) {
    CacheEntry entry;
    entry.result = "test";
    entry.cachedAt = millis();
    entry.ttlMs = 100000;
    entry.isError = false;

    ASSERT(entry.isValid());

    // Zero TTL is always invalid
    entry.ttlMs = 0;
    ASSERT(!entry.isValid());
    ASSERT_EQ((int)entry.remainingMs(), 0);
}

TEST(cache_entry_expired) {
    CacheEntry entry;
    entry.result = "test";
    entry.cachedAt = millis();
    entry.ttlMs = 100;
    entry.isError = false;

    _mockMillis() += 200;

    ASSERT(!entry.isValid());
    ASSERT_EQ((int)entry.remainingMs(), 0);
}

// ── Integration Tests: Server with Cache ────────────────────────────────

TEST(server_cache_disabled_by_default) {
    Server server("test");
    ASSERT(!server.isCacheEnabled());
}

TEST(server_cache_enable) {
    Server server("test");
    server.cache().setToolTTL("temp", 2000);
    server.enableCache();
    ASSERT(server.isCacheEnabled());
    ASSERT(server.cache().isToolCached("temp"));
}

TEST(server_cache_hit_skips_handler) {
    Server server("test");
    int callCount = 0;

    server.addTool("temp", "Read temperature",
        R"({"type":"object","properties":{"unit":{"type":"string"}}})",
        [&](const JsonObject& args) -> String {
            callCount++;
            return "{\"temp\":22.5}";
        });

    server.cache().setToolTTL("temp", 50000);
    server.enableCache();

    String req1 = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{"unit":"C"}},"id":1})";
    String resp1 = server._processJsonRpc(req1);
    ASSERT_EQ(callCount, 1);
    ASSERT_STR_CONTAINS(resp1.c_str(), "22.5");

    String req2 = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{"unit":"C"}},"id":2})";
    String resp2 = server._processJsonRpc(req2);
    ASSERT_EQ(callCount, 1);  // Cache hit!
    ASSERT_STR_CONTAINS(resp2.c_str(), "22.5");

    // Different args — cache miss
    String req3 = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{"unit":"F"}},"id":3})";
    server._processJsonRpc(req3);
    ASSERT_EQ(callCount, 2);
}

TEST(server_cache_expired_calls_handler) {
    Server server("test");
    int callCount = 0;

    server.addTool("temp", "Read temperature",
        R"({"type":"object"})",
        [&](const JsonObject& args) -> String {
            callCount++;
            return String("{\"temp\":") + String(20 + callCount) + "}";
        });

    server.cache().setToolTTL("temp", 100);
    server.enableCache();

    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{}},"id":1})";
    server._processJsonRpc(req);
    ASSERT_EQ(callCount, 1);

    _mockMillis() += 200;

    String resp = server._processJsonRpc(req);
    ASSERT_EQ(callCount, 2);
    ASSERT_STR_CONTAINS(resp.c_str(), "22");
}

TEST(server_cache_unconfigured_not_cached) {
    Server server("test");
    int callCount = 0;

    server.addTool("gpio", "Write GPIO",
        R"({"type":"object"})",
        [&](const JsonObject& args) -> String {
            callCount++;
            return "OK";
        });

    server.enableCache();

    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"gpio","arguments":{}},"id":1})";
    server._processJsonRpc(req);
    server._processJsonRpc(req);
    ASSERT_EQ(callCount, 2);
}

TEST(server_cache_with_after_hook) {
    Server server("test");
    int hookCount = 0;

    server.addTool("temp", "Read",
        R"({"type":"object"})",
        [](const JsonObject&) -> String { return "22"; });

    server.cache().setToolTTL("temp", 50000);
    server.enableCache();

    server.onAfterToolCall([&](const Server::ToolCallContext& ctx) {
        hookCount++;
    });

    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{}},"id":1})";
    server._processJsonRpc(req);
    server._processJsonRpc(req);

    ASSERT_EQ(hookCount, 2);
}

TEST(server_cache_stats_accessible) {
    Server server("test");
    server.cache().setToolTTL("temp", 50000);
    server.enableCache();

    server.addTool("temp", "Read",
        R"({"type":"object"})",
        [](const JsonObject&) -> String { return "22"; });

    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp","arguments":{}},"id":1})";
    server._processJsonRpc(req);
    server._processJsonRpc(req);

    ASSERT_EQ((int)server.cache().hits(), 1);
    ASSERT_EQ((int)server.cache().misses(), 1);
    ASSERT_EQ((int)server.cache().size(), 1);
}

TEST(server_cache_invalidate_from_handler) {
    Server server("test");
    int readCount = 0;

    server.addTool("temp_read", "Read temperature",
        R"({"type":"object"})",
        [&](const JsonObject&) -> String {
            readCount++;
            return String("{\"temp\":") + String(20 + readCount) + "}";
        });

    server.addTool("temp_reset", "Reset sensor",
        R"({"type":"object"})",
        [&](const JsonObject&) -> String {
            server.cache().invalidateTool("temp_read");
            return "OK";
        });

    server.cache().setToolTTL("temp_read", 500000);
    server.enableCache();

    String readReq = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp_read","arguments":{}},"id":1})";
    String resetReq = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"temp_reset","arguments":{}},"id":2})";

    server._processJsonRpc(readReq);
    ASSERT_EQ(readCount, 1);
    server._processJsonRpc(readReq);
    ASSERT_EQ(readCount, 1);  // cache hit

    server._processJsonRpc(resetReq);  // invalidates

    server._processJsonRpc(readReq);
    ASSERT_EQ(readCount, 2);
}

TEST(server_cache_with_rich_tool) {
    Server server("test");
    int callCount = 0;

    server.addRichTool("camera", "Capture",
        R"({"type":"object"})",
        [&](const JsonObject&) -> MCPToolResult {
            callCount++;
            return MCPToolResult::text("{\"status\":\"ok\"}");
        });

    server.cache().setToolTTL("camera", 50000);
    server.enableCache();

    String req = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"camera","arguments":{}},"id":1})";
    server._processJsonRpc(req);
    server._processJsonRpc(req);
    ASSERT_EQ(callCount, 1);
}

// ═══════════════════════════════════════════════════════════════════════

int main() {
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
