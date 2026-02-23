/**
 * mcpd — Tool Pipelines
 *
 * Chain multiple tool calls into atomic sequences that execute on the MCU,
 * reducing network round-trips and enabling transactional hardware operations.
 *
 * Usage:
 *   mcpd::Pipeline pipeline;
 *   pipeline.define("read_and_display")
 *       .step("dht_read", {{"pin", "4"}})
 *       .step("lcd_write", {{"text", "$prev.temperature"}})
 *       .onError(mcpd::ErrorPolicy::Rollback);
 *
 *   pipeline.commit();
 *   pipeline.setDispatcher([&](const String& tool, const String& params) { ... });
 *   auto result = pipeline.execute("read_and_display");
 *
 * Features:
 *   - Named reusable pipelines with builder API
 *   - Step-to-step data passing via $prev.field references
 *   - Error policies: Stop, Continue, Rollback (with undo callbacks)
 *   - Conditional steps (skip if predicate fails)
 *   - Runtime pipeline creation/modification
 *   - Bounded step count (MCU memory safety)
 *   - Execution results include per-step timing and status
 *   - JSON status output for diagnostics
 */

#ifndef MCPD_PIPELINE_H
#define MCPD_PIPELINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <map>

namespace mcpd {

struct StepResult {
    String stepName;
    bool success = false;
    String output;
    String error;
    unsigned long durationMs = 0;
};

struct PipelineResult {
    String pipelineName;
    bool success = false;
    std::vector<StepResult> steps;
    unsigned long totalDurationMs = 0;
    int stepsExecuted = 0;
    int stepsFailed = 0;
    String error;

    String toJson() const {
        JsonDocument doc;
        doc["pipeline"] = pipelineName;
        doc["success"] = success;
        doc["stepsExecuted"] = stepsExecuted;
        doc["stepsFailed"] = stepsFailed;
        doc["totalDurationMs"] = totalDurationMs;
        if (error.length() > 0) doc["error"] = error;

        JsonArray arr = doc["steps"].to<JsonArray>();
        for (const auto& s : steps) {
            JsonObject obj = arr.add<JsonObject>();
            obj["step"] = s.stepName;
            obj["success"] = s.success;
            obj["durationMs"] = s.durationMs;
            if (s.output.length() > 0) {
                JsonDocument parsed;
                if (deserializeJson(parsed, s.output) == DeserializationError::Ok) {
                    obj["output"] = parsed.as<JsonVariant>();
                } else {
                    obj["output"] = s.output;
                }
            }
            if (s.error.length() > 0) obj["error"] = s.error;
        }

        String result;
        serializeJson(doc, result);
        return result;
    }
};

struct PipelineStep {
    String toolName;
    std::map<String, String> params;
    std::function<bool(const PipelineResult&)> condition;
    std::function<void()> undoCallback;
    String description;
    unsigned long timeoutMs = 5000;
};

enum class ErrorPolicy {
    Stop,
    Continue,
    Rollback
};

class PipelineBuilder {
public:
    PipelineBuilder(const String& name) : _name(name) {}

    PipelineBuilder& step(const String& toolName, const std::map<String, String>& params = {}) {
        PipelineStep s;
        s.toolName = toolName;
        s.params = params;
        _steps.push_back(s);
        return *this;
    }

    PipelineBuilder& describe(const String& desc) {
        if (!_steps.empty()) _steps.back().description = desc;
        return *this;
    }

    PipelineBuilder& when(std::function<bool(const PipelineResult&)> cond) {
        if (!_steps.empty()) _steps.back().condition = cond;
        return *this;
    }

    PipelineBuilder& undo(std::function<void()> undoFn) {
        if (!_steps.empty()) _steps.back().undoCallback = undoFn;
        return *this;
    }

    PipelineBuilder& timeout(unsigned long ms) {
        if (!_steps.empty()) _steps.back().timeoutMs = ms;
        return *this;
    }

    PipelineBuilder& onError(ErrorPolicy policy) {
        _errorPolicy = policy;
        return *this;
    }

    const String& name() const { return _name; }
    const std::vector<PipelineStep>& steps() const { return _steps; }
    ErrorPolicy errorPolicy() const { return _errorPolicy; }

private:
    String _name;
    std::vector<PipelineStep> _steps;
    ErrorPolicy _errorPolicy = ErrorPolicy::Stop;
};

class Pipeline {
public:
    using ToolDispatcher = std::function<String(const String& toolName, const String& paramsJson)>;

