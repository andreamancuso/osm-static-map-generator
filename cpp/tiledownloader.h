#include "shared.h"

#pragma once

#ifdef __EMSCRIPTEN__
void downloadTile(TileDescriptor* tileDescriptor);
#else
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

void downloadTiles(
    const std::vector<std::unique_ptr<TileDescriptor>>& tileDescriptors,
    const std::unordered_map<std::string, std::string>& headers,
    int timeoutSeconds,
    int maxConnections,
    int maxRetries
);
#endif
