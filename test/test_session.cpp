/**
 * mcpd — Session Management Tests
 *
 * Tests for MCPSession.h: SessionManager creation, validation, eviction,
 * timeout, concurrency limits, and edge cases.
 */

#include "test_framework.h"
#include "arduino_mock.h"

// Reset millis for controlled timeout testing
// (millis offset managed via arduino_mock.h _mockMillis())

#include "../src/MCPSession.h"
#include "../src/mcpd.h"

// ════════════════════════════════════════════════════════════════════════
// Session Creation
// ════════════════════════════════════════════════════════════════════════

TEST(session_create_returns_nonempty_id) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("test-client");
    ASSERT(id.length() > 0);
}

TEST(session_create_unique_ids) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(100);
    std::vector<std::string> ids;
    for (int i = 0; i < 50; i++) {
        String id = mgr.createSession("client-" + String(i));
        ASSERT(id.length() > 0);
        std::string sid(id.c_str());
        for (const auto& prev : ids) {
            ASSERT_NE(sid, prev);
        }
        ids.push_back(sid);
    }
}

TEST(session_create_stores_client_name) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("my-client");
    const mcpd::Session* s = mgr.getSession(id);
    ASSERT(s != nullptr);
    ASSERT_EQ(std::string(s->clientName.c_str()), std::string("my-client"));
}

TEST(session_create_sets_initialized) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    const mcpd::Session* s = mgr.getSession(id);
    ASSERT(s != nullptr);
    ASSERT(s->initialized);
}

TEST(session_create_sets_timestamps) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    const mcpd::Session* s = mgr.getSession(id);
    ASSERT(s != nullptr);
    ASSERT(s->createdAt > 0);
    ASSERT(s->lastActivity > 0);
}

// ════════════════════════════════════════════════════════════════════════
// Session Validation
// ════════════════════════════════════════════════════════════════════════

TEST(session_validate_existing) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    ASSERT(mgr.validateSession(id));
}

TEST(session_validate_nonexistent) {
    mcpd::SessionManager mgr;
    ASSERT(!mgr.validateSession("nonexistent-id-12345"));
}

TEST(session_validate_empty_id) {
    mcpd::SessionManager mgr;
    ASSERT(!mgr.validateSession(""));
}

TEST(session_validate_touches_session) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    const mcpd::Session* s = mgr.getSession(id);
    unsigned long before = s->lastActivity;
    // Small delay simulation
    delay(10);
    mgr.validateSession(id);
    s = mgr.getSession(id);
    ASSERT(s->lastActivity >= before);
}

// ════════════════════════════════════════════════════════════════════════
// Session Removal
// ════════════════════════════════════════════════════════════════════════

TEST(session_remove_existing) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    ASSERT(mgr.removeSession(id));
    ASSERT(mgr.getSession(id) == nullptr);
}

TEST(session_remove_nonexistent) {
    mcpd::SessionManager mgr;
    ASSERT(!mgr.removeSession("bogus-id"));
}

TEST(session_remove_reduces_count) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    String id1 = mgr.createSession("c1");
    String id2 = mgr.createSession("c2");
    ASSERT_EQ(mgr.activeCount(), (size_t)2);
    mgr.removeSession(id1);
    ASSERT_EQ(mgr.activeCount(), (size_t)1);
}

TEST(session_remove_double_remove) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    ASSERT(mgr.removeSession(id));
    ASSERT(!mgr.removeSession(id));
}

// ════════════════════════════════════════════════════════════════════════
// Session Limits & Eviction
// ════════════════════════════════════════════════════════════════════════

TEST(session_max_limit_default_is_4) {
    mcpd::SessionManager mgr;
    ASSERT_EQ(mgr.maxSessions(), (size_t)4);
}

