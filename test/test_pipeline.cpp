#include "test_framework.h"
#include "MCPPipeline.h"

// ============================================================================
// Pipeline Tests
// ============================================================================

// --- Basic Pipeline Tests ---

TEST(Pipeline_DefineAndCommit) {
    mcpd::Pipeline pipeline;
    pipeline.define("test_pipe")
        .step("tool_a", {{"key", "val"}})
        .step("tool_b");
    ASSERT_EQ((int)pipeline.count(), 0);  // Not committed yet
    int committed = pipeline.commit();
    ASSERT_EQ(committed, 1);
    ASSERT_EQ((int)pipeline.count(), 1);
    ASSERT_TRUE(pipeline.exists("test_pipe"));
}

TEST(Pipeline_EmptyPipelineNotCommitted) {
    mcpd::Pipeline pipeline;
    pipeline.define("empty");
    int committed = pipeline.commit();
    ASSERT_EQ(committed, 0);  // No steps = not committed
    ASSERT_EQ((int)pipeline.count(), 0);
}

TEST(Pipeline_Remove) {
    mcpd::Pipeline pipeline;
    pipeline.define("removable").step("tool_a");
    pipeline.commit();
    ASSERT_TRUE(pipeline.exists("removable"));
    ASSERT_TRUE(pipeline.remove("removable"));
    ASSERT_FALSE(pipeline.exists("removable"));
    ASSERT_FALSE(pipeline.remove("nonexistent"));
}

TEST(Pipeline_List) {
    mcpd::Pipeline pipeline;
    pipeline.define("alpha").step("t1");
    pipeline.define("beta").step("t2");
    pipeline.commit();
    auto names = pipeline.list();
    ASSERT_EQ((int)names.size(), 2);
}

TEST(Pipeline_ReplaceExisting) {
    mcpd::Pipeline pipeline;
    pipeline.define("dup").step("old_tool");
    pipeline.commit();

    pipeline.define("dup").step("new_tool_a").step("new_tool_b");
    pipeline.commit();

    ASSERT_EQ((int)pipeline.count(), 1);
    // Info should show 2 steps now
    String info = pipeline.info("dup");
    ASSERT_STR_CONTAINS(info.c_str(), "\"steps\":2");
}

TEST(Pipeline_MaxStepsEnforced) {
    mcpd::Pipeline pipeline;
    auto& builder = pipeline.define("big");
    for (int i = 0; i < 25; i++) {  // > MAX_STEPS_PER_PIPELINE (20)
        builder.step("tool_" + String(i));
    }
    int committed = pipeline.commit();
    ASSERT_EQ(committed, 0);  // Too many steps
}

// --- Execution Tests ---

TEST(Pipeline_BasicExecution) {
    mcpd::Pipeline pipeline;
    pipeline.define("simple")
        .step("sensor_read", {{"pin", "4"}})
        .step("display_write", {{"text", "hello"}});
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        if (tool == "sensor_read") return "{\"temperature\":22.5}";
        if (tool == "display_write") return "{\"ok\":true}";
        return "{\"error\":\"unknown tool\"}";
    });

    auto result = pipeline.execute("simple");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stepsExecuted, 2);
    ASSERT_EQ(result.stepsFailed, 0);
    ASSERT_EQ(callCount, 2);
}

TEST(Pipeline_PrevSubstitution) {
    mcpd::Pipeline pipeline;
    pipeline.define("chain")
        .step("read_temp")
        .step("display", {{"text", "Temp: $prev.temperature"}});
    pipeline.commit();

    String displayParams;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        if (tool == "read_temp") return "{\"temperature\":\"23.1\"}";
        if (tool == "display") {
            displayParams = params;
            return "{\"ok\":true}";
        }
        return "{}";
    });

    auto result = pipeline.execute("chain");
    ASSERT_TRUE(result.success);
    ASSERT_STR_CONTAINS(displayParams.c_str(), "23.1");
}

TEST(Pipeline_OverrideSubstitution) {
    mcpd::Pipeline pipeline;
    pipeline.define("configurable")
        .step("sensor", {{"pin", "$override.sensorPin"}});
    pipeline.commit();

    String capturedParams;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        capturedParams = params;
        return "{\"value\":42}";
    });

    auto result = pipeline.execute("configurable", "{\"sensorPin\":\"7\"}");
    ASSERT_TRUE(result.success);
    ASSERT_STR_CONTAINS(capturedParams.c_str(), "7");
}

