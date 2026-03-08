# osm-static-map-generator

A C++ library that generates static map images by compositing [OpenStreetMap](https://www.openstreetmap.org/) raster tiles. Supports both **WebAssembly** and **native C++** targets.

> **Important:** If you use this library, please follow [OpenStreetMap's Licence/Attribution Guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines).

## Features

- Render map images at any center point (longitude/latitude) and zoom level
- Configurable output dimensions, tile size, padding, and zoom range
- Custom tile server URLs with `{x}`, `{y}`, `{z}` token substitution
- Multiple tile layer support
- Custom HTTP headers for tile requests
- WASM build exposes `generateMap()` to JavaScript via Emscripten bindings
- Native build produces a static library linkable from C++

## Background

This library was written to power a C++ map component for [React bindings for Dear ImGui](https://github.com/nicmr/react-imgui). It was inspired by [staticmaps](https://github.com/StephanGeorg/staticmaps) (Node.js), and uses [Leptonica](https://github.com/DanBloomberg/leptonica) for image compositing — the only image library that reliably works under WebAssembly.

## Prerequisites

- **CMake** >= 3.8
- **Ninja** build system
- **Git submodules** initialized (`git submodule update --init --recursive`)

For the **WASM build** (pick one):
- **Docker** (recommended) — no other setup needed
- **Emscripten SDK** installed locally with `EMSDK` environment variable set

For the **native build** (x64 Windows):
- **MSVC** (Visual Studio Build Tools or Visual Studio with C++ workload)
- vcpkg automatically installs remaining dependencies (JPEG, PNG, TIFF, CURL)

## Building

### Clone with submodules

```bash
git clone --recurse-submodules https://github.com/andreamancuso/osm-static-map-generator.git
cd osm-static-map-generator
```

### WASM build (Docker — recommended)

```bash
./cpp/build-wasm-docker.sh
```

This builds a Docker image from `cpp/Dockerfile.wasm` (based on `emscripten/emsdk:3.1.64` with `pkg-config` and `ninja-build`), then runs the CMake build inside the container. Output: `ts/src/lib/wasm/osmStaticMapGenerator.mjs` (single-file WASM module).

### WASM build (local Emscripten)

Requires the `EMSDK` environment variable to be set:

```bash
cd cpp
cmake -S . -B build -GNinja
cmake --build ./build --target osmStaticMapGenerator
```

The CMake configure step automatically generates the required headers for Leptonica (`endianness.h`) and libtiff (`tiffconf.h`, `tif_config.h`).

### Native build (x64 Windows)

From an x64 Developer Command Prompt:

```bash
cd cpp
cmake -S . -B build -GNinja
cmake --build ./build --target osmStaticMapGenerator
```

Output: `cpp/build/osmStaticMapGenerator.lib`

vcpkg (included as a submodule) automatically downloads and builds the native dependencies: nlohmann-json, libjpeg-turbo, libpng, tiff, and curl.

## Usage (WASM)

The WASM module exposes a single `generateMap()` function that accepts a JSON string:

```javascript
import initModule from './osmStaticMapGenerator.mjs';

const module = await initModule({
  eventHandlers: {
    onMapGeneratorJobDone: (dataPtr, numBytes) => {
      // dataPtr: pointer to PNG image data
      // numBytes: size of the PNG data
    },
    onMapGeneratorJobError: (errorMessage) => {
      console.error(errorMessage);
    }
  }
});

module.generateMap(JSON.stringify({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },  // longitude, latitude
  zoom: 13,
  tileSize: 256,                          // optional, default: 256
  tileUrl: "https://tile.openstreetmap.org/{z}/{x}/{y}.png",  // optional
  tileRequestHeaders: {},                 // optional
  zoomRange: { min: 1, max: 17 },        // optional
  paddingX: 0,                            // optional
  paddingY: 0,                            // optional
  reverseY: false                         // optional (for TMS tile servers)
}));
```

## Usage (Native C++)

```cpp
#include "mapgenerator.h"

MapGeneratorOptions options;
options.m_width = 800;
options.m_height = 600;

auto callback = [](void* data, size_t numBytes) {
    // data: pointer to PNG image data
    // numBytes: size of the PNG data
};

MapGenerator generator(options, callback);
generator.Render({-0.1276, 51.5074}, 13);  // {longitude, latitude}, zoom
```

## Architecture

The rendering pipeline:

1. **MapGenerator::Render()** — receives center coordinates and zoom level
2. **DrawLayer()** — calculates which tiles are needed based on the viewport
3. **DownloadTiles()** — fetches tiles asynchronously (Emscripten Fetch API for WASM, libcurl for native)
4. **MarkTileRequestFinished()** — tracks tile completion (thread-safe via mutex)
5. **DrawImage()** — composites all tiles into the final image using Leptonica's `pixRasterop()`
6. **Callback** — delivers the resulting PNG data

Platform differences (WASM vs native) are abstracted via `#ifdef __EMSCRIPTEN__` in `shared.cpp`, `tiledownloader.cpp`, and `main.cpp`.

## Dependencies

| Library | Purpose | WASM | Native |
|---------|---------|------|--------|
| [Leptonica](https://github.com/DanBloomberg/leptonica) | Image compositing | submodule | submodule |
| [libtiff](https://gitlab.com/libtiff/libtiff) | TIFF support (for Leptonica) | submodule | vcpkg |
| [libpng](https://github.com/pnggroup/libpng) | PNG encoding/decoding | Emscripten port | vcpkg |
| [libjpeg](https://libjpeg-turbo.org/) | JPEG support | Emscripten port | vcpkg |
| [libcurl](https://curl.se/libcurl/) | HTTP tile downloads | N/A (uses Fetch API) | vcpkg |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing | vcpkg | vcpkg |

## Roadmap

- Proper error handling/recovery with retry mechanism
- Tests
- Request throttling
- Multithreading support
- Coroutine support

## License

[MIT](LICENSE) — Copyright (c) 2024 Andrea Mancuso
