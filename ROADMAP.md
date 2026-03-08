# Roadmap

## 1. Native Node.js addon via N-API ✅

### Goal

Publish the map generator as a native Node.js addon (`osm-static-map-generator` on npm) so Node.js apps can render maps without WASM overhead. Uses [node-addon-api](https://github.com/nodejs/node-addon-api) (N-API) for ABI stability across Node.js versions.

### Package structure

Two separate npm packages in a `packages/` directory:

```
packages/
├── native/                     # N-API addon
│   ├── package.json            # cmake-js, node-addon-api, node-gyp-build
│   ├── CMakeLists.txt          # cmake-js build config
│   ├── src/
│   │   ├── addon.cpp           # N-API entry point
│   │   ├── render_worker.h     # AsyncWorker header
│   │   └── render_worker.cpp   # AsyncWorker implementation
│   ├── lib/
│   │   └── index.js            # JS wrapper exposing async API
│   ├── test.js                 # Smoke test (London map)
│   ├── test-partial-render.js  # Failure/rejection test
│   └── prebuilds/              # Pre-compiled .node binaries per platform
└── wasm/                       # WASM package (existing module, repackaged)
    ├── package.json
    └── lib/
        └── index.js            # JS wrapper around .mjs module
```

### N-API binding design

The addon exposes a single async function that returns an object with the PNG buffer and metadata:

```javascript
const { generateMap } = require('osm-static-map-generator');

const result = await generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13,
  tileRequestHeaders: {
    "User-Agent": "my-app/1.0"
  }
});

// result.buffer — Node.js Buffer containing the rendered PNG
// result.failedTileCount — number of tiles that failed to download
```

Uses `Napi::AsyncWorker` so the entire render (HTTP downloads + image compositing) runs on the libuv thread pool, never blocking the Node.js event loop.

### Build system

Following the [ubx-parser](https://github.com/nicmr/ubx-parser) pattern:

- **cmake-js** (not node-gyp) for building — integrates with the existing CMake setup
- **node-gyp-build** at runtime to locate prebuilt `.node` binaries
- **Prebuilds** directory structure: `prebuilds/{platform}-{arch}/node.napi.node`

---

## 2. Threading model and the event loop

### Current behavior (native mode) ✅

The tile download and rendering pipeline uses **`curl_multi`** for parallel downloads on a single thread:

1. `Render()` calls `DrawLayer()` which calculates the tile grid
2. `DownloadTiles()` calls `downloadTiles()` which uses `curl_multi_perform()` + `curl_multi_poll()` for concurrent HTTP requests
3. `CURLMOPT_MAX_TOTAL_CONNECTIONS` limits concurrency (default: 2, per OSM policy)
4. When all tiles complete, `MarkTileRequestFinished()` triggers `DrawImage()` to composite and call the callback with PNG data

The entire `Render()` call blocks on the libuv thread pool thread, not the main thread. A typical 4×4 grid now downloads in ~200ms (parallel) vs ~3.2s (sequential).

### Future phases

#### Phase 2: Thread pool for image compositing

Leptonica's `pixRasterop()` is CPU-bound. For large maps with many tiles, compositing could be parallelized:
- Process tiles in parallel (each `PrepareTile()` is independent)
- Final `pixRasterop()` calls must be sequential (single output image)
- Use `std::async` or a simple thread pool within `Execute()`

Lower priority — compositing is fast compared to network I/O.

#### Phase 3: Coroutine support (C++20)

With C++20 coroutines, `downloadTile()` could `co_await` a curl operation, allowing the compiler to manage suspension/resumption. This would enable:
- Non-blocking downloads without manual `curl_multi` state management
- Cleaner async composition
- Integration with Node.js's event loop via libuv (advanced)

Requires significant refactoring and careful integration with both Emscripten (which has its own async model) and N-API.

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

## 4. Error handling ✅

### Implemented

- **Tile download timeout:** `CURLOPT_TIMEOUT` applied from `m_tileRequestTimeout` option
- **Retry with exponential backoff:** Failed tiles retry up to `tileRetryCount` times (default: 3) with 500ms × 2^attempt backoff
- **Partial render vs hard fail:** Configurable via `allowPartialRender` (default: `true`). When `false`, the Promise rejects if any tile fails. When `true`, renders with missing tiles and reports `failedTileCount`
- **Error propagation to N-API:** Failed renders reject the Promise. Successful renders resolve with `{ buffer, failedTileCount }`

---

## 5. Request throttling ✅

Implemented via `curl_multi_setopt(CURLMOPT_MAX_TOTAL_CONNECTIONS, m_tileRequestLimit)`. Default: 2 concurrent connections per OSM tile server policy. Configurable via `tileRequestLimit` option.

---

## 6. WASM test harness

### Problem

The WASM module uses `ENVIRONMENT='web'` and `emscripten_fetch` for tile downloads, so it can only run in a browser. There is currently no way to test the WASM build.

### Proposed solution

A browser-based test harness under `packages/wasm/test/`:

- **`test/index.html`** — Loads the `.mjs` module as an ES module, registers `Module.eventHandlers` (`onMapGeneratorJobDone`, `onMapGeneratorJobError`), calls `generateMap()` with London test params. On success, reads PNG data from WASM heap (`instance.HEAPU8.buffer` at the data pointer), copies to a `Uint8Array` (C++ frees after callback returns), creates a Blob URL, and displays it on an `<img>` element. Shows timing info and byte count.

- **`test/serve.mjs`** — Zero-dependency Node.js HTTP server using built-in `http` and `fs`. Serves `packages/wasm/` directory on port 8080 with correct MIME types (`.mjs` → `text/javascript`). Sets COOP/COEP headers for WASM compatibility.

- **`package.json`** — Add `"test:serve": "node test/serve.mjs"` script.

### Usage

```bash
./cpp/build-wasm-docker.sh                    # build WASM binary
cd packages/wasm && npm run test:serve        # start server
# Open http://localhost:8080 in browser
```

---

## 7. Emscripten upgrade (3.1.64 → 5.0.2)

### Risk assessment

The upgrade crosses two major versions (4.0 and 5.0). Key findings:

| Concern | Risk | Notes |
|---------|------|-------|
| `WASM_BIGINT` enabled by default (4.0) | **None** | Only affects 64-bit types. All embind-exposed types are 32-bit (`int`, `std::string`) |
| embind library exports removed (4.0) | **None** | We only use `emscripten::function("generateMap", ...)` |
| `EM_ASM_ARGS` deprecated | **Low** | Still compiles but undocumented. Replace with `EM_ASM` (same syntax, backward-compatible with 3.1.64) |
| `--no-heap-copy` linker flag | **Medium** | Legacy flag, may be unrecognized in 5.0.2. Remove it — was already a no-op with `ALLOW_MEMORY_GROWTH=1` |
| `-s WASM=1`, `-s STANDALONE_WASM=0` | **Low** | Redundant defaults. Remove for cleanliness |
| `SINGLE_FILE` encoding change (5.0) | **Low** | Changed base64 → UTF-8 binary. Consuming HTML needs `<meta charset="utf-8">` |
| Leptonica C code + newer Clang | **Medium** | May trigger `-Wimplicit-int` errors. `-Wno-implicit-int` flag is already present |
| `npm install` in EMSDK dir (`CMakeLists.txt:14`) | **Medium** | May not be needed in 5.0.2. Keep for now, remove if it errors |
| Fetch API (`emscripten_fetch`) | **None** | Stable API, unchanged |
| embind requires C++17 (5.0) | **None** | Project uses C++23 |

### Changes needed

1. **`cpp/Dockerfile.wasm`** — Bump `emscripten/emsdk:3.1.64` → `emscripten/emsdk:5.0.2`
2. **`cpp/main.cpp`** — Replace `EM_ASM_ARGS` with `EM_ASM` (lines 13, 20). Backward-compatible change.
3. **`cpp/CMakeLists.txt`** — Remove obsolete WASM linker flags: `--no-heap-copy`, `-s WASM=1`, `-s STANDALONE_WASM=0`
4. **`README.md`**, **`CLAUDE.md`** — Update emscripten version references

### Implementation sequence

1. Build WASM test harness first (item 6) against 3.1.64 as baseline
2. Make backward-compatible C++ changes (`EM_ASM`, flag cleanup) — verify they still build on 3.1.64
3. Bump Docker image tag to 5.0.2
4. Rebuild and test with WASM test harness
5. Update documentation

---

## Implementation order

| Step | Description | Status | Depends on |
|------|-------------|--------|------------|
| 1 | Create `packages/native/` N-API addon | ✅ Done | — |
| 2 | Create `packages/wasm/` structure, relocate WASM output | ✅ Done | — |
| 3 | `curl_multi` parallel downloads + headers + timeout + throttling | ✅ Done | 1 |
| 4 | Retry with exponential backoff + `allowPartialRender` + failure reporting | ✅ Done | 3 |
| 5 | WASM test harness (browser-based) | Pending | 2 |
| 6 | Emscripten upgrade 3.1.64 → 5.0.2 | Pending | 5 |
| 7 | CI workflow (GitHub Actions) | Pending | 1, 2 |
| 8 | Thread pool for image compositing | Pending | 3 |
| 9 | Coroutine support (C++20) | Pending | 3 |