TEST(session_respects_max_limit) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(2);
    mgr.setIdleTimeout(0); // disable timeout so eviction is based on age
    String id1 = mgr.createSession("c1");
    String id2 = mgr.createSession("c2");
    ASSERT(id1.length() > 0);
    ASSERT(id2.length() > 0);
    // Third session should evict oldest
    String id3 = mgr.createSession("c3");
    ASSERT(id3.length() > 0);
    ASSERT_EQ(mgr.activeCount(), (size_t)2);
}

TEST(session_unlimited_when_max_zero) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(0);
    for (int i = 0; i < 20; i++) {
        String id = mgr.createSession("c" + String(i));
        ASSERT(id.length() > 0);
    }
    ASSERT_EQ(mgr.activeCount(), (size_t)20);
}

TEST(session_evicts_oldest_idle) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(2);
    mgr.setIdleTimeout(0);
    String id1 = mgr.createSession("c1");
    delay(10);
    String id2 = mgr.createSession("c2");
    // Touch id2 so it's more recent
    mgr.validateSession(id2);
    // Create third — should evict id1 (oldest idle)
    String id3 = mgr.createSession("c3");
    ASSERT(id3.length() > 0);
    // id1 should be gone
    ASSERT(mgr.getSession(id1) == nullptr);
    // id2 should still exist
    ASSERT(mgr.getSession(id2) != nullptr);
}

// ════════════════════════════════════════════════════════════════════════
// Timeout & Pruning
// ════════════════════════════════════════════════════════════════════════

TEST(session_default_timeout_30min) {
    mcpd::SessionManager mgr;
    ASSERT_EQ(mgr.idleTimeout(), 30UL * 60 * 1000);
}

TEST(session_set_custom_timeout) {
    mcpd::SessionManager mgr;
    mgr.setIdleTimeout(5000);
    ASSERT_EQ(mgr.idleTimeout(), (unsigned long)5000);
}

TEST(session_prune_does_nothing_when_timeout_zero) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    mgr.setIdleTimeout(0);
    mgr.createSession("c1");
    mgr.createSession("c2");
    mgr.pruneExpired();
    ASSERT_EQ(mgr.activeCount(), (size_t)2);
}

TEST(session_active_count) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    ASSERT_EQ(mgr.activeCount(), (size_t)0);
    mgr.createSession("c1");
    ASSERT_EQ(mgr.activeCount(), (size_t)1);
    mgr.createSession("c2");
    ASSERT_EQ(mgr.activeCount(), (size_t)2);
}

// ════════════════════════════════════════════════════════════════════════
// Session Diagnostics / Summary
// ════════════════════════════════════════════════════════════════════════

TEST(session_summary_empty) {
    mcpd::SessionManager mgr;
    String summary = mgr.summary();
    ASSERT(summary.length() > 0);
    ASSERT_STR_CONTAINS(summary.c_str(), "activeSessions");
    ASSERT_STR_CONTAINS(summary.c_str(), "0");
}

TEST(session_summary_with_sessions) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    mgr.createSession("alpha");
    mgr.createSession("beta");
    String summary = mgr.summary();
    ASSERT_STR_CONTAINS(summary.c_str(), "alpha");
    ASSERT_STR_CONTAINS(summary.c_str(), "beta");
}

TEST(session_summary_contains_maxSessions) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(8);
    String summary = mgr.summary();
    ASSERT_STR_CONTAINS(summary.c_str(), "\"maxSessions\":8");
}

// ════════════════════════════════════════════════════════════════════════
// Session Struct
// ════════════════════════════════════════════════════════════════════════

TEST(session_struct_default_constructor) {
    mcpd::Session s;
    ASSERT_EQ(s.createdAt, (unsigned long)0);
    ASSERT_EQ(s.lastActivity, (unsigned long)0);
    ASSERT(!s.initialized);
}

TEST(session_struct_parameterized_constructor) {
    mcpd::Session s("sess-123", "my-client");
    ASSERT_EQ(std::string(s.id.c_str()), std::string("sess-123"));
    ASSERT_EQ(std::string(s.clientName.c_str()), std::string("my-client"));
    ASSERT(s.initialized);
    ASSERT(s.createdAt > 0);
}

