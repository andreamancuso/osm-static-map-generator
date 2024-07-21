#include <exception>
#include <stdexcept>
#include <emscripten/fetch.h>

#include "tiledownloader.h"
#include "shared.h"

void downloadSucceeded(emscripten_fetch_t *fetch) {
    printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
  
    auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);
    
    printf("About to mark event as set\n");

    tileDescriptor->HandleSuccess(fetch);

    printf("Marked event as set\n");

    emscripten_fetch_close(fetch); // Free data associated with the fetch.
}

void downloadFailed(emscripten_fetch_t *fetch) {
    printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);

    auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);

    tileDescriptor->HandleFailure(fetch);

    emscripten_fetch_close(fetch); // Also free data on failure.
}

void download(TileDescriptor& tileDescriptor) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = (void*)&tileDescriptor;
    attr.onsuccess = downloadSucceeded;
    attr.onerror = downloadFailed;

    // printf("Inside promise d %s\n", tileDescriptor.url.c_str());

    std::unique_ptr<char[]> urlAsChar = std::make_unique<char[]>(tileDescriptor.m_url.length() + 1);
    strcpy(urlAsChar.get(), tileDescriptor.m_url.c_str());

    emscripten_fetch(&attr, tileDescriptor.m_url.c_str());
}