/**
 * mcpd — Tool Result Cache
 *
 * Caches tool call results with configurable per-tool TTL.
 * Ideal for sensor tools on MCU where hardware reads are expensive
 * (e.g., DHT sensor needs 2s between reads, I2C scans are slow).
 *
 * Cache keys are computed from tool name + serialized arguments,
 * so identical calls return cached results within the TTL window.
 */

#ifndef MCPD_CACHE_H
#define MCPD_CACHE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>

namespace mcpd {

struct CacheEntry {
    String result;          // Cached result string
    unsigned long cachedAt; // millis() when cached
    unsigned long ttlMs;    // Time-to-live in milliseconds
    bool isError;           // Whether the cached result was an error

    bool isValid() const {
        if (ttlMs == 0) return false;
        return (millis() - cachedAt) < ttlMs;
    }

    unsigned long ageMs() const {
        return millis() - cachedAt;
    }

    unsigned long remainingMs() const {
        if (!isValid()) return 0;
        return ttlMs - ageMs();
    }
};

/**
 * Tool result cache with per-tool TTL and bounded size.
 *
 * Usage:
 *   mcp.cache().setToolTTL("temperature_read", 2000);  // cache for 2s
 *   mcp.cache().setToolTTL("gpio_read", 500);           // cache for 500ms
 *   mcp.cache().setMaxEntries(32);                      // limit memory usage
 *   mcp.enableCache();                                  // activate caching
 */
class ToolResultCache {
public:
    ToolResultCache() = default;

    /**
     * Set the cache TTL for a specific tool.
     * Only tools with explicit TTL are cached.
     * @param toolName  Tool name
     * @param ttlMs     Cache duration in milliseconds (0 = disable for this tool)
     */
    void setToolTTL(const char* toolName, unsigned long ttlMs) {
        if (ttlMs == 0) {
            _toolTTLs.erase(String(toolName));
        } else {
            _toolTTLs[String(toolName)] = ttlMs;
        }
    }

    /**
     * Get the configured TTL for a tool (0 = not cached).
     */
    unsigned long getToolTTL(const char* toolName) const {
        auto it = _toolTTLs.find(String(toolName));
        return (it != _toolTTLs.end()) ? it->second : 0;
    }

    /**
     * Check if a tool has caching configured.
     */
    bool isToolCached(const char* toolName) const {
        return _toolTTLs.find(String(toolName)) != _toolTTLs.end();
    }

    /**
     * Look up a cached result. Returns true if a valid (non-expired) entry exists.
     * @param toolName  Tool name
     * @param argsJson  Serialized arguments JSON
     * @param[out] result  The cached result string
     * @param[out] isError Whether the cached result was an error
     * @return true if cache hit
     */
    bool get(const char* toolName, const String& argsJson,
             String& result, bool& isError) {
        if (!_enabled) return false;

        String key = _makeKey(toolName, argsJson);
        auto it = _entries.find(key);
        if (it == _entries.end()) return false;

        if (!it->second.isValid()) {
            _entries.erase(it);
            return false;
        }

        result = it->second.result;
        isError = it->second.isError;
        _hits++;
        return true;
    }

    /**
     * Store a result in the cache.
     * Only stores if the tool has a configured TTL.
     */
    void put(const char* toolName, const String& argsJson,
             const String& result, bool isError = false) {
        if (!_enabled) return;

        unsigned long ttl = getToolTTL(toolName);
        if (ttl == 0) return;

        // Evict expired entries if at capacity
        if (_entries.size() >= _maxEntries) {
            _evictExpired();
        }
        // If still at capacity, evict oldest
        if (_entries.size() >= _maxEntries) {
            _evictOldest();
        }

        String key = _makeKey(toolName, argsJson);
        CacheEntry entry;
        entry.result = result;
        entry.cachedAt = millis();
        entry.ttlMs = ttl;
        entry.isError = isError;
        _entries[key] = entry;
        _misses++;
    }

    /**
     * Invalidate all cached results for a specific tool.
     */
    void invalidateTool(const char* toolName) {
        String prefix = String(toolName) + ":";
        auto it = _entries.begin();
        while (it != _entries.end()) {
            if (it->first.startsWith(prefix)) {
                it = _entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * Invalidate a specific cached result.
     */
    void invalidate(const char* toolName, const String& argsJson) {
        String key = _makeKey(toolName, argsJson);
        _entries.erase(key);
    }

    /**
     * Clear the entire cache.
     */
    void clear() {
        _entries.clear();
        _hits = 0;
        _misses = 0;
    }

    /**
     * Enable or disable the cache.
     */
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }

    /**
     * Set maximum number of cache entries (default: 32).
     * Keeps memory usage bounded on MCU.
     */
    void setMaxEntries(size_t max) { _maxEntries = max; }
    size_t getMaxEntries() const { return _maxEntries; }

    /**
     * Get cache statistics.
     */
    size_t size() const { return _entries.size(); }
    unsigned long hits() const { return _hits; }
    unsigned long misses() const { return _misses; }
    float hitRate() const {
        unsigned long total = _hits + _misses;
        return (total > 0) ? (float)_hits / (float)total : 0.0f;
    }

    /**
     * Get stats as JSON string for diagnostics.
     */
    String statsJson() const {
        JsonDocument doc;
        doc["enabled"] = _enabled;
        doc["entries"] = _entries.size();
        doc["maxEntries"] = _maxEntries;
        doc["hits"] = _hits;
        doc["misses"] = _misses;
        doc["hitRate"] = hitRate();
        doc["toolCount"] = _toolTTLs.size();
        String out;
        serializeJson(doc, out);
        return out;
    }

private:
    bool _enabled = false;
    size_t _maxEntries = 32;
    unsigned long _hits = 0;
    unsigned long _misses = 0;

    std::map<String, unsigned long> _toolTTLs;  // tool name → TTL in ms
    std::map<String, CacheEntry> _entries;       // key → entry

    /**
     * Build a cache key from tool name and arguments.
     * Key format: "toolName:argsHash" where argsHash is a simple
     * string hash to keep keys short on MCU.
     */
    String _makeKey(const char* toolName, const String& argsJson) const {
        // Simple DJB2 hash of the arguments
        unsigned long hash = 5381;
        for (size_t i = 0; i < argsJson.length(); i++) {
            hash = ((hash << 5) + hash) + (unsigned char)argsJson[i];
        }
        return String(toolName) + ":" + String(hash);
    }

    void _evictExpired() {
        auto it = _entries.begin();
        while (it != _entries.end()) {
            if (!it->second.isValid()) {
                it = _entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    void _evictOldest() {
        if (_entries.empty()) return;
        auto oldest = _entries.begin();
        unsigned long oldestAge = 0;
        for (auto it = _entries.begin(); it != _entries.end(); ++it) {
            unsigned long age = it->second.ageMs();
            if (age > oldestAge) {
                oldestAge = age;
                oldest = it;
            }
        }
        _entries.erase(oldest);
    }
};

} // namespace mcpd

#endif // MCPD_CACHE_H
