#include <exception>
#include <stdexcept>
#include <emscripten/fetch.h>

#include "tiledownloader.h"
#include "shared.h"

void download(TileDescriptor& tileDescriptor) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    attr.userData = (void*)&tileDescriptor;
    attr.onsuccess = [](emscripten_fetch_t *fetch) {
        auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);
        
        tileDescriptor->HandleSuccess(fetch);

        emscripten_fetch_close(fetch); // Free data associated with the fetch.
    };

    attr.onerror = [](emscripten_fetch_t *fetch) {
        auto tileDescriptor = reinterpret_cast<TileDescriptor*>(fetch->userData);

        tileDescriptor->HandleFailure(fetch);

        emscripten_fetch_close(fetch); // Also free data on failure.
    };

    std::unique_ptr<char[]> urlAsChar = std::make_unique<char[]>(tileDescriptor.m_url.length() + 1);
    strcpy(urlAsChar.get(), tileDescriptor.m_url.c_str());

    emscripten_fetch(&attr, tileDescriptor.m_url.c_str());
}