    static constexpr size_t MAX_PIPELINES = 16;
    static constexpr size_t MAX_STEPS_PER_PIPELINE = 20;

    Pipeline() = default;

    PipelineBuilder& define(const String& name) {
        _builders.emplace_back(name);
        return _builders.back();
    }

    int commit() {
        int count = 0;
        for (auto& builder : _builders) {
            if (_pipelines.size() >= MAX_PIPELINES) break;
            if (builder.steps().size() > MAX_STEPS_PER_PIPELINE) continue;
            if (builder.steps().empty()) continue;

            PipelineDef def;
            def.name = builder.name();
            def.steps = builder.steps();
            def.errorPolicy = builder.errorPolicy();

            bool replaced = false;
            for (auto& existing : _pipelines) {
                if (existing.name == def.name) {
                    existing = def;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) _pipelines.push_back(def);
            count++;
        }
        _builders.clear();
        return count;
    }

    void setDispatcher(ToolDispatcher dispatcher) {
        _dispatcher = dispatcher;
    }

    PipelineResult execute(const String& name, const String& overrides = "{}") {
        PipelineResult result;
        result.pipelineName = name;

        PipelineDef* def = nullptr;
        for (auto& p : _pipelines) {
            if (p.name == name) { def = &p; break; }
        }
        if (!def) {
            result.error = "Pipeline not found: " + name;
            return result;
        }
        if (!_dispatcher) {
            result.error = "No tool dispatcher set";
            return result;
        }

        JsonDocument overrideDoc;
        deserializeJson(overrideDoc, overrides);

        String prevOutput = "{}";
        unsigned long totalStart = millis();

        for (const auto& step : def->steps) {
            if (step.condition && !step.condition(result)) {
                StepResult sr;
                sr.stepName = step.toolName;
                sr.success = true;
                sr.output = "{\"skipped\":true}";
                result.steps.push_back(sr);
                result.stepsExecuted++;
                continue;
            }

            JsonDocument paramDoc;
            for (const auto& kv : step.params) {
                String val = kv.second;
                val = _substituteRefs(val, prevOutput, overrideDoc);
                paramDoc[kv.first.c_str()] = val;
            }

            // Apply per-tool overrides
            if (!overrideDoc[step.toolName.c_str()].isNull()) {
                JsonObject toolOverrides = overrideDoc[step.toolName.c_str()].as<JsonObject>();
                for (JsonPair p : toolOverrides) {
                    paramDoc[p.key()] = p.value();
                }
            }

            String paramsJson;
            serializeJson(paramDoc, paramsJson);

            StepResult sr;
            sr.stepName = step.toolName;
            unsigned long stepStart = millis();

            sr.output = _dispatcher(step.toolName, paramsJson);
            sr.durationMs = millis() - stepStart;

            JsonDocument outputDoc;
            bool parseOk = (deserializeJson(outputDoc, sr.output) == DeserializationError::Ok);
            bool hasError = parseOk && !outputDoc["error"].isNull();

            if (hasError) {
                sr.success = false;
                sr.error = outputDoc["error"].as<String>();
                result.stepsFailed++;
            } else {
                sr.success = true;
                prevOutput = sr.output;
            }

            result.steps.push_back(sr);
            result.stepsExecuted++;

            if (!sr.success) {
                if (def->errorPolicy == ErrorPolicy::Stop) {
                    result.error = "Step failed: " + step.toolName;
                    break;
                } else if (def->errorPolicy == ErrorPolicy::Rollback) {
                    result.error = "Step failed: " + step.toolName + " (rolling back)";
                    _rollback(result, def->steps);
                    break;
                }
            }
        }

        result.totalDurationMs = millis() - totalStart;
        result.success = (result.stepsFailed == 0);
        return result;
    }

    bool remove(const String& name) {
        for (auto it = _pipelines.begin(); it != _pipelines.end(); ++it) {
            if (it->name == name) { _pipelines.erase(it); return true; }
        }
        return false;
    }

    bool exists(const String& name) const {
        for (const auto& p : _pipelines) {
            if (p.name == name) return true;
        }
        return false;
    }

    std::vector<String> list() const {
        std::vector<String> names;
        for (const auto& p : _pipelines) names.push_back(p.name);
        return names;
    }

    size_t count() const { return _pipelines.size(); }

    String info(const String& name) const {
        for (const auto& p : _pipelines) {
            if (p.name == name) {
                JsonDocument doc;
                doc["name"] = p.name;
                doc["steps"] = (int)p.steps.size();
                doc["errorPolicy"] = p.errorPolicy == ErrorPolicy::Stop ? "stop" :
                                     p.errorPolicy == ErrorPolicy::Continue ? "continue" : "rollback";
                JsonArray arr = doc["stepDetails"].to<JsonArray>();
                for (const auto& s : p.steps) {
                    JsonObject obj = arr.add<JsonObject>();
                    obj["tool"] = s.toolName;
                    if (s.description.length() > 0) obj["description"] = s.description;
                    obj["timeoutMs"] = s.timeoutMs;
                    obj["hasCondition"] = (bool)s.condition;
                    obj["hasUndo"] = (bool)s.undoCallback;
                }
                String result;
                serializeJson(doc, result);
                return result;
            }
        }
        return "{\"error\":\"Pipeline not found\"}";
    }

    String toJson() const {
        JsonDocument doc;
        doc["count"] = (int)_pipelines.size();
        doc["maxPipelines"] = (int)MAX_PIPELINES;
        doc["maxStepsPerPipeline"] = (int)MAX_STEPS_PER_PIPELINE;
        JsonArray arr = doc["pipelines"].to<JsonArray>();
        for (const auto& p : _pipelines) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"] = p.name;
            obj["steps"] = (int)p.steps.size();
            obj["errorPolicy"] = p.errorPolicy == ErrorPolicy::Stop ? "stop" :
                                 p.errorPolicy == ErrorPolicy::Continue ? "continue" : "rollback";
        }
        String result;
        serializeJson(doc, result);
        return result;
    }

private:
    struct PipelineDef {
        String name;
        std::vector<PipelineStep> steps;
        ErrorPolicy errorPolicy = ErrorPolicy::Stop;
    };

