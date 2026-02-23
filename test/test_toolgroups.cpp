#include "test_framework.h"
#include "../src/mcpd.h"
#include "../src/mcpd.cpp"

// ============================================================================
// Tool Group Tests
// ============================================================================

// --- Basic Group Creation ---

TEST(ToolGroup_CreateGroup) {
    mcpd::ToolGroupManager mgr;
    ASSERT_TRUE(mgr.createGroup("sensors", "All sensor tools"));
    ASSERT_TRUE(mgr.hasGroup("sensors"));
    ASSERT_EQ((int)mgr.groupCount(), 1);
}

TEST(ToolGroup_CreateDuplicate) {
    mcpd::ToolGroupManager mgr;
    ASSERT_TRUE(mgr.createGroup("sensors"));
    ASSERT_FALSE(mgr.createGroup("sensors"));  // duplicate
    ASSERT_EQ((int)mgr.groupCount(), 1);
}

TEST(ToolGroup_RemoveGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    ASSERT_TRUE(mgr.removeGroup("sensors"));
    ASSERT_FALSE(mgr.hasGroup("sensors"));
    ASSERT_EQ((int)mgr.groupCount(), 0);
}

TEST(ToolGroup_RemoveNonexistent) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.removeGroup("nonexistent"));
}

TEST(ToolGroup_GroupDescription) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors", "Temperature and humidity sensors");
    const mcpd::ToolGroup* g = mgr.getGroup("sensors");
    ASSERT_TRUE(g != nullptr);
    ASSERT_STR_CONTAINS(g->description.c_str(), "Temperature");
}

// --- Adding/Removing Tools ---

TEST(ToolGroup_AddToolToGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    ASSERT_TRUE(mgr.addToolToGroup("temperature_read", "sensors"));
    auto tools = mgr.getToolsInGroup("sensors");
    ASSERT_EQ((int)tools.size(), 1);
    ASSERT_TRUE(tools.count("temperature_read") > 0);
}

TEST(ToolGroup_AddToolDuplicate) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    ASSERT_TRUE(mgr.addToolToGroup("temp", "sensors"));
    ASSERT_FALSE(mgr.addToolToGroup("temp", "sensors"));  // already in
}

TEST(ToolGroup_AddToolAutoCreatesGroup) {
    mcpd::ToolGroupManager mgr;
    ASSERT_TRUE(mgr.addToolToGroup("temp", "sensors"));
    ASSERT_TRUE(mgr.hasGroup("sensors"));
}

TEST(ToolGroup_RemoveToolFromGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    ASSERT_TRUE(mgr.removeToolFromGroup("temp", "sensors"));
    auto tools = mgr.getToolsInGroup("sensors");
    ASSERT_EQ((int)tools.size(), 0);
}

TEST(ToolGroup_RemoveToolFromNonexistentGroup) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.removeToolFromGroup("temp", "nonexistent"));
}

TEST(ToolGroup_RemoveToolNotInGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    ASSERT_FALSE(mgr.removeToolFromGroup("temp", "sensors"));
}

// --- Enable/Disable Groups ---

TEST(ToolGroup_EnableDisable) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    ASSERT_TRUE(mgr.isGroupEnabled("sensors"));
    ASSERT_TRUE(mgr.disableGroup("sensors"));
    ASSERT_FALSE(mgr.isGroupEnabled("sensors"));
    ASSERT_TRUE(mgr.enableGroup("sensors"));
    ASSERT_TRUE(mgr.isGroupEnabled("sensors"));
}

TEST(ToolGroup_EnableNonexistent) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.enableGroup("nonexistent"));
}

TEST(ToolGroup_DisableNonexistent) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.disableGroup("nonexistent"));
}

TEST(ToolGroup_IsGroupEnabledNonexistent) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.isGroupEnabled("nonexistent"));
}

// --- Tool Group Disabled Logic ---

