set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "15.0")

# Enable per-function/data sections so -dead_strip can strip unused code from vcpkg libs
set(VCPKG_C_FLAGS "-ffunction-sections -fdata-sections")
set(VCPKG_CXX_FLAGS "-ffunction-sections -fdata-sections")
