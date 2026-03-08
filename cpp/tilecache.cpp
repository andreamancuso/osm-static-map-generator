#include "tilecache.h"

TileCache::TileCache(int maxEntries, int ttlMs)
    : m_maxEntries(maxEntries), m_ttlMs(ttlMs) {}

TileCache& TileCache::getGlobalInstance() {
    static TileCache instance;
    return instance;
}

void TileCache::configure(int maxEntries, int ttlMs) {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_maxEntries = maxEntries;
    m_ttlMs = ttlMs;

    // Evict if the new max is smaller
    while (m_maxEntries > 0 && static_cast<int>(m_entries.size()) > m_maxEntries) {
        auto& oldest = m_lruOrder.back();
        m_entries.erase(oldest);
        m_lruOrder.pop_back();
    }
}

std::optional<std::vector<uint8_t>> TileCache::get(const std::string& url) {
    const std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(url);
    if (it == m_entries.end()) {
        return std::nullopt;
    }

    // Check TTL
    if (m_ttlMs > 0) {
        auto age = std::chrono::steady_clock::now() - it->second.insertedAt;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(age).count() > m_ttlMs) {
            m_lruOrder.remove(url);
            m_entries.erase(it);
            return std::nullopt;
        }
    }

    // Move to front of LRU list
    m_lruOrder.remove(url);
    m_lruOrder.push_front(url);

    return it->second.data;
}

void TileCache::put(const std::string& url, const void* data, size_t numBytes) {
    const std::lock_guard<std::mutex> lock(m_mutex);

    if (m_maxEntries <= 0) return;

    // Update existing entry
    auto it = m_entries.find(url);
    if (it != m_entries.end()) {
        it->second.data.assign(static_cast<const uint8_t*>(data),
                               static_cast<const uint8_t*>(data) + numBytes);
        it->second.insertedAt = std::chrono::steady_clock::now();
        m_lruOrder.remove(url);
        m_lruOrder.push_front(url);
        return;
    }

    // Evict if at capacity
    evict();

    // Insert new entry
    CacheEntry entry;
    entry.data.assign(static_cast<const uint8_t*>(data),
                      static_cast<const uint8_t*>(data) + numBytes);
    entry.insertedAt = std::chrono::steady_clock::now();

    m_entries[url] = std::move(entry);
    m_lruOrder.push_front(url);
}

void TileCache::evict() {
    // Remove expired entries first
    if (m_ttlMs > 0) {
        auto now = std::chrono::steady_clock::now();
        auto it = m_lruOrder.rbegin();
        while (it != m_lruOrder.rend()) {
            auto entryIt = m_entries.find(*it);
            if (entryIt != m_entries.end()) {
                auto age = now - entryIt->second.insertedAt;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(age).count() > m_ttlMs) {
                    m_entries.erase(entryIt);
                    // Convert reverse_iterator to forward iterator for erase
                    it = std::make_reverse_iterator(m_lruOrder.erase(std::next(it).base()));
                    continue;
                }
            }
            ++it;
        }
    }

    // Evict LRU entries if still at capacity
    while (static_cast<int>(m_entries.size()) >= m_maxEntries) {
        auto& oldest = m_lruOrder.back();
        m_entries.erase(oldest);
        m_lruOrder.pop_back();
    }
}
