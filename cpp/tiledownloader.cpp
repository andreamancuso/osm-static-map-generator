#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <curl/curl.h>
#endif

#include "tiledownloader.h"
#include "shared.h"
#include "mapgenerator.h"

#ifndef __EMSCRIPTEN__
#include "tilecache.h"
#endif

#ifdef __EMSCRIPTEN__
void downloadTile(TileDescriptor* tileDescriptor) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = (void*)tileDescriptor;
    attr.onsuccess = [](emscripten_fetch_t *fetch) {
        if (fetch->userData) {
            auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);
            tileDescriptor->HandleSuccess(fetch);
        } else {
            printf("Error: fetch->userData is null in onsuccess callback.\n");
        }

        emscripten_fetch_close(fetch); // Free data associated with the fetch.
    };

    attr.onerror = [](emscripten_fetch_t *fetch) {
        if (fetch->userData) {
            auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);
            tileDescriptor->HandleFailure(fetch);
        } else {
            printf("Error: fetch->userData is null in onerror callback.\n");
        }

        emscripten_fetch_close(fetch); // Also free data on failure.
    };

    emscripten_fetch(&attr, tileDescriptor->m_url.c_str());
}
#else
#include <thread>
#include <chrono>

static CURL* createEasyHandle(
    TileDescriptor* td,
    struct curl_slist* headerList,
    int timeoutSeconds
) {
    CURL* easy = curl_easy_init();
    if (!easy) return nullptr;

    curl_easy_setopt(easy, CURLOPT_URL, td->m_url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,
        +[](void* buffer, size_t sz, size_t n, void* userdata) -> size_t {
            size_t totalBytes = sz * n;
            reinterpret_cast<TileDescriptor*>(userdata)->AppendData(buffer, totalBytes);
            return totalBytes;
        });
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, td);

    if (headerList) {
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headerList);
    }
    if (timeoutSeconds > 0) {
        curl_easy_setopt(easy, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    }
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);

    return easy;
}

void downloadTiles(
    const std::vector<std::unique_ptr<TileDescriptor>>& tileDescriptors,
    const std::unordered_map<std::string, std::string>& headers,
    int timeoutSeconds,
    int maxConnections,
    int maxRetries
) {
    CURLM* multiHandle = curl_multi_init();
    if (!multiHandle) {
        for (auto& td : tileDescriptors) {
            td->HandleFailure();
        }
        return;
    }

    // Build shared header list
    struct curl_slist* headerList = nullptr;
    for (const auto& [key, value] : headers) {
        std::string headerLine = key + ": " + value;
        headerList = curl_slist_append(headerList, headerLine.c_str());
    }

    // Throttle concurrent connections
    if (maxConnections > 0) {
        curl_multi_setopt(multiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS,
                          static_cast<long>(maxConnections));
    }

    // Start with all tiles pending
    std::vector<TileDescriptor*> pendingTiles;
    for (auto& td : tileDescriptors) {
        pendingTiles.push_back(td.get());
    }

    for (int attempt = 0; attempt <= maxRetries && !pendingTiles.empty(); attempt++) {
        // Backoff before retry (not on first attempt)
        if (attempt > 0) {
            int delayMs = 500 * (1 << (attempt - 1)); // 500ms, 1s, 2s, 4s...
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }

        // Create easy handles for pending tiles
        std::unordered_map<CURL*, TileDescriptor*> handleToTile;

        for (auto* td : pendingTiles) {
            if (td->m_success.has_value()) continue; // already served from cache
            td->ResetData();
            CURL* easy = createEasyHandle(td, headerList, timeoutSeconds);
            if (!easy) {
                td->HandleFailure();
                continue;
            }
            curl_multi_add_handle(multiHandle, easy);
            handleToTile[easy] = td;
        }

        // Drive all transfers to completion
        int runningHandles = 0;
        do {
            CURLMcode mc = curl_multi_perform(multiHandle, &runningHandles);
            if (mc != CURLM_OK) break;
            if (runningHandles > 0) {
                curl_multi_poll(multiHandle, nullptr, 0, 1000, nullptr);
            }
        } while (runningHandles > 0);

        // Process results, collect failures for retry
        std::vector<TileDescriptor*> failedTiles;
        int msgsInQueue = 0;
        CURLMsg* msg = nullptr;
        while ((msg = curl_multi_info_read(multiHandle, &msgsInQueue))) {
            if (msg->msg == CURLMSG_DONE) {
                CURL* easy = msg->easy_handle;
                auto it = handleToTile.find(easy);
                if (it != handleToTile.end()) {
                    TileDescriptor* td = it->second;
                    if (msg->data.result == CURLE_OK) {
                        td->m_success.emplace(true);
                        TileCache::getGlobalInstance().put(td->m_url, td->m_data, td->m_numBytes);
                        td->m_mapGeneratorPtr->MarkTileRequestFinished(td->m_id, true);
                    } else if (attempt < maxRetries) {
                        failedTiles.push_back(td);
                    } else {
                        td->HandleFailure();
                    }
                }
            }
        }

        // Cleanup easy handles from this round
        for (auto& [easy, td] : handleToTile) {
            curl_multi_remove_handle(multiHandle, easy);
            curl_easy_cleanup(easy);
        }

        pendingTiles = std::move(failedTiles);
    }

    // Cleanup
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_multi_cleanup(multiHandle);
}
#endif