    std::vector<PipelineDef> _pipelines;
    std::vector<PipelineBuilder> _builders;
    ToolDispatcher _dispatcher;

    String _substituteRefs(const String& val, const String& prevOutput, const JsonDocument& overrides) {
        if (val.indexOf("$prev.") < 0 && val.indexOf("$override.") < 0) {
            return val;
        }

        String result = val;

        if (result.indexOf("$prev.") >= 0) {
            JsonDocument prevDoc;
            if (deserializeJson(prevDoc, prevOutput) == DeserializationError::Ok) {
                int pos;
                while ((pos = result.indexOf("$prev.")) >= 0) {
                    int end = pos + 6;
                    while (end < (int)result.length() &&
                           (isalnum(result.charAt(end)) || result.charAt(end) == '_')) {
                        end++;
                    }
                    String field = result.substring(pos + 6, end);
                    String replacement = "";
                    if (!prevDoc[field.c_str()].isNull()) {
                        replacement = prevDoc[field.c_str()].as<String>();
                    }
                    result = result.substring(0, pos) + replacement + result.substring(end);
                }
            }
        }

        if (result.indexOf("$override.") >= 0) {
            int pos;
            while ((pos = result.indexOf("$override.")) >= 0) {
                int end = pos + 10;
                while (end < (int)result.length() &&
                       (isalnum(result.charAt(end)) || result.charAt(end) == '_')) {
                    end++;
                }
                String field = result.substring(pos + 10, end);
                String replacement = "";
                if (!overrides[field.c_str()].isNull()) {
                    replacement = overrides[field.c_str()].as<String>();
                }
                result = result.substring(0, pos) + replacement + result.substring(end);
            }
        }

        return result;
    }

    void _rollback(const PipelineResult& result, const std::vector<PipelineStep>& steps) {
        for (int i = (int)result.steps.size() - 2; i >= 0; i--) {
            if (result.steps[i].success && i < (int)steps.size() && steps[i].undoCallback) {
                steps[i].undoCallback();
            }
        }
    }
};

} // namespace mcpd

#endif // MCPD_PIPELINE_H