TEST(Pipeline_ToolOverrideParams) {
    mcpd::Pipeline pipeline;
    pipeline.define("overridable")
        .step("sensor", {{"pin", "4"}});
    pipeline.commit();

    String capturedParams;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        capturedParams = params;
        return "{}";
    });

    // Override sensor's pin parameter
    auto result = pipeline.execute("overridable", "{\"sensor\":{\"pin\":\"8\"}}");
    ASSERT_TRUE(result.success);
    ASSERT_STR_CONTAINS(capturedParams.c_str(), "8");
}

TEST(Pipeline_NotFoundError) {
    mcpd::Pipeline pipeline;
    auto result = pipeline.execute("nonexistent");
    ASSERT_FALSE(result.success);
    ASSERT_STR_CONTAINS(result.error.c_str(), "not found");
}

TEST(Pipeline_NoDispatcherError) {
    mcpd::Pipeline pipeline;
    pipeline.define("no_dispatch").step("tool");
    pipeline.commit();
    auto result = pipeline.execute("no_dispatch");
    ASSERT_FALSE(result.success);
    ASSERT_STR_CONTAINS(result.error.c_str(), "dispatcher");
}

// --- Error Policy Tests ---

TEST(Pipeline_ErrorPolicyStop) {
    mcpd::Pipeline pipeline;
    pipeline.define("stop_on_error")
        .step("fail_tool")
        .step("never_reached")
        .onError(mcpd::ErrorPolicy::Stop);
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        if (tool == "fail_tool") return "{\"error\":\"something broke\"}";
        return "{}";
    });

    auto result = pipeline.execute("stop_on_error");
    ASSERT_FALSE(result.success);
    ASSERT_EQ(callCount, 1);  // Second step never called
    ASSERT_EQ(result.stepsFailed, 1);
}

TEST(Pipeline_ErrorPolicyContinue) {
    mcpd::Pipeline pipeline;
    pipeline.define("continue_on_error")
        .step("fail_tool")
        .step("still_runs")
        .onError(mcpd::ErrorPolicy::Continue);
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        if (tool == "fail_tool") return "{\"error\":\"oops\"}";
        return "{\"ok\":true}";
    });

    auto result = pipeline.execute("continue_on_error");
    ASSERT_FALSE(result.success);  // Overall still fails
    ASSERT_EQ(callCount, 2);      // But both ran
    ASSERT_EQ(result.stepsFailed, 1);
    ASSERT_EQ(result.stepsExecuted, 2);
}

TEST(Pipeline_ErrorPolicyRollback) {
    mcpd::Pipeline pipeline;
    bool undone = false;

    auto& builder = pipeline.define("rollback_test");
    builder.step("setup_gpio")
        .undo([&]() { undone = true; });
    builder.step("fail_step");
    builder.onError(mcpd::ErrorPolicy::Rollback);
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        if (tool == "setup_gpio") return "{\"ok\":true}";
        return "{\"error\":\"failed\"}";
    });

    auto result = pipeline.execute("rollback_test");
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(undone);  // Rollback executed
    ASSERT_STR_CONTAINS(result.error.c_str(), "rolling back");
}

// --- Conditional Steps ---

TEST(Pipeline_ConditionalStep_Executed) {
    mcpd::Pipeline pipeline;
    auto& builder = pipeline.define("conditional");
    builder.step("sensor");
    builder.step("alert")
        .when([](const mcpd::PipelineResult& r) {
            // Always true for this test
            return true;
        });
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        return "{\"ok\":true}";
    });

    auto result = pipeline.execute("conditional");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(callCount, 2);
}

TEST(Pipeline_ConditionalStep_Skipped) {
    mcpd::Pipeline pipeline;
    auto& builder = pipeline.define("skip_step");
    builder.step("sensor");
    builder.step("alert")
        .when([](const mcpd::PipelineResult& r) {
            return false;  // Always skip
        });
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        return "{\"ok\":true}";
    });

    auto result = pipeline.execute("skip_step");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(callCount, 1);  // alert was skipped
    ASSERT_EQ(result.stepsExecuted, 2);  // But counts as executed (skipped)
}

