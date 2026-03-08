# Roadmap

## 1. Native Node.js addon via N-API

### Goal

Publish the map generator as a native Node.js addon (`osm-static-map-generator` on npm) so Node.js apps can render maps without WASM overhead. Uses [node-addon-api](https://github.com/nodejs/node-addon-api) (N-API) for ABI stability across Node.js versions.

### Package structure

Two separate npm packages in a `packages/` directory:

```
packages/
├── native/                     # N-API addon (this section)
│   ├── package.json            # cmake-js, node-addon-api, node-gyp-build
│   ├── CMakeLists.txt          # cmake-js build config
│   ├── src/
│   │   └── addon.cpp           # N-API bindings wrapping MapGenerator
│   ├── lib/
│   │   └── index.js            # JS wrapper exposing async API
│   └── prebuilds/              # Pre-compiled .node binaries per platform
└── wasm/                       # WASM package (existing module, repackaged)
    ├── package.json
    └── lib/
        └── index.js            # JS wrapper around .mjs module
```

**Why separate packages:** WASM and native serve different consumers. WASM runs in browsers and is fully portable. The native addon is Node.js-only but avoids WASM overhead and uses real HTTP (libcurl) instead of browser Fetch. Different install footprints, different dependencies, different platforms.

### N-API binding design

The addon exposes a single async function:

```javascript
const { generateMap } = require('osm-static-map-generator');

const pngBuffer = await generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13
});

// pngBuffer is a Node.js Buffer containing the rendered PNG
```

**C++ side (`addon.cpp`):**

```
generateMap(jsonString) → parse options → create MapGenerator → Render() → PNG Buffer
```

Uses `Napi::AsyncWorker` so the entire render (HTTP downloads + image compositing) runs on the libuv thread pool, never blocking the Node.js event loop.

```cpp
class RenderWorker : public Napi::AsyncWorker {
    // Execute() — runs on thread pool:
    //   1. Parse JSON into MapGeneratorOptions
    //   2. Create MapGenerator with callback that captures PNG data
    //   3. Call Render(center, zoom)
    //   4. Entire flow completes synchronously (see threading section below)
    //
    // OnOK() — runs on main thread:
    //   Copy captured PNG data into Napi::Buffer
    //   Resolve the Promise
};
```

### Build system

Following the [ubx-parser](https://github.com/nicmr/ubx-parser) pattern:

- **cmake-js** (not node-gyp) for building — integrates with the existing CMake setup
- **node-gyp-build** at runtime to locate prebuilt `.node` binaries
- **Prebuilds** directory structure: `prebuilds/{platform}-{arch}/node.napi.node`

`packages/native/CMakeLists.txt` compiles:
- `addon.cpp` (N-API entry point)
- `shared.cpp`, `mapgenerator.cpp`, `tiledownloader.cpp` (existing library sources)
- Leptonica sources (from submodule, same glob pattern)
- Links: `${CMAKE_JS_LIB}`, JPEG, PNG, TIFF, CURL

Does NOT compile libtiff sources — uses system/vcpkg libtiff (same as the existing native build).

### WASM output relocation

The WASM build currently outputs to `ts/src/lib/wasm/`. This should change to `packages/wasm/lib/` to match the new structure. Requires updating `RUNTIME_OUTPUT_DIRECTORY` in `cpp/CMakeLists.txt`.

---

## 2. Threading model and the event loop

### Current behavior (native mode)

The tile download and rendering pipeline is **fully synchronous** in native mode:

1. `Render()` calls `DrawLayer()` which calculates the tile grid
2. `DownloadTiles()` loops over each tile and calls `downloadTile()` **sequentially**
3. Each `downloadTile()` calls `curl_easy_perform()` — a **blocking** HTTP request
4. After each download completes, `HandleSuccess()` stores the data and `MarkTileRequestFinished()` records completion
5. When all tiles are done, `DrawImage()` composites them and calls the callback with PNG data

**The entire `Render()` call blocks until all tiles are downloaded and the image is composited.** For a typical map (e.g., 4×4 grid = 16 tiles at 200ms each), this is ~3.2 seconds of blocking time — all spent waiting on sequential HTTP requests.

### Why this is fine for N-API (for now)

`Napi::AsyncWorker::Execute()` runs on the **libuv thread pool**, not the main thread. Blocking there does not freeze the Node.js event loop. The user's JavaScript code gets a Promise that resolves when rendering is complete.

**Caveat:** The libuv thread pool defaults to 4 threads (`UV_THREADPOOL_SIZE`). If multiple `generateMap()` calls are in flight simultaneously, they compete for threads. With sequential downloads, each render ties up a thread for seconds. Heavy concurrent usage could exhaust the pool, causing DNS resolution and file I/O to queue behind map renders.

### How to avoid blocking the event loop

The AsyncWorker pattern already avoids blocking the event loop. The question is how to reduce the total time a worker thread is occupied:

#### Phase 1: Parallel downloads with `curl_multi`

Replace the sequential `curl_easy_perform()` loop with `curl_multi_perform()`. This allows all tile downloads to run concurrently on a single thread:

```
Current:  tile1 ──200ms──> tile2 ──200ms──> ... tile16 ──200ms──>  (3.2s total)
Improved: tile1 ──200ms──>
          tile2 ──200ms──>
          ...              (all overlap)
          tile16 ──200ms──>                                         (~200ms total)
```

**Implementation:** In `tiledownloader.cpp`, add a `downloadTilesParallel(std::vector<TileDescriptor*>)` function that:
1. Creates a `CURLM*` multi handle
2. Adds all tiles as easy handles
3. Loops on `curl_multi_perform()` + `curl_multi_wait()` until all complete
4. Processes results via `curl_multi_info_read()`

This is a targeted change to `tiledownloader.cpp` and `mapgenerator.cpp` (call the new function instead of looping). No API changes needed.

#### Phase 2: Thread pool for image compositing

Leptonica's `pixRasterop()` is CPU-bound. For large maps with many tiles, compositing could be parallelized:
- Process tiles in parallel (each `PrepareTile()` is independent)
- Final `pixRasterop()` calls must be sequential (single output image)
- Use `std::async` or a simple thread pool within `Execute()`

This is lower priority — compositing is fast compared to network I/O.

#### Phase 3: Coroutine support (C++20)

The existing code already has "coroutines" in the roadmap. With C++20 coroutines, `downloadTile()` could `co_await` a curl operation, allowing the compiler to manage suspension/resumption. This would enable:
- Non-blocking downloads without manual `curl_multi` state management
- Cleaner async composition
- Integration with Node.js's event loop via libuv (advanced)

This is the long-term ideal but requires significant refactoring and careful integration with both Emscripten (which has its own async model) and N-API.

---

## 3. CI workflow (GitHub Actions)

### Overview

A `.github/workflows/build.yml` inspired by [ubx-parser's workflow](https://github.com/nicmr/ubx-parser) with two build targets:

### Job 1: `build-wasm`

Runs on `ubuntu-latest`. Uses the existing Docker-based build:

```yaml
steps:
  - Checkout with submodules (recursive)
  - Build Docker image from cpp/Dockerfile.wasm
  - Run WASM build inside container
  - Upload osmStaticMapGenerator.mjs as artifact
```

### Job 2: `build-native`

Matrix build across platforms:

| Runner | Platform | Arch | Dependencies |
|--------|----------|------|--------------|
| `ubuntu-latest` | linux | x64 | `apt: libjpeg-dev libpng-dev libtiff-dev libcurl4-openssl-dev` |
| `ubuntu-24.04-arm` | linux | arm64 | same as above |
| `macos-14` | darwin | arm64 | `brew: jpeg libpng libtiff curl` (most pre-installed) |
| `windows-latest` | win32 | x64 | vcpkg (already configured in the project) |

Steps:
```yaml
- Checkout with submodules (recursive)
- Setup Node.js 22
- Install system dependencies (platform-specific)
- cd packages/native && npm install --ignore-scripts
- npx cmake-js compile
- Copy .node to prebuilds/{platform}-{arch}/node.napi.node
- Run smoke test
- Upload prebuild artifact
```

### Job 3: `publish`

Runs on tag push (`v*`), after both build jobs succeed:
- Downloads all prebuild artifacts
- Publishes `packages/native` to npm (with prebuilds included)
- Publishes `packages/wasm` to npm

### Platform-specific concerns

**Windows:** The `cmake-js` + vcpkg integration needs the vcpkg toolchain file. The existing `CMakeLists.txt` already handles this via `deps/vcpkg/scripts/buildsystems/vcpkg.cmake`. Windows CI may need `VCPKG_TARGET_TRIPLET=x64-windows` set explicitly. MSVC also requires generating `node.lib` from `node.def` (cmake-js handles this, see ubx-parser's CMakeLists.txt pattern).

**macOS:** Most image libraries ship with Xcode Command Line Tools or Homebrew. `curl` is pre-installed. May need `brew install libtiff`.

**Linux ARM64:** The `ubuntu-24.04-arm` runner is relatively new. Package availability should be the same as x64. Cross-compilation is NOT needed — GitHub provides native ARM runners.

### Trigger paths

```yaml
on:
  push:
    branches: [main]
    paths: ['cpp/**', 'packages/**', '.github/workflows/build.yml']
    tags: ['v*']
  pull_request:
    branches: [main]
    paths: ['cpp/**', 'packages/**', '.github/workflows/build.yml']
```

---

## 4. Error handling improvements

### Current state

- `MarkTileRequestFinished()` receives `successOrFailure` but does nothing on failure (`// TODO: how to handle this?`)
- `PrepareTile()` returns `false` on PNG decode failure but the caller just skips it silently (`// TODO: log?`)
- No retry mechanism for failed tile downloads
- No timeout on curl requests (`CURLOPT_TIMEOUT` not set)

### Proposed improvements

1. **Tile download timeout:** Set `CURLOPT_TIMEOUT` based on `m_tileRequestTimeout` (the option already exists but is never applied to curl)
2. **Retry with backoff:** On download failure, retry up to N times with exponential backoff before marking the tile as failed
3. **Partial render vs hard fail:** Decide on a policy — render with missing tiles (leave blank) or fail the entire render. The current code silently skips failed tiles, which produces maps with holes. This should be configurable.
4. **Error propagation to N-API:** Failed renders should reject the Promise with a descriptive error, not silently produce a broken image

---

## 5. Request throttling

### Current state

`m_tileRequestLimit` exists as a config option but is never enforced (`// TODO: use it`).

### Proposed implementation

Limit concurrent in-flight tile requests. With the `curl_multi` approach (Phase 1 above), this maps directly to `curl_multi_setopt(CURLMOPT_MAX_TOTAL_CONNECTIONS, m_tileRequestLimit)`. This is important for respecting tile server rate limits and being a good OSM citizen.

---

## Implementation order

| Step | Description | Depends on |
|------|-------------|------------|
| 1 | Create `packages/native/` structure (package.json, CMakeLists.txt, addon.cpp, lib/index.js) | — |
| 2 | Create `packages/wasm/` structure, relocate WASM output | — |
| 3 | Get native addon building and passing a smoke test locally | 1 |
| 4 | Set up `.github/workflows/build.yml` with WASM + native matrix | 1, 2 |
| 5 | Add `curl_multi` parallel downloads | 3 |
| 6 | Add download timeout and retry logic | 5 |
| 7 | Add request throttling (`CURLMOPT_MAX_TOTAL_CONNECTIONS`) | 5 |
| 8 | Evaluate coroutine support | 5 |