TEST(ToolGroup_ToolInDisabledGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    mgr.disableGroup("sensors");
    ASSERT_TRUE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_ToolInEnabledGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    ASSERT_FALSE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_ToolNoGroup) {
    mcpd::ToolGroupManager mgr;
    ASSERT_FALSE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_ToolInMultipleGroups_OneEnabled) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.createGroup("critical");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("temp", "critical");
    mgr.disableGroup("sensors");
    // Tool should NOT be disabled — it's in "critical" which is enabled
    ASSERT_FALSE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_ToolInMultipleGroups_AllDisabled) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.createGroup("critical");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("temp", "critical");
    mgr.disableGroup("sensors");
    mgr.disableGroup("critical");
    ASSERT_TRUE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_ReenableGroupReenablesTool) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    mgr.disableGroup("sensors");
    ASSERT_TRUE(mgr.isToolGroupDisabled("temp"));
    mgr.enableGroup("sensors");
    ASSERT_FALSE(mgr.isToolGroupDisabled("temp"));
}

// --- Reverse Lookup ---

TEST(ToolGroup_GetGroupsForTool) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.createGroup("network");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("temp", "network");
    auto groups = mgr.getGroupsForTool("temp");
    ASSERT_EQ((int)groups.size(), 2);
    ASSERT_TRUE(groups.count("sensors") > 0);
    ASSERT_TRUE(groups.count("network") > 0);
}

TEST(ToolGroup_GetGroupsForUnknownTool) {
    mcpd::ToolGroupManager mgr;
    auto groups = mgr.getGroupsForTool("unknown");
    ASSERT_EQ((int)groups.size(), 0);
}

// --- Listing ---

TEST(ToolGroup_GetGroupNames) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("alpha");
    mgr.createGroup("beta");
    mgr.createGroup("gamma");
    auto names = mgr.getGroupNames();
    ASSERT_EQ((int)names.size(), 3);
}

TEST(ToolGroup_GetToolsInNonexistentGroup) {
    mcpd::ToolGroupManager mgr;
    auto tools = mgr.getToolsInGroup("nonexistent");
    ASSERT_EQ((int)tools.size(), 0);
}

TEST(ToolGroup_GetGroup) {
    mcpd::ToolGroupManager mgr;
    ASSERT_TRUE(mgr.getGroup("nonexistent") == nullptr);
    mgr.createGroup("sensors", "Sensor tools");
    const mcpd::ToolGroup* g = mgr.getGroup("sensors");
    ASSERT_TRUE(g != nullptr);
    ASSERT_STR_CONTAINS(g->name.c_str(), "sensors");
}

// --- Serialization ---

TEST(ToolGroup_ToJson) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors", "Sensor tools");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("humid", "sensors");
    String json = mgr.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "\"name\":\"sensors\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"description\":\"Sensor tools\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"enabled\":true");
    ASSERT_STR_CONTAINS(json.c_str(), "temp");
    ASSERT_STR_CONTAINS(json.c_str(), "humid");
}

TEST(ToolGroup_ToJsonEmpty) {
    mcpd::ToolGroupManager mgr;
    String json = mgr.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "[]");
}

TEST(ToolGroup_ToJsonDisabled) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.disableGroup("sensors");
    String json = mgr.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "\"enabled\":false");
}

// --- Remove group cleans up reverse index ---

TEST(ToolGroup_RemoveGroupCleansReverseIndex) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    auto groups = mgr.getGroupsForTool("temp");
    ASSERT_EQ((int)groups.size(), 1);
    mgr.removeGroup("sensors");
    groups = mgr.getGroupsForTool("temp");
    ASSERT_EQ((int)groups.size(), 0);
}

TEST(ToolGroup_RemoveGroupToolStaysInOtherGroups) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.createGroup("critical");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("temp", "critical");
    mgr.removeGroup("sensors");
    auto groups = mgr.getGroupsForTool("temp");
    ASSERT_EQ((int)groups.size(), 1);
    ASSERT_TRUE(groups.count("critical") > 0);
}

