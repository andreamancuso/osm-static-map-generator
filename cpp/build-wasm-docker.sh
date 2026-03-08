#!/usr/bin/env bash
# Build the WASM target using the emscripten/emsdk Docker image.
# Run from the repository root: ./cpp/build-wasm-docker.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Convert to Windows path if running under MSYS/Git Bash (needed for Docker volume mounts)
if command -v cygpath &>/dev/null; then
  REPO_ROOT="$(cygpath -w "$REPO_ROOT")"
fi

docker build -t osm-emsdk -f "$SCRIPT_DIR/Dockerfile.wasm" "$SCRIPT_DIR"
docker run --rm \
  -v "$REPO_ROOT":/src \
  osm-emsdk \
  bash -c "cd /src/cpp && cmake -S . -B build-wasm -GNinja && cmake --build ./build-wasm --target osmStaticMapGenerator"
