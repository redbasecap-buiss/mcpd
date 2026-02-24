/**
 * mcpd — Tests for MCPAlerts (Threshold-Based Alerting System)
 */

#include "test_framework.h"
#include "../src/MCPAlerts.h"

// ═══════════════════════════════════════════════════════════════════════
//  Basic Alert Creation
// ═══════════════════════════════════════════════════════════════════════

TEST(AlertCreation) {
    mcpd::AlertEngine engine(8);
    ASSERT_EQ((int)engine.count(), 0);
    ASSERT_EQ((int)engine.capacity(), 8);
    ASSERT_TRUE(engine.addAlert("temp_high", mcpd::AlertOp::GreaterThan, 40.0));
    ASSERT_EQ((int)engine.count(), 1);
    ASSERT_TRUE(engine.hasAlert("temp_high"));
    ASSERT_FALSE(engine.hasAlert("nonexistent"));
}

TEST(AlertCreation_DuplicateName) {
    mcpd::AlertEngine engine;
    ASSERT_TRUE(engine.addAlert("test", mcpd::AlertOp::GreaterThan, 10.0));
    ASSERT_FALSE(engine.addAlert("test", mcpd::AlertOp::LessThan, 5.0));
    ASSERT_EQ((int)engine.count(), 1);
}

TEST(AlertCreation_NullName) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.addAlert(nullptr, mcpd::AlertOp::GreaterThan, 10.0));
    ASSERT_FALSE(engine.addAlert("", mcpd::AlertOp::GreaterThan, 10.0));
}

TEST(AlertCreation_CapacityLimit) {
    mcpd::AlertEngine engine(2);
    ASSERT_TRUE(engine.addAlert("a1", mcpd::AlertOp::GreaterThan, 10.0));
    ASSERT_TRUE(engine.addAlert("a2", mcpd::AlertOp::LessThan, 5.0));
    ASSERT_FALSE(engine.addAlert("a3", mcpd::AlertOp::Equal, 0.0));
}

TEST(AlertCreation_RangeAlert) {
    mcpd::AlertEngine engine;
    ASSERT_TRUE(engine.addAlert("pressure", mcpd::AlertOp::OutsideRange, 950.0, 1050.0));
    ASSERT_TRUE(engine.addAlert("temp", mcpd::AlertOp::InsideRange, 20.0, 30.0));
    ASSERT_EQ((int)engine.count(), 2);
}

TEST(AlertCreation_RangeRequiresRangeOp) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.addAlert("bad", mcpd::AlertOp::GreaterThan, 10.0, 20.0));
    ASSERT_EQ((int)engine.count(), 0);
}

TEST(AlertRemoval) {
    mcpd::AlertEngine engine;
    engine.addAlert("a1", mcpd::AlertOp::GreaterThan, 10.0);
    engine.addAlert("a2", mcpd::AlertOp::LessThan, 5.0);
    ASSERT_TRUE(engine.removeAlert("a1"));
    ASSERT_EQ((int)engine.count(), 1);
    ASSERT_FALSE(engine.hasAlert("a1"));
    ASSERT_TRUE(engine.hasAlert("a2"));
    ASSERT_FALSE(engine.removeAlert("nonexistent"));
}

TEST(AlertClearAll) {
    mcpd::AlertEngine engine;
    engine.addAlert("a1", mcpd::AlertOp::GreaterThan, 10.0);
    engine.addAlert("a2", mcpd::AlertOp::LessThan, 5.0);
    engine.clearAlerts();
    ASSERT_EQ((int)engine.count(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  Basic Threshold Checking
// ═══════════════════════════════════════════════════════════════════════

TEST(CheckGreaterThan) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_FALSE(engine.check("temp", 39.0));
    ASSERT_FALSE(engine.check("temp", 40.0));
    ASSERT_TRUE(engine.check("temp", 41.0));
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Active);
}

TEST(CheckGreaterEqual) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterEqual, 40.0);
    ASSERT_FALSE(engine.check("temp", 39.9));
    ASSERT_TRUE(engine.check("temp", 40.0));
}