// --- Builder Features ---

TEST(Pipeline_StepDescription) {
    mcpd::Pipeline pipeline;
    pipeline.define("described")
        .step("tool_a")
        .describe("Read the temperature sensor");
    pipeline.commit();

    String info = pipeline.info("described");
    ASSERT_STR_CONTAINS(info.c_str(), "Read the temperature sensor");
}

TEST(Pipeline_StepTimeout) {
    mcpd::Pipeline pipeline;
    pipeline.define("timed")
        .step("slow_tool")
        .timeout(10000);
    pipeline.commit();

    String info = pipeline.info("timed");
    ASSERT_STR_CONTAINS(info.c_str(), "10000");
}

// --- Info and JSON ---

TEST(Pipeline_InfoNotFound) {
    mcpd::Pipeline pipeline;
    String info = pipeline.info("ghost");
    ASSERT_STR_CONTAINS(info.c_str(), "not found");
}

TEST(Pipeline_InfoDetails) {
    mcpd::Pipeline pipeline;
    pipeline.define("detailed")
        .step("tool_a")
        .step("tool_b")
        .onError(mcpd::ErrorPolicy::Continue);
    pipeline.commit();

    String info = pipeline.info("detailed");
    ASSERT_STR_CONTAINS(info.c_str(), "\"name\":\"detailed\"");
    ASSERT_STR_CONTAINS(info.c_str(), "\"steps\":2");
    ASSERT_STR_CONTAINS(info.c_str(), "\"errorPolicy\":\"continue\"");
}

TEST(Pipeline_ToJson) {
    mcpd::Pipeline pipeline;
    pipeline.define("a").step("t1");
    pipeline.define("b").step("t2").step("t3");
    pipeline.commit();

    String json = pipeline.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "\"count\":2");
    ASSERT_STR_CONTAINS(json.c_str(), "\"maxPipelines\":");
}

TEST(Pipeline_ResultToJson) {
    mcpd::Pipeline pipeline;
    pipeline.define("json_result")
        .step("tool_a");
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{\"value\":42}";
    });

    auto result = pipeline.execute("json_result");
    String json = result.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "\"pipeline\":\"json_result\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"success\":true");
    ASSERT_STR_CONTAINS(json.c_str(), "\"stepsExecuted\":1");
}

// --- Multi-step Data Flow ---

TEST(Pipeline_ThreeStepChain) {
    mcpd::Pipeline pipeline;
    pipeline.define("three_step")
        .step("read")
        .step("transform", {{"input", "$prev.raw"}})
        .step("write", {{"data", "$prev.processed"}});
    pipeline.commit();

    std::vector<String> calls;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        calls.push_back(tool);
        if (tool == "read") return "{\"raw\":\"100\"}";
        if (tool == "transform") return "{\"processed\":\"200\"}";
        if (tool == "write") return "{\"written\":true}";
        return "{}";
    });

    auto result = pipeline.execute("three_step");
    ASSERT_TRUE(result.success);
    ASSERT_EQ((int)calls.size(), 3);
    ASSERT_STR_CONTAINS(calls[0].c_str(), "read");
    ASSERT_STR_CONTAINS(calls[1].c_str(), "transform");
    ASSERT_STR_CONTAINS(calls[2].c_str(), "write");
}

TEST(Pipeline_PrevFieldNotFound) {
    mcpd::Pipeline pipeline;
    pipeline.define("missing_ref")
        .step("read")
        .step("use", {{"val", "$prev.nonexistent"}});
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{\"other\":\"data\"}";
    });

    auto result = pipeline.execute("missing_ref");
    ASSERT_TRUE(result.success);  // Substitutes as empty string, not an error
}

TEST(Pipeline_MultiplePrevRefs) {
    mcpd::Pipeline pipeline;
    pipeline.define("multi_ref")
        .step("read")
        .step("display", {{"line1", "T:$prev.temp"}, {"line2", "H:$prev.humidity"}});
    pipeline.commit();

    String capturedParams;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        if (tool == "read") return "{\"temp\":\"22\",\"humidity\":\"65\"}";
        capturedParams = params;
        return "{}";
    });

    auto result = pipeline.execute("multi_ref");
    ASSERT_TRUE(result.success);
    ASSERT_STR_CONTAINS(capturedParams.c_str(), "22");
    ASSERT_STR_CONTAINS(capturedParams.c_str(), "65");
}