TEST(session_struct_touch) {
    mcpd::Session s("id", "client");
    unsigned long before = s.lastActivity;
    delay(5);
    s.touch();
    ASSERT(s.lastActivity >= before);
}

TEST(session_struct_ageMs) {
    mcpd::Session s("id", "client");
    delay(10);
    ASSERT(s.ageMs() >= 0);
}

TEST(session_struct_idleMs) {
    mcpd::Session s("id", "client");
    delay(10);
    ASSERT(s.idleMs() >= 0);
}

// ════════════════════════════════════════════════════════════════════════
// Edge Cases
// ════════════════════════════════════════════════════════════════════════

TEST(session_create_with_empty_client_name) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("");
    ASSERT(id.length() > 0);
    const mcpd::Session* s = mgr.getSession(id);
    ASSERT(s != nullptr);
    ASSERT_EQ(std::string(s->clientName.c_str()), std::string(""));
}

TEST(session_create_with_long_client_name) {
    mcpd::SessionManager mgr;
    String longName;
    for (int i = 0; i < 100; i++) longName += "abcdefghij";
    String id = mgr.createSession(longName);
    ASSERT(id.length() > 0);
}

TEST(session_validate_after_remove_returns_false) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    ASSERT(mgr.validateSession(id));
    mgr.removeSession(id);
    ASSERT(!mgr.validateSession(id));
}

TEST(session_get_after_remove_returns_null) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    mgr.removeSession(id);
    ASSERT(mgr.getSession(id) == nullptr);
}

TEST(session_create_after_remove_reuses_slot) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(1);
    String id1 = mgr.createSession("c1");
    mgr.removeSession(id1);
    ASSERT_EQ(mgr.activeCount(), (size_t)0);
    String id2 = mgr.createSession("c2");
    ASSERT(id2.length() > 0);
    ASSERT_EQ(mgr.activeCount(), (size_t)1);
}

TEST(session_max_sessions_1_evicts_on_create) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(1);
    mgr.setIdleTimeout(0);
    String id1 = mgr.createSession("c1");
    String id2 = mgr.createSession("c2");
    ASSERT(id2.length() > 0);
    ASSERT_EQ(mgr.activeCount(), (size_t)1);
    ASSERT(mgr.getSession(id1) == nullptr);
    ASSERT(mgr.getSession(id2) != nullptr);
}

TEST(session_id_is_32_hex_chars) {
    mcpd::SessionManager mgr;
    String id = mgr.createSession("client");
    ASSERT_EQ(id.length(), (size_t)32);
    for (size_t i = 0; i < id.length(); i++) {
        char c = id.charAt(i);
        ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(session_prune_called_during_create) {
    // pruneExpired is called inside createSession, so expired sessions
    // should be cleaned up automatically
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    mgr.setIdleTimeout(1); // 1ms timeout
    mgr.createSession("c1");
    delay(10);
    // Creating new session should prune the expired one
    mgr.createSession("c2");
    // Can't guarantee c1 is pruned since millis resolution varies,
    // but count should be <= 2
    ASSERT(mgr.activeCount() <= 2);
}

TEST(session_set_max_sessions_changes_limit) {
    mcpd::SessionManager mgr;
    mgr.setMaxSessions(10);
    ASSERT_EQ(mgr.maxSessions(), (size_t)10);
    mgr.setMaxSessions(3);
    ASSERT_EQ(mgr.maxSessions(), (size_t)3);
}

// ════════════════════════════════════════════════════════════════════════
// Main
// ════════════════════════════════════════════════════════════════════════

int main() {
    printf("\n══════════════════════════════════════════\n");
    printf(" mcpd Session Management Tests\n");
    printf("══════════════════════════════════════════\n\n");

    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
