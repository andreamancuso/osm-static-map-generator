set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Enable per-function/data sections so --gc-sections can strip unused code from vcpkg libs
set(VCPKG_C_FLAGS "-ffunction-sections -fdata-sections")
set(VCPKG_CXX_FLAGS "-ffunction-sections -fdata-sections")