TEST(CheckLessThan) {
    mcpd::AlertEngine engine;
    engine.addAlert("batt", mcpd::AlertOp::LessThan, 20.0);
    ASSERT_FALSE(engine.check("batt", 21.0));
    ASSERT_FALSE(engine.check("batt", 20.0));
    ASSERT_TRUE(engine.check("batt", 19.0));
}

TEST(CheckLessEqual) {
    mcpd::AlertEngine engine;
    engine.addAlert("batt", mcpd::AlertOp::LessEqual, 20.0);
    ASSERT_FALSE(engine.check("batt", 20.1));
    ASSERT_TRUE(engine.check("batt", 20.0));
}

TEST(CheckEqual) {
    mcpd::AlertEngine engine;
    engine.addAlert("mode", mcpd::AlertOp::Equal, 1.0);
    ASSERT_FALSE(engine.check("mode", 0.0));
    ASSERT_TRUE(engine.check("mode", 1.0));
}

TEST(CheckNotEqual) {
    mcpd::AlertEngine engine;
    engine.addAlert("mode", mcpd::AlertOp::NotEqual, 0.0);
    ASSERT_FALSE(engine.check("mode", 0.0));
    ASSERT_TRUE(engine.check("mode", 1.0));
}

TEST(CheckOutsideRange) {
    mcpd::AlertEngine engine;
    engine.addAlert("pressure", mcpd::AlertOp::OutsideRange, 950.0, 1050.0);
    ASSERT_FALSE(engine.check("pressure", 1000.0));
    ASSERT_FALSE(engine.check("pressure", 950.0));
    ASSERT_FALSE(engine.check("pressure", 1050.0));
    ASSERT_TRUE(engine.check("pressure", 949.0));
    engine.reset("pressure");
    ASSERT_TRUE(engine.check("pressure", 1051.0));
}

TEST(CheckInsideRange) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::InsideRange, 20.0, 30.0);
    ASSERT_FALSE(engine.check("temp", 19.0));
    ASSERT_TRUE(engine.check("temp", 25.0));
    engine.reset("temp");
    ASSERT_TRUE(engine.check("temp", 20.0));
    engine.reset("temp");
    ASSERT_TRUE(engine.check("temp", 30.0));
}

TEST(CheckNonexistentAlert) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.check("nope", 42.0));
}

TEST(CheckDisabledAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.setEnabled("temp", false);
    ASSERT_FALSE(engine.check("temp", 50.0));
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

// ═══════════════════════════════════════════════════════════════════════
//  Alert Clearing
// ═══════════════════════════════════════════════════════════════════════

TEST(AlertClears) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 50.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Active);
    engine.check("temp", 35.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

TEST(AlertClearsFromAcknowledged) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 50.0);
    engine.acknowledge("temp");
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Acknowledged);
    engine.check("temp", 35.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

// ═══════════════════════════════════════════════════════════════════════
//  Hysteresis
// ═══════════════════════════════════════════════════════════════════════

TEST(HysteresisPreventsFlapping) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.setHysteresis("temp", 2.0);

    engine.check("temp", 41.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Active);

    engine.check("temp", 39.5);  // below threshold but within hysteresis
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Active);

    engine.check("temp", 37.5);  // below threshold - hysteresis (40 - 2 = 38)
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

TEST(HysteresisLessThan) {
    mcpd::AlertEngine engine;
    engine.addAlert("batt", mcpd::AlertOp::LessThan, 20.0);
    engine.setHysteresis("batt", 3.0);

    engine.check("batt", 15.0);
    ASSERT_EQ(engine.getState("batt"), mcpd::AlertState::Active);

    engine.check("batt", 21.0);  // above threshold but within hysteresis
    ASSERT_EQ(engine.getState("batt"), mcpd::AlertState::Active);

    engine.check("batt", 24.0);  // above threshold + hysteresis
    ASSERT_EQ(engine.getState("batt"), mcpd::AlertState::Clear);
}

TEST(HysteresisOnNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.setHysteresis("nope", 1.0));
}

// ═══════════════════════════════════════════════════════════════════════
//  Cooldown
// ═══════════════════════════════════════════════════════════════════════

