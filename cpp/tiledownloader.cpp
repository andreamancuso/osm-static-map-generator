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
void downloadTile(TileDescriptor* tileDescriptor) {
    CURL *curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, tileDescriptor->m_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void *buffer, size_t sz, size_t n, void *pThis) -> size_t {
            size_t totalBytes = sz * n;
            auto td = reinterpret_cast<TileDescriptor*>(pThis);
            td->AppendData(buffer, totalBytes);
            return totalBytes;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, tileDescriptor);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            tileDescriptor->m_success.emplace(true);
            tileDescriptor->m_mapGeneratorPtr->MarkTileRequestFinished(tileDescriptor->m_id, true);
        } else {
            tileDescriptor->HandleFailure();
        }
    }
}
#endif
