# osm-static-map-generator

A C++ library that generates static map images by compositing [OpenStreetMap](https://www.openstreetmap.org/) raster tiles. Available as a **Node.js native addon** and a **WebAssembly module** for browsers.

> **Important:** If you use this library, please follow [OpenStreetMap's Licence/Attribution Guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines).

## Packages

| Package | Description | Install |
|---------|-------------|---------|
| [osm-static-map-generator](https://www.npmjs.com/package/osm-static-map-generator) | Native Node.js addon with prebuilt binaries for linux-x64, linux-arm64, darwin-arm64, win32-x64 | `npm install osm-static-map-generator` |
| [osm-static-map-generator-wasm](https://www.npmjs.com/package/osm-static-map-generator-wasm) | WebAssembly module for browser use | `npm install osm-static-map-generator-wasm` |

## Features

- Render map images at any center point (longitude/latitude) and zoom level
- Configurable output dimensions, tile size, padding, and zoom range
- Custom tile server URLs with `{x}`, `{y}`, `{z}` token substitution
- Parallel tile downloads with throttling
- Partial render support when some tiles fail
- Custom HTTP headers for tile requests

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

This builds a Docker image from `cpp/Dockerfile.wasm` (based on `emscripten/emsdk:5.0.2` with `pkg-config` and `ninja-build`), then runs the CMake build inside the container. Output: `packages/wasm/lib/osmStaticMapGenerator.mjs` (single-file WASM module).

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

## Usage (Node.js)

```js
const { generateMap } = require('osm-static-map-generator');
const fs = require('fs');

const result = await generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 }, // longitude, latitude
  zoom: 13,
  tileRequestHeaders: {
    "User-Agent": "my-app/1.0"
  }
});

fs.writeFileSync('map.png', result.buffer);
console.log(`${result.buffer.length} bytes, ${result.failedTileCount} failed tiles`);
```

## Usage (Browser / WASM)

```js
import createModule from 'osm-static-map-generator-wasm/lib/osmStaticMapGenerator.mjs';

const instance = await createModule({
  eventHandlers: {
    onMapGeneratorJobDone: (dataPtr, numBytes) => {
      const data = new Uint8Array(instance.HEAPU8.buffer, dataPtr, numBytes);
      const png = new Uint8Array(data); // copy before WASM memory is freed
      const blob = new Blob([png], { type: 'image/png' });
      document.getElementById('map').src = URL.createObjectURL(blob);
    },
    onMapGeneratorJobError: (errorMessage) => {
      console.error(errorMessage);
    }
  }
});

instance.generateMap(JSON.stringify({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13
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

[MIT](LICENSE)