TEST(CooldownPreventsRapidRefiring) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.setCooldown("temp", 10000);

    engine.check("temp", 50.0);
    ASSERT_EQ((int)engine.getFireCount("temp"), 1);

    engine.check("temp", 30.0);  // clear
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);

    // Re-fire blocked by cooldown
    ASSERT_FALSE(engine.check("temp", 50.0));
    ASSERT_EQ((int)engine.getFireCount("temp"), 1);
}

TEST(CooldownOnNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.setCooldown("nope", 1000));
}

// ═══════════════════════════════════════════════════════════════════════
//  Listeners & Callbacks
// ═══════════════════════════════════════════════════════════════════════

TEST(GlobalListener) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);

    int fireCount = 0, clearCount = 0;
    engine.onAlert([&](const mcpd::AlertEvent& e) {
        if (e.fired) fireCount++;
        else clearCount++;
    });

    engine.check("temp", 50.0);
    ASSERT_EQ(fireCount, 1);
    ASSERT_EQ(clearCount, 0);

    engine.check("temp", 30.0);
    ASSERT_EQ(fireCount, 1);
    ASSERT_EQ(clearCount, 1);
}

TEST(PerAlertCallback) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);

    double capturedValue = 0;
    engine.setCallback("temp", [&](const mcpd::AlertEvent& e) {
        capturedValue = e.value;
    });

    engine.check("temp", 45.0);
    ASSERT_TRUE(std::abs(capturedValue - 45.0) < 0.01);
}

TEST(MultipleListeners) {
    mcpd::AlertEngine engine;
    engine.addAlert("x", mcpd::AlertOp::GreaterThan, 0.0);

    int c1 = 0, c2 = 0;
    engine.onAlert([&](const mcpd::AlertEvent&) { c1++; });
    engine.onAlert([&](const mcpd::AlertEvent&) { c2++; });

    engine.check("x", 1.0);
    ASSERT_EQ(c1, 1);
    ASSERT_EQ(c2, 1);
}

TEST(ClearListeners) {
    mcpd::AlertEngine engine;
    engine.addAlert("x", mcpd::AlertOp::GreaterThan, 0.0);

    int count = 0;
    engine.onAlert([&](const mcpd::AlertEvent&) { count++; });
    engine.clearListeners();

    engine.check("x", 1.0);
    ASSERT_EQ(count, 0);
}

TEST(ListenerEventFields) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0, mcpd::AlertSeverity::Critical);

    mcpd::AlertEvent captured = {};
    engine.onAlert([&](const mcpd::AlertEvent& e) { captured = e; });

    engine.check("temp", 42.5);
    ASSERT_STR_CONTAINS(captured.name, "temp");
    ASSERT_TRUE(std::abs(captured.value - 42.5) < 0.01);
    ASSERT_TRUE(std::abs(captured.threshold - 40.0) < 0.01);
    ASSERT_TRUE(captured.fired);
    ASSERT_EQ((int)captured.severity, (int)mcpd::AlertSeverity::Critical);
    ASSERT_EQ((int)captured.op, (int)mcpd::AlertOp::GreaterThan);
}

TEST(CallbackOnNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.setCallback("nope", [](const mcpd::AlertEvent&) {}));
}

// ═══════════════════════════════════════════════════════════════════════
//  Acknowledge & Reset
// ═══════════════════════════════════════════════════════════════════════

TEST(Acknowledge) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 50.0);
    ASSERT_TRUE(engine.acknowledge("temp"));
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Acknowledged);
}

TEST(AcknowledgeClearAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_FALSE(engine.acknowledge("temp"));
}

TEST(AcknowledgeNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.acknowledge("nope"));
}

TEST(ResetAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 50.0);
    ASSERT_TRUE(engine.reset("temp"));
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

TEST(ResetNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.reset("nope"));
}

TEST(ResetAll) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 0.0);
    engine.check("a", 1.0);
    engine.check("b", 1.0);
    ASSERT_EQ((int)engine.activeCount(), 2);
    engine.resetAll();
    ASSERT_EQ((int)engine.activeCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  Configuration
// ═══════════════════════════════════════════════════════════════════════

TEST(SetSeverity) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_TRUE(engine.setSeverity("temp", mcpd::AlertSeverity::Critical));
    ASSERT_FALSE(engine.setSeverity("nope", mcpd::AlertSeverity::Critical));
}

