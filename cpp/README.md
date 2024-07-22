# Building the WASM

For Leptonica, create endianness.h: `#define L_LITTLE_ENDIAN`

For libtiff, run `emcmake cmake ..` from within a newly created `build` folder

- Build the WASM:
  - `cd cpp`
  - `cmake -S . -B build -GNinja`
  - `cmake --build ./build --target osmStaticMapGenerator`
