cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_CXX_STANDARD 23)

# ---- vcpkg toolchain (MUST be set before project()) ----
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/deps/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "vcpkg toolchain file")

# Auto-detect vcpkg triplet based on platform
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(WIN32)
        if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
            set(VCPKG_TARGET_TRIPLET "arm64-windows")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-windows")
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-osx")
        endif()
    else()
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(VCPKG_TARGET_TRIPLET "arm64-linux")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-linux")
        endif()
    endif()
endif()

project(osm-static-map-generator)

# ---- cmake-js v8: generate node.lib from .def file on Windows ----
if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF}
                    /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

include_directories(${CMAKE_JS_INC})

# ---- Paths ----
set(CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp)
set(DEPS ${CPP_DIR}/deps)

# ---- Leptonica compile definitions ----
add_compile_definitions(HAVE_LIBJPEG HAVE_LIBTIFF HAVE_LIBPNG)

# ---- Generate endianness.h for leptonica ----
file(WRITE "${DEPS}/leptonica/src/endianness.h" "#define L_LITTLE_ENDIAN\n")

# ---- Leptonica sources ----
file(GLOB LEPTONICA_SRC "${DEPS}/leptonica/src/*.c")

# ---- Build leptonica as a separate static C library ----
add_library(leptonica_lib STATIC ${LEPTONICA_SRC})
target_include_directories(leptonica_lib PRIVATE ${DEPS}/leptonica/src)
if(MSVC)
    target_compile_options(leptonica_lib PRIVATE /W0)
else()
    target_compile_options(leptonica_lib PRIVATE -w)
    set_target_properties(leptonica_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ---- Find vcpkg packages ----
find_package(nlohmann_json REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(CURL REQUIRED)

# Link image libs into leptonica
target_link_libraries(leptonica_lib PRIVATE JPEG::JPEG PNG::PNG TIFF::TIFF)

# ---- Source files ----
set(SOURCE_FILES
    src/addon.cpp
    src/render_worker.cpp
    ${CPP_DIR}/shared.cpp
    ${CPP_DIR}/mapgenerator.cpp
    ${CPP_DIR}/tiledownloader.cpp
)

# ---- MSVC flags ----
if(MSVC)
    add_compile_options(/bigobj /MP)
endif()
if(NOT MSVC)
    add_link_options("$<$<CONFIG:Release>:-s>")
endif()

# ---- Addon shared library ----
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX "" SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${PROJECT_NAME} PRIVATE "$<$<PLATFORM_ID:Linux>:-Wl,--gc-sections>")
endif()

target_link_libraries(${PROJECT_NAME}
    ${CMAKE_JS_LIB}
    leptonica_lib
    nlohmann_json::nlohmann_json
    JPEG::JPEG
    PNG::PNG
    TIFF::TIFF
    CURL::libcurl
)

# ---- Include Node-API wrappers ----
execute_process(COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${NODE_ADDON_API_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CPP_DIR}
    ${DEPS}/leptonica/src
)

# ---- N-API version ----
add_definitions(-DNAPI_VERSION=9)