TEST(SetEnabled) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_TRUE(engine.isEnabled("temp"));
    ASSERT_TRUE(engine.setEnabled("temp", false));
    ASSERT_FALSE(engine.isEnabled("temp"));
    ASSERT_FALSE(engine.setEnabled("nope", true));
}

TEST(IsEnabledNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_FALSE(engine.isEnabled("nope"));
}

// ═══════════════════════════════════════════════════════════════════════
//  Query Functions
// ═══════════════════════════════════════════════════════════════════════

TEST(ActiveCount) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 10.0);
    engine.check("a", 1.0);
    ASSERT_EQ((int)engine.activeCount(), 1);
    engine.check("b", 11.0);
    ASSERT_EQ((int)engine.activeCount(), 2);
}

TEST(ActiveAlerts) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 10.0);
    engine.addAlert("c", mcpd::AlertOp::LessThan, 5.0);
    engine.check("a", 1.0);
    engine.check("b", 5.0);
    engine.check("c", 3.0);
    auto active = engine.activeAlerts();
    ASSERT_EQ((int)active.size(), 2);
}

TEST(FireCount) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_EQ((int)engine.getFireCount("temp"), 0);
    engine.check("temp", 50.0);
    ASSERT_EQ((int)engine.getFireCount("temp"), 1);
    engine.check("temp", 51.0);  // stays active
    ASSERT_EQ((int)engine.getFireCount("temp"), 1);
    engine.check("temp", 30.0);
    engine.check("temp", 50.0);
    ASSERT_EQ((int)engine.getFireCount("temp"), 2);
}

TEST(CheckCount) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 30.0);
    engine.check("temp", 50.0);
    engine.check("temp", 35.0);
    ASSERT_EQ((int)engine.getCheckCount("temp"), 3);
    ASSERT_EQ((int)engine.getCheckCount("nope"), 0);
}

TEST(LastValue) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 42.5);
    ASSERT_TRUE(std::abs(engine.getLastValue("temp") - 42.5) < 0.01);
    ASSERT_TRUE(std::abs(engine.getLastValue("nope")) < 0.01);
}

TEST(LastFiredMs) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_EQ(engine.getLastFiredMs("temp"), (unsigned long)0);
    engine.check("temp", 50.0);
    // millis() is 0 in test mock, so just check it doesn't crash
    ASSERT_EQ(engine.getLastFiredMs("nope"), (unsigned long)0);
}

TEST(GetStateNonexistent) {
    mcpd::AlertEngine engine;
    ASSERT_EQ(engine.getState("nope"), mcpd::AlertState::Clear);
}

// ═══════════════════════════════════════════════════════════════════════
//  CheckAll
// ═══════════════════════════════════════════════════════════════════════

TEST(CheckAllWithPrefix) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp_high", mcpd::AlertOp::GreaterThan, 40.0);
    engine.addAlert("temp_low", mcpd::AlertOp::LessThan, 10.0);
    engine.addAlert("pressure_high", mcpd::AlertOp::GreaterThan, 1100.0);
    int active = engine.checkAll("temp_", 50.0);
    ASSERT_EQ(active, 1);  // only temp_high
}

TEST(CheckAllEmptyPrefix) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 0.0);
    ASSERT_EQ(engine.checkAll("", 1.0), 2);
}

