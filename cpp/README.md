# Building the WASM

For Leptonica, create endianness.h: `#define L_LITTLE_ENDIAN`

For libtiff, run `emcmake cmake ..` from within a newly created `build` folder

Add `-Wno-implicit-int` to compiler flags (libtiff uses an implicit int in a function)

- Build the WASM:
  - `cd cpp`
  - `cmake -S . -B build -GNinja`
  - `cmake --build ./build --target osmStaticMapGenerator`