// --- Edge Cases ---

TEST(Pipeline_SingleStep) {
    mcpd::Pipeline pipeline;
    pipeline.define("single").step("only_tool");
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{\"result\":\"ok\"}";
    });

    auto result = pipeline.execute("single");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stepsExecuted, 1);
}

TEST(Pipeline_EmptyToolOutput) {
    mcpd::Pipeline pipeline;
    pipeline.define("empty_output")
        .step("silent_tool")
        .step("next_tool");
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{}";
    });

    auto result = pipeline.execute("empty_output");
    ASSERT_TRUE(result.success);
}

TEST(Pipeline_NonJsonOutput) {
    mcpd::Pipeline pipeline;
    pipeline.define("text_output")
        .step("text_tool");
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "plain text result";
    });

    auto result = pipeline.execute("text_output");
    ASSERT_TRUE(result.success);
    // Result JSON should handle non-JSON output gracefully
    String json = result.toJson();
    ASSERT_STR_CONTAINS(json.c_str(), "plain text result");
}

TEST(Pipeline_MultipleRollbacks) {
    mcpd::Pipeline pipeline;
    int undoCount = 0;

    auto& builder = pipeline.define("multi_rollback");
    builder.step("step1").undo([&]() { undoCount++; });
    builder.step("step2").undo([&]() { undoCount++; });
    builder.step("step3");  // This one fails
    builder.onError(mcpd::ErrorPolicy::Rollback);
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        if (tool == "step3") return "{\"error\":\"fail\"}";
        return "{\"ok\":true}";
    });

    auto result = pipeline.execute("multi_rollback");
    ASSERT_FALSE(result.success);
    ASSERT_EQ(undoCount, 2);  // Both step1 and step2 undo'd
}

TEST(Pipeline_MaxPipelinesEnforced) {
    mcpd::Pipeline pipeline;
    for (size_t i = 0; i < mcpd::Pipeline::MAX_PIPELINES + 5; i++) {
        pipeline.define("pipe_" + String((int)i)).step("tool");
    }
    pipeline.commit();
    ASSERT_EQ((int)pipeline.count(), (int)mcpd::Pipeline::MAX_PIPELINES);
}

TEST(Pipeline_TimingTracked) {
    mcpd::Pipeline pipeline;
    pipeline.define("timed_exec")
        .step("tool_a")
        .step("tool_b");
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{}";
    });

    auto result = pipeline.execute("timed_exec");
    ASSERT_TRUE(result.success);
    // Duration should be tracked (>= 0)
    ASSERT_TRUE(result.totalDurationMs >= 0);
    ASSERT_EQ((int)result.steps.size(), 2);
}

TEST(Pipeline_ErrorPolicyStopDefault) {
    // Default error policy should be Stop
    mcpd::Pipeline pipeline;
    pipeline.define("default_policy")
        .step("fail")
        .step("never");
    pipeline.commit();

    int callCount = 0;
    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        callCount++;
        return "{\"error\":\"fail\"}";
    });

    auto result = pipeline.execute("default_policy");
    ASSERT_FALSE(result.success);
    ASSERT_EQ(callCount, 1);  // Default Stop policy
}

TEST(Pipeline_ConditionalBasedOnPrevResult) {
    mcpd::Pipeline pipeline;
    auto& builder = pipeline.define("smart_conditional");
    builder.step("check");
    builder.step("action")
        .when([](const mcpd::PipelineResult& r) {
            // Only run if first step succeeded
            return !r.steps.empty() && r.steps.back().success;
        });
    pipeline.commit();

    pipeline.setDispatcher([&](const String& tool, const String& params) -> String {
        return "{\"ok\":true}";
    });

    auto result = pipeline.execute("smart_conditional");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stepsExecuted, 2);
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    printf("\n  === MCPPipeline Tests ===\n\n");
    TEST_SUMMARY();
    return _tests_failed;
}
