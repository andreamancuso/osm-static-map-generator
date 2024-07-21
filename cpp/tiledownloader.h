#include <future>
#include <exception>
#include <stdexcept>
#include <emscripten/fetch.h>
#include "shared.h"

#pragma once

void downloadSucceeded(emscripten_fetch_t *fetch);

void downloadFailed(emscripten_fetch_t *fetch);

void download(TileDescriptor& tileDescriptor);
