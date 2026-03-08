# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ library that generates static map images from OpenStreetMap raster tiles. Supports dual compilation: WebAssembly (via Emscripten) and native C++. Uses Leptonica for image compositing. C++23 standard.

**Important:** Follow [OpenStreetMap's Licence/Attribution Guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines).

## Build Commands

All commands run from the `cpp/` directory:

```bash
cd cpp
cmake -S . -B build -GNinja
cmake --build ./build --target osmStaticMapGenerator
```

**WASM vs Native:** The build mode is auto-detected via the `EMSDK` environment variable. If set, builds a WASM `.mjs` module output to `ts/src/lib/wasm/`. Otherwise, builds a native library using system libraries (CURL, JPEG, PNG, TIFF).

**WASM prerequisite:** For Leptonica, create `endianness.h` with `#define L_LITTLE_ENDIAN`. For libtiff, run `emcmake cmake ..` from a `build` folder first.

## Architecture

### Rendering Pipeline

```
JSON Options → MapGeneratorOptions → MapGenerator::Render(center, zoom)
  → DrawLayer() (calculate tile grid for viewport)
  → DownloadTiles() (Emscripten Fetch API or CURL)
  → MarkTileRequestFinished() (thread-safe, mutex-protected)
  → DrawImage() (composite tiles via Leptonica pixRasterop)
  → PNG output via callback
```

### Key Source Files (all in `cpp/`)

- **main.cpp** — WASM entry point. `WasmRunner` manages concurrent map generation jobs. Emscripten bindings expose `generateMap()` to JavaScript.
- **mapgenerator.h/cpp** — Core engine. `MapGenerator` converts geo coordinates to tile coordinates, orchestrates tile downloads, and composites the final image. `MapGeneratorOptions` holds config (dimensions, tile size, zoom range, tile URL, headers, etc.).
- **shared.h/cpp** — `TileDescriptor` (per-tile state + image data), coordinate transform functions (`lonToX`, `latToY`, etc.), `replaceTokens()` template for URL token substitution.
- **tiledownloader.h/cpp** — Platform-abstracted tile fetching. Uses `emscripten_fetch` in WASM mode, libcurl in native mode.

### Platform Abstraction

Conditional compilation via `#ifdef __EMSCRIPTEN__` in `shared.cpp`, `tiledownloader.cpp`, and `main.cpp`. WASM mode uses Emscripten Fetch API and JS callbacks via `EM_ASM_ARGS`; native mode uses libcurl.

### Dependencies

- **vcpkg** (submodule at `cpp/deps/vcpkg`) — manages `nlohmann-json`
- **Git submodules** — leptonica, libtiff, emscripten (all in `cpp/deps/`)
- **Emscripten ports** — libpng, libjpeg (WASM only)
- **System libraries** (native only) — CURL, JPEG, PNG, TIFF

## Current Limitations

No tests exist yet. Error handling for tile request failures is incomplete. Tile request limit/throttling not yet implemented.
