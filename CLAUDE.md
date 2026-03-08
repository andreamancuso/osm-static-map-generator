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

**WASM vs Native:** The build mode is auto-detected via the `EMSDK` environment variable. If set, builds a WASM `.mjs` module output to `ts/src/lib/wasm/`. Otherwise, builds a native library using vcpkg-provided libraries (CURL, JPEG, PNG, TIFF).

**WASM build via Docker** (no local Emscripten install needed):

```bash
./cpp/build-wasm-docker.sh
```

Uses `cpp/Dockerfile.wasm` (based on `emscripten/emsdk:5.0.2` with `pkg-config` and `ninja-build` baked in). The libtiff configure step is automated in CMakeLists.txt via `execute_process`. Output goes to `ts/src/lib/wasm/osmStaticMapGenerator.mjs`.

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

Conditional compilation via `#ifdef __EMSCRIPTEN__` in `shared.cpp`, `tiledownloader.cpp`, and `main.cpp`. WASM mode uses Emscripten Fetch API and JS callbacks via `EM_ASM`; native mode uses libcurl.

### Dependencies

- **vcpkg** (submodule at `cpp/deps/vcpkg`) — manages nlohmann-json, and for native builds: libjpeg-turbo, libpng, tiff, curl
- **Git submodules** — leptonica, libtiff (WASM build only), emscripten (if not using Docker)
- **Emscripten ports** — libpng, libjpeg (WASM only)
- **Docker alternative** — `emscripten/emsdk` image replaces the emscripten submodule for WASM builds

## Emscripten Upgrade Notes (3.1.64 → 5.0.2)

The following changes were required when upgrading across two major versions (4.0 and 5.0). Document these for future upgrades.

### Changes made

1. **`EM_ASM_ARGS` → `EM_ASM`** (`cpp/main.cpp`): `EM_ASM_ARGS` was deprecated. `EM_ASM` now accepts variadic arguments with the same `$0`, `$1` positional syntax. This change is backward-compatible with 3.x.

2. **Removed obsolete linker flags** (`cpp/CMakeLists.txt`):
   - `--no-heap-copy` — legacy flag, no-op with `ALLOW_MEMORY_GROWTH=1`, unrecognized in 5.x
   - `-s WASM=1` — redundant default since 4.0
   - `-s STANDALONE_WASM=0` — redundant default since 4.0

3. **Exported `HEAPU8`** (`cpp/CMakeLists.txt`): Added `-s EXPORTED_RUNTIME_METHODS=['HEAPU8']` to linker flags. Emscripten 5.x no longer exports heap views by default. The WASM test harness reads PNG data from `instance.HEAPU8.buffer`.

4. **Docker image tag** (`cpp/Dockerfile.wasm`): `emscripten/emsdk:3.1.64` → `emscripten/emsdk:5.0.2`.

### What didn't need changing

- **`WASM_BIGINT` default ON (4.0)**: Only affects 64-bit types. All embind-exposed types are 32-bit (`int`, `std::string`).
- **embind library exports removed (4.0)**: We only use `emscripten::function("generateMap", ...)`.
- **embind requires C++17 (5.0)**: Project uses C++23.
- **Fetch API (`emscripten_fetch`)**: Stable, unchanged.
- **`MODULARIZE` factory returns Promise (4.0)**: Already handled with `await`.
- **`SINGLE_FILE` encoding changed to UTF-8 (5.0)**: Test harness HTML already includes `<meta charset="utf-8">`.

### Future upgrade checklist

When upgrading Emscripten again:
1. Check the [changelog](https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md) for breaking changes
2. Update `cpp/Dockerfile.wasm` image tag
3. Rebuild: `./cpp/build-wasm-docker.sh`
4. Test: `cd packages/wasm && npm run test:serve` → open http://localhost:8080
5. Watch for: deprecated macros, removed linker flags, runtime methods no longer exported by default, Leptonica C code warnings with newer Clang

## Current Limitations

No tests exist yet. Error handling for tile request failures is incomplete. Tile request limit/throttling not yet implemented.
