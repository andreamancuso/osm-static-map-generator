# cpp

See the [root README](../README.md) for build instructions.

The CMake build automatically handles:
- Generating `endianness.h` for Leptonica
- Configuring libtiff to generate `tiffconf.h` and `tif_config.h` (WASM builds)
- Installing vcpkg dependencies (native builds)