// --- Multiple tools in group ---

TEST(ToolGroup_MultipleToolsInGroup) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("humid", "sensors");
    mgr.addToolToGroup("pressure", "sensors");
    auto tools = mgr.getToolsInGroup("sensors");
    ASSERT_EQ((int)tools.size(), 3);
}

TEST(ToolGroup_DisableGroupAffectsAllTools) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("humid", "sensors");
    mgr.disableGroup("sensors");
    ASSERT_TRUE(mgr.isToolGroupDisabled("temp"));
    ASSERT_TRUE(mgr.isToolGroupDisabled("humid"));
}

// --- Server Integration Tests ---

TEST(ToolGroup_ServerCreateAndDisable) {
    mcpd::Server mcp("test-server");
    mcp.addTool("temp_read", "Read temperature", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"temp\":22.5}");
    });
    mcp.addTool("humid_read", "Read humidity", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"humidity\":65}");
    });
    mcp.addTool("led_set", "Set LED", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"ok\":true}");
    });

    // Create groups
    mcp.createToolGroup("sensors", "Sensor tools");
    mcp.addToolToGroup("temp_read", "sensors");
    mcp.addToolToGroup("humid_read", "sensors");
    mcp.addToolToGroup("led_set", "actuators");

    // All tools should be enabled initially
    ASSERT_TRUE(mcp.isToolEnabled("temp_read"));
    ASSERT_TRUE(mcp.isToolEnabled("humid_read"));
    ASSERT_TRUE(mcp.isToolEnabled("led_set"));

    // Disable sensors group
    mcp.disableToolGroup("sensors");
    ASSERT_FALSE(mcp.isToolEnabled("temp_read"));
    ASSERT_FALSE(mcp.isToolEnabled("humid_read"));
    ASSERT_TRUE(mcp.isToolEnabled("led_set"));  // not in sensors group

    // Re-enable
    mcp.enableToolGroup("sensors");
    ASSERT_TRUE(mcp.isToolEnabled("temp_read"));
    ASSERT_TRUE(mcp.isToolEnabled("humid_read"));
}

TEST(ToolGroup_ServerGroupAndIndividualDisable) {
    mcpd::Server mcp("test-server");
    mcp.addTool("temp_read", "Read temperature", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"temp\":22.5}");
    });

    mcp.createToolGroup("sensors");
    mcp.addToolToGroup("temp_read", "sensors");

    // Individually disable the tool
    mcp.disableTool("temp_read");
    ASSERT_FALSE(mcp.isToolEnabled("temp_read"));

    // Even if group is enabled, individual disable takes precedence
    ASSERT_TRUE(mcp.toolGroups().isGroupEnabled("sensors"));
    ASSERT_FALSE(mcp.isToolEnabled("temp_read"));

    // Re-enable individual
    mcp.enableTool("temp_read");
    ASSERT_TRUE(mcp.isToolEnabled("temp_read"));
}

TEST(ToolGroup_ServerToolsListFiltersGroupDisabled) {
    mcpd::Server mcp("test-server");
    mcp.addTool("temp_read", "Read temperature", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"temp\":22.5}");
    });
    mcp.addTool("led_set", "Set LED", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"ok\":true}");
    });

    mcp.createToolGroup("sensors");
    mcp.addToolToGroup("temp_read", "sensors");
    mcp.disableToolGroup("sensors");

    // Simulate tools/list
    String body = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"id\":1}";
    String resp = mcp._processJsonRpc(body);

    // Should contain led_set but not temp_read
    ASSERT_STR_CONTAINS(resp.c_str(), "led_set");
    ASSERT_STR_NOT_CONTAINS(resp.c_str(), "temp_read");
}

