#pragma once

#include "shared.h"

#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdint>

#ifdef __EMSCRIPTEN__
void downloadTile(TileDescriptor* tileDescriptor);
#else
void downloadTiles(
    const std::vector<std::unique_ptr<TileDescriptor>>& tileDescriptors,
    const std::unordered_map<std::string, std::string>& headers,
    int timeoutSeconds,
    int maxConnections,
    int maxRetries
);
#endif

// Decoupled single-tile download API.
// Desktop: synchronous (blocking) — call from a background thread.
// WASM: asynchronous — callback fires on the event loop.
using TileFetchCallback = std::function<void(bool success, std::vector<uint8_t> data)>;

void fetchTile(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    TileFetchCallback callback
);
