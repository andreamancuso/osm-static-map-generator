#pragma once

#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class TileCache {
public:
    TileCache(int maxEntries = 256, int ttlMs = 3600000);

    std::optional<std::vector<uint8_t>> get(const std::string& url);
    void put(const std::string& url, const void* data, size_t numBytes);
    void configure(int maxEntries, int ttlMs);

    static TileCache& getGlobalInstance();

private:
    struct CacheEntry {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point insertedAt;
    };

    void evict();

    int m_maxEntries;
    int m_ttlMs;
    std::mutex m_mutex;
    std::unordered_map<std::string, CacheEntry> m_entries;
    std::list<std::string> m_lruOrder; // most recent at front
};
