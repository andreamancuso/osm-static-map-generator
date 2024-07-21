For Leptonica, create endianness.h: `#define L_LITTLE_ENDIAN`

- Build the WASM:
  - `cd cpp`
  - `cmake -S . -B build -GNinja`
  - `cmake --build ./build --target osmStaticMapGenerator`