TEST(CheckAllNullPrefix) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    ASSERT_EQ(engine.checkAll(nullptr, 1.0), 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  Enum Conversions
// ═══════════════════════════════════════════════════════════════════════

TEST(AlertOpToFromString) {
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::GreaterThan), ">");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::GreaterEqual), ">=");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::LessThan), "<");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::LessEqual), "<=");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::Equal), "==");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::NotEqual), "!=");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::OutsideRange), "outside_range");
    ASSERT_STR_EQ(mcpd::alertOpToString(mcpd::AlertOp::InsideRange), "inside_range");

    ASSERT_EQ((int)mcpd::alertOpFromString(">"), (int)mcpd::AlertOp::GreaterThan);
    ASSERT_EQ((int)mcpd::alertOpFromString(">="), (int)mcpd::AlertOp::GreaterEqual);
    ASSERT_EQ((int)mcpd::alertOpFromString("<"), (int)mcpd::AlertOp::LessThan);
    ASSERT_EQ((int)mcpd::alertOpFromString("<="), (int)mcpd::AlertOp::LessEqual);
    ASSERT_EQ((int)mcpd::alertOpFromString("=="), (int)mcpd::AlertOp::Equal);
    ASSERT_EQ((int)mcpd::alertOpFromString("!="), (int)mcpd::AlertOp::NotEqual);
    ASSERT_EQ((int)mcpd::alertOpFromString("outside_range"), (int)mcpd::AlertOp::OutsideRange);
    ASSERT_EQ((int)mcpd::alertOpFromString("inside_range"), (int)mcpd::AlertOp::InsideRange);
    ASSERT_EQ((int)mcpd::alertOpFromString(nullptr), (int)mcpd::AlertOp::GreaterThan);
    ASSERT_EQ((int)mcpd::alertOpFromString("bogus"), (int)mcpd::AlertOp::GreaterThan);
}

TEST(AlertSeverityToFromString) {
    ASSERT_STR_EQ(mcpd::alertSeverityToString(mcpd::AlertSeverity::Debug), "debug");
    ASSERT_STR_EQ(mcpd::alertSeverityToString(mcpd::AlertSeverity::Info), "info");
    ASSERT_STR_EQ(mcpd::alertSeverityToString(mcpd::AlertSeverity::Warning), "warning");
    ASSERT_STR_EQ(mcpd::alertSeverityToString(mcpd::AlertSeverity::Error), "error");
    ASSERT_STR_EQ(mcpd::alertSeverityToString(mcpd::AlertSeverity::Critical), "critical");

    ASSERT_EQ((int)mcpd::alertSeverityFromString("debug"), (int)mcpd::AlertSeverity::Debug);
    ASSERT_EQ((int)mcpd::alertSeverityFromString("critical"), (int)mcpd::AlertSeverity::Critical);
    ASSERT_EQ((int)mcpd::alertSeverityFromString(nullptr), (int)mcpd::AlertSeverity::Info);
    ASSERT_EQ((int)mcpd::alertSeverityFromString("bogus"), (int)mcpd::AlertSeverity::Info);
}

TEST(AlertStateToString) {
    ASSERT_STR_EQ(mcpd::alertStateToString(mcpd::AlertState::Clear), "clear");
    ASSERT_STR_EQ(mcpd::alertStateToString(mcpd::AlertState::Active), "active");
    ASSERT_STR_EQ(mcpd::alertStateToString(mcpd::AlertState::Acknowledged), "acknowledged");
}

// ═══════════════════════════════════════════════════════════════════════
//  JSON Serialization
// ═══════════════════════════════════════════════════════════════════════

TEST(ToJSON_Empty) {
    mcpd::AlertEngine engine;
    ASSERT_STR_EQ(engine.toJSON().c_str(), "[]");
}

TEST(ToJSON_SingleAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0, mcpd::AlertSeverity::Warning);
    String json = engine.toJSON();
    ASSERT_STR_CONTAINS(json.c_str(), "\"name\":\"temp\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"op\":\">\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"severity\":\"warning\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"state\":\"clear\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"enabled\":true");
}

TEST(ToJSON_RangeAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("p", mcpd::AlertOp::OutsideRange, 950.0, 1050.0);
    String json = engine.toJSON();
    ASSERT_STR_CONTAINS(json.c_str(), "\"thresholdHigh\":");
    ASSERT_STR_CONTAINS(json.c_str(), "\"op\":\"outside_range\"");
}

TEST(ToJSON_ActiveAlert) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.check("temp", 50.0);
    String json = engine.toJSON();
    ASSERT_STR_CONTAINS(json.c_str(), "\"state\":\"active\"");
    ASSERT_STR_CONTAINS(json.c_str(), "\"fireCount\":1");
}