TEST(ToolGroup_ServerToolCallRejectsGroupDisabled) {
    mcpd::Server mcp("test-server");
    mcp.addTool("temp_read", "Read temperature", "{\"type\":\"object\"}", [](const JsonObject&) {
        return String("{\"temp\":22.5}");
    });

    mcp.createToolGroup("sensors");
    mcp.addToolToGroup("temp_read", "sensors");
    mcp.disableToolGroup("sensors");

    // Initialize first
    String init = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-03-26\",\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"},\"capabilities\":{}},\"id\":1}";
    mcp._processJsonRpc(init);

    // Try to call disabled tool
    String call = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"temp_read\",\"arguments\":{}},\"id\":2}";
    String resp = mcp._processJsonRpc(call);
    ASSERT_STR_CONTAINS(resp.c_str(), "Tool not found");
}

TEST(ToolGroup_ServerToolGroupAccessor) {
    mcpd::Server mcp("test-server");
    mcp.createToolGroup("sensors", "All sensors");
    ASSERT_EQ((int)mcp.toolGroups().groupCount(), 1);
    ASSERT_TRUE(mcp.toolGroups().hasGroup("sensors"));
}

TEST(ToolGroup_ServerEnableNonexistentGroup) {
    mcpd::Server mcp("test-server");
    ASSERT_FALSE(mcp.enableToolGroup("nonexistent"));
    ASSERT_FALSE(mcp.disableToolGroup("nonexistent"));
}

// --- Edge Cases ---

TEST(ToolGroup_EmptyGroupName) {
    mcpd::ToolGroupManager mgr;
    ASSERT_TRUE(mgr.createGroup("", "Empty name group"));
    ASSERT_TRUE(mgr.hasGroup(""));
}

TEST(ToolGroup_ManyGroups) {
    mcpd::ToolGroupManager mgr;
    for (int i = 0; i < 20; i++) {
        String name = "group_" + String(i);
        mgr.createGroup(name.c_str());
    }
    ASSERT_EQ((int)mgr.groupCount(), 20);
}

TEST(ToolGroup_ToolInManyGroups) {
    mcpd::ToolGroupManager mgr;
    for (int i = 0; i < 10; i++) {
        String name = "group_" + String(i);
        mgr.addToolToGroup("shared_tool", name.c_str());
    }
    auto groups = mgr.getGroupsForTool("shared_tool");
    ASSERT_EQ((int)groups.size(), 10);
}

TEST(ToolGroup_DisableAllGroupsThenReenableOne) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("a");
    mgr.createGroup("b");
    mgr.addToolToGroup("tool1", "a");
    mgr.addToolToGroup("tool1", "b");
    mgr.disableGroup("a");
    mgr.disableGroup("b");
    ASSERT_TRUE(mgr.isToolGroupDisabled("tool1"));
    mgr.enableGroup("a");
    ASSERT_FALSE(mgr.isToolGroupDisabled("tool1"));
}

TEST(ToolGroup_RemoveToolCleansReverse) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors");
    mgr.addToolToGroup("temp", "sensors");
    mgr.removeToolFromGroup("temp", "sensors");
    // After removing tool from its only group, reverse index should be empty
    auto groups = mgr.getGroupsForTool("temp");
    ASSERT_EQ((int)groups.size(), 0);
    // Tool should no longer be group-disabled even if group is disabled
    mgr.disableGroup("sensors");
    ASSERT_FALSE(mgr.isToolGroupDisabled("temp"));
}

TEST(ToolGroup_MultipleGroupsSerialization) {
    mcpd::ToolGroupManager mgr;
    mgr.createGroup("sensors", "Sensors");
    mgr.createGroup("actuators", "Actuators");
    mgr.addToolToGroup("temp", "sensors");
    mgr.addToolToGroup("motor", "actuators");
    mgr.disableGroup("actuators");
    String json = mgr.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "sensors");
    ASSERT_STR_CONTAINS(json.c_str(), "actuators");
    ASSERT_STR_CONTAINS(json.c_str(), "\"enabled\":false");
}

// ============================================================================
// Test runner
// ============================================================================

int main() {
    printf("\n  === MCPToolGroup Tests ===\n\n");
    TEST_SUMMARY();
    return _tests_failed;
}
