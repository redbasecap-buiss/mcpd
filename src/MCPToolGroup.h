/**
 * mcpd — Tool Groups
 *
 * Organize tools into named groups for bulk enable/disable and logical grouping.
 * Useful for MCU projects with many tools (e.g., "sensors", "actuators", "network").
 */

#ifndef MCPD_TOOL_GROUP_H
#define MCPD_TOOL_GROUP_H

#include <Arduino.h>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace mcpd {

/**
 * Represents a named group of tools.
 */
struct ToolGroup {
    String name;              // Group identifier (e.g., "sensors")
    String description;       // Human-readable description
    bool enabled = true;      // Group-level enable/disable
    std::set<String> tools;   // Tool names in this group

    ToolGroup() = default;
    ToolGroup(const char* name, const char* desc = "")
        : name(name), description(desc) {}
};

/**
 * Manages tool groups — logical groupings for bulk operations.
 *
 * Tools can belong to multiple groups. Disabling a group disables
 * all its tools (unless they belong to another enabled group).
 *
 * Usage:
 *   manager.createGroup("sensors", "All sensor tools");
 *   manager.addToolToGroup("temperature_read", "sensors");
 *   manager.addToolToGroup("humidity_read", "sensors");
 *   manager.disableGroup("sensors");  // disables both tools
 */
class ToolGroupManager {
public:
    /**
     * Create a new tool group.
     * @return true if created, false if already exists
     */
    bool createGroup(const char* name, const char* description = "") {
        String key(name);
        if (_groups.count(key)) return false;
        _groups[key] = ToolGroup(name, description);
        return true;
    }

    /**
     * Remove a tool group.
     * @return true if removed, false if not found
     */
    bool removeGroup(const char* name) {
        String key(name);
        auto it = _groups.find(key);
        if (it == _groups.end()) return false;
        // Remove reverse mappings
        for (auto& tool : it->second.tools) {
            auto tit = _toolToGroups.find(tool);
            if (tit != _toolToGroups.end()) {
                tit->second.erase(key);
                if (tit->second.empty()) _toolToGroups.erase(tit);
            }
        }
        _groups.erase(it);
        return true;
    }

    /**
     * Add a tool to a group.
     * Creates the group if it doesn't exist.
     * @return true if the tool was added (not already in group)
     */
    bool addToolToGroup(const char* toolName, const char* groupName) {
        String gKey(groupName);
        String tKey(toolName);
        if (!_groups.count(gKey)) {
            createGroup(groupName);
        }
        bool inserted = _groups[gKey].tools.insert(tKey).second;
        if (inserted) {
            _toolToGroups[tKey].insert(gKey);
        }
        return inserted;
    }

    /**
     * Remove a tool from a group.
     * @return true if removed
     */
    bool removeToolFromGroup(const char* toolName, const char* groupName) {
        String gKey(groupName);
        String tKey(toolName);
        auto it = _groups.find(gKey);
        if (it == _groups.end()) return false;
        bool erased = it->second.tools.erase(tKey) > 0;
        if (erased) {
            auto tit = _toolToGroups.find(tKey);
            if (tit != _toolToGroups.end()) {
                tit->second.erase(gKey);
                if (tit->second.empty()) _toolToGroups.erase(tit);
            }
        }
        return erased;
    }

    /**
     * Enable or disable a group.
     * @return true if group exists
     */
    bool enableGroup(const char* name, bool enabled = true) {
        String key(name);
        auto it = _groups.find(key);
        if (it == _groups.end()) return false;
        it->second.enabled = enabled;
        return true;
    }

    bool disableGroup(const char* name) { return enableGroup(name, false); }

    /**
     * Check if a group is enabled.
     */
    bool isGroupEnabled(const char* name) const {
        String key(name);
        auto it = _groups.find(key);
        if (it == _groups.end()) return false;
        return it->second.enabled;
    }

    /**
     * Check if a group exists.
     */
    bool hasGroup(const char* name) const {
        return _groups.count(String(name)) > 0;
    }

    /**
     * Check if a tool is disabled by group membership.
     * A tool is group-disabled if ALL its groups are disabled.
     * Tools with no group are never group-disabled.
     */
    bool isToolGroupDisabled(const char* toolName) const {
        String tKey(toolName);
        auto it = _toolToGroups.find(tKey);
        if (it == _toolToGroups.end()) return false;  // no group = not group-disabled
        // If tool is in at least one enabled group, it's not disabled
        for (auto& gName : it->second) {
            auto git = _groups.find(gName);
            if (git != _groups.end() && git->second.enabled) return false;
        }
        return true;  // all groups disabled
    }

    /**
     * Get all tools in a group.
     */
    std::set<String> getToolsInGroup(const char* name) const {
        String key(name);
        auto it = _groups.find(key);
        if (it == _groups.end()) return {};
        return it->second.tools;
    }

    /**
     * Get all groups a tool belongs to.
     */
    std::set<String> getGroupsForTool(const char* toolName) const {
        String tKey(toolName);
        auto it = _toolToGroups.find(tKey);
        if (it == _toolToGroups.end()) return {};
        return it->second;
    }

    /**
     * Get all group names.
     */
    std::vector<String> getGroupNames() const {
        std::vector<String> names;
        names.reserve(_groups.size());
        for (auto& pair : _groups) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * Get a group by name (const).
     * @return pointer to group, or nullptr if not found
     */
    const ToolGroup* getGroup(const char* name) const {
        String key(name);
        auto it = _groups.find(key);
        if (it == _groups.end()) return nullptr;
        return &it->second;
    }

    /**
     * Get total number of groups.
     */
    size_t groupCount() const { return _groups.size(); }

    /**
     * Serialize all groups to JSON.
     * Returns: [{"name":"sensors","description":"...","enabled":true,"tools":["a","b"]}, ...]
     */
    String toJson() const {
        String json = "[";
        bool first = true;
        for (auto& pair : _groups) {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"" + pair.second.name + "\"";
            if (pair.second.description.length() > 0) {
                json += ",\"description\":\"" + pair.second.description + "\"";
            }
            json += ",\"enabled\":";
            json += pair.second.enabled ? "true" : "false";
            json += ",\"tools\":[";
            bool tfirst = true;
            for (auto& t : pair.second.tools) {
                if (!tfirst) json += ",";
                tfirst = false;
                json += "\"" + t + "\"";
            }
            json += "]}";
        }
        json += "]";
        return json;
    }

private:
    std::map<String, ToolGroup> _groups;
    std::map<String, std::set<String>> _toolToGroups;  // reverse index: tool → groups
};

} // namespace mcpd

#endif // MCPD_TOOL_GROUP_H