TEST(ActiveToJSON_Empty) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_STR_EQ(engine.activeToJSON().c_str(), "[]");
}

TEST(ActiveToJSON_WithActive) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 100.0);
    engine.check("a", 1.0);
    engine.check("b", 50.0);
    String json = engine.activeToJSON();
    ASSERT_STR_CONTAINS(json.c_str(), "\"name\":\"a\"");
    ASSERT_TRUE(std::string(json.c_str()).find("\"name\":\"b\"") == std::string::npos);
}

TEST(Summary) {
    mcpd::AlertEngine engine;
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 0.0);
    engine.check("a", 1.0);
    String s = engine.summary();
    ASSERT_STR_CONTAINS(s.c_str(), "2 rules");
    ASSERT_STR_CONTAINS(s.c_str(), "1 active");
}

// ═══════════════════════════════════════════════════════════════════════
//  Edge Cases
// ═══════════════════════════════════════════════════════════════════════

TEST(StaysActiveWhileConditionMet) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);

    int fires = 0;
    engine.onAlert([&](const mcpd::AlertEvent& e) { if (e.fired) fires++; });

    engine.check("temp", 50.0);
    engine.check("temp", 55.0);
    engine.check("temp", 60.0);
    ASSERT_EQ(fires, 1);
}

TEST(ReFireAfterClear) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);

    int fires = 0;
    engine.onAlert([&](const mcpd::AlertEvent& e) { if (e.fired) fires++; });

    engine.check("temp", 50.0);
    engine.check("temp", 30.0);
    engine.check("temp", 50.0);
    ASSERT_EQ(fires, 2);
}

TEST(NaNValue) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    ASSERT_FALSE(engine.check("temp", std::nan("")));
}

TEST(LargeValues) {
    mcpd::AlertEngine engine;
    engine.addAlert("x", mcpd::AlertOp::GreaterThan, 1e15);
    ASSERT_TRUE(engine.check("x", 2e15));
}

TEST(NegativeValues) {
    mcpd::AlertEngine engine;
    engine.addAlert("x", mcpd::AlertOp::LessThan, -10.0);
    ASSERT_TRUE(engine.check("x", -15.0));
    ASSERT_FALSE(engine.check("x", -5.0));
}

TEST(ZeroHysteresis) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp", mcpd::AlertOp::GreaterThan, 40.0);
    engine.setHysteresis("temp", 0.0);
    engine.check("temp", 41.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Active);
    engine.check("temp", 39.0);
    ASSERT_EQ(engine.getState("temp"), mcpd::AlertState::Clear);
}

TEST(AddAfterRemoveReusesSlot) {
    mcpd::AlertEngine engine(2);
    engine.addAlert("a", mcpd::AlertOp::GreaterThan, 0.0);
    engine.addAlert("b", mcpd::AlertOp::GreaterThan, 0.0);
    engine.removeAlert("a");
    ASSERT_TRUE(engine.addAlert("c", mcpd::AlertOp::GreaterThan, 0.0));
    ASSERT_EQ((int)engine.count(), 2);
}

TEST(MultipleRangeAlerts) {
    mcpd::AlertEngine engine;
    engine.addAlert("safe_zone", mcpd::AlertOp::InsideRange, 20.0, 30.0);
    engine.addAlert("danger_zone", mcpd::AlertOp::OutsideRange, 10.0, 40.0);

    engine.check("safe_zone", 25.0);
    engine.check("danger_zone", 25.0);
    ASSERT_EQ(engine.getState("safe_zone"), mcpd::AlertState::Active);
    ASSERT_EQ(engine.getState("danger_zone"), mcpd::AlertState::Clear);
}

TEST(JSONEscaping) {
    mcpd::AlertEngine engine;
    engine.addAlert("temp\"test", mcpd::AlertOp::GreaterThan, 40.0);
    String json = engine.toJSON();
    ASSERT_STR_CONTAINS(json.c_str(), "temp\\\"test");
}

// ═══════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    printf("\n📋 MCPAlerts Tests\n");
    printf("  ════════════════════════════════════════\n\n");
    TEST_SUMMARY();
    return _tests_failed > 0 ? 1 : 0;
}
