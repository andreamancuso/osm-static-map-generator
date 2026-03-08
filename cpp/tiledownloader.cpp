#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <curl/curl.h>
#endif

#include "tiledownloader.h"
#include "shared.h"
#include "mapgenerator.h"

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
void downloadTiles(
    const std::vector<std::unique_ptr<TileDescriptor>>& tileDescriptors,
    const std::unordered_map<std::string, std::string>& headers,
    int timeoutSeconds,
    int maxConnections
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

    // Create an easy handle per tile
    std::unordered_map<CURL*, TileDescriptor*> handleToTile;

    for (auto& td : tileDescriptors) {
        CURL* easy = curl_easy_init();
        if (!easy) {
            td->HandleFailure();
            continue;
        }

        curl_easy_setopt(easy, CURLOPT_URL, td->m_url.c_str());
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION,
            +[](void* buffer, size_t sz, size_t n, void* userdata) -> size_t {
                size_t totalBytes = sz * n;
                reinterpret_cast<TileDescriptor*>(userdata)->AppendData(buffer, totalBytes);
                return totalBytes;
            });
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, td.get());

        if (headerList) {
            curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headerList);
        }

        if (timeoutSeconds > 0) {
            curl_easy_setopt(easy, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
        }

        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);

        curl_multi_add_handle(multiHandle, easy);
        handleToTile[easy] = td.get();
    }

    // Drive all transfers to completion
    int runningHandles = 0;
    do {
        CURLMcode mc = curl_multi_perform(multiHandle, &runningHandles);
        if (mc != CURLM_OK) {
            break;
        }
        if (runningHandles > 0) {
            curl_multi_poll(multiHandle, nullptr, 0, 1000, nullptr);
        }
    } while (runningHandles > 0);

    // Process results
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
                    td->m_mapGeneratorPtr->MarkTileRequestFinished(td->m_id, true);
                } else {
                    td->HandleFailure();
                }
            }
        }
    }

    // Cleanup
    for (auto& [easy, td] : handleToTile) {
        curl_multi_remove_handle(multiHandle, easy);
        curl_easy_cleanup(easy);
    }
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_multi_cleanup(multiHandle);
}
#endif
