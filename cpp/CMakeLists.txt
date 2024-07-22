cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE deps/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten")


if(NOT DEFINED ENV{EMSDK})
    message(FATAL_ERROR "Environment variable EMSDK is not defined. Follow the README steps and source emsdk_env.* from the EMSDK root directory in this shell before running CMake")
endif()


set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE $ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake)

execute_process(
        COMMAND "npm install"
        WORKING_DIRECTORY "$ENV{EMSDK}/upstream/emscripten"
)

project(osmStaticMapGenerator LANGUAGES C CXX)

add_compile_definitions(HAVE_LIBJPEG HAVE_LIBTIFF HAVE_LIBPNG)

# add_definitions(-std=c++23)
set(CMAKE_CXX_STANDARD 23)

find_package(Stb REQUIRED)
find_package(nlohmann_json REQUIRED)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CMAKE_EXECUTABLE_SUFFIX ".mjs")

file(GLOB LEPTONICA_SRC "${DEPS}/leptonica/src/*.c")

set(LIBTIFF_SRC
    ${DEPS}/libtiff/libtiff/tif_aux.c
    ${DEPS}/libtiff/libtiff/tif_close.c
    ${DEPS}/libtiff/libtiff/tif_codec.c
    ${DEPS}/libtiff/libtiff/tif_color.c
    ${DEPS}/libtiff/libtiff/tif_compress.c
    ${DEPS}/libtiff/libtiff/tif_dir.c
    ${DEPS}/libtiff/libtiff/tif_dirinfo.c
    ${DEPS}/libtiff/libtiff/tif_dirread.c
    ${DEPS}/libtiff/libtiff/tif_dirwrite.c
    ${DEPS}/libtiff/libtiff/tif_dumpmode.c
    ${DEPS}/libtiff/libtiff/tif_error.c
    ${DEPS}/libtiff/libtiff/tif_extension.c
    ${DEPS}/libtiff/libtiff/tif_fax3.c
    ${DEPS}/libtiff/libtiff/tif_fax3sm.c
    ${DEPS}/libtiff/libtiff/tif_flush.c
    ${DEPS}/libtiff/libtiff/tif_getimage.c
    ${DEPS}/libtiff/libtiff/tif_hash_set.c
    ${DEPS}/libtiff/libtiff/tif_jbig.c
    ${DEPS}/libtiff/libtiff/tif_jpeg.c
    ${DEPS}/libtiff/libtiff/tif_jpeg_12.c
    ${DEPS}/libtiff/libtiff/tif_lerc.c
    ${DEPS}/libtiff/libtiff/tif_luv.c
    ${DEPS}/libtiff/libtiff/tif_lzma.c
    ${DEPS}/libtiff/libtiff/tif_lzw.c
    ${DEPS}/libtiff/libtiff/tif_next.c
    ${DEPS}/libtiff/libtiff/tif_ojpeg.c
    ${DEPS}/libtiff/libtiff/tif_open.c
    ${DEPS}/libtiff/libtiff/tif_packbits.c
    ${DEPS}/libtiff/libtiff/tif_pixarlog.c
    ${DEPS}/libtiff/libtiff/tif_predict.c
    ${DEPS}/libtiff/libtiff/tif_print.c
    ${DEPS}/libtiff/libtiff/tif_read.c
    ${DEPS}/libtiff/libtiff/tif_strip.c
    ${DEPS}/libtiff/libtiff/tif_swab.c
    ${DEPS}/libtiff/libtiff/tif_thunder.c
    ${DEPS}/libtiff/libtiff/tif_tile.c
    ${DEPS}/libtiff/libtiff/tif_version.c
    ${DEPS}/libtiff/libtiff/tif_warning.c
    ${DEPS}/libtiff/libtiff/tif_webp.c
    ${DEPS}/libtiff/libtiff/tif_write.c
    ${DEPS}/libtiff/libtiff/tif_zip.c
    ${DEPS}/libtiff/libtiff/tif_zstd.c
    ${DEPS}/libtiff/libtiff/tif_unix.c
)

if (DEFINED EMSCRIPTEN)
	add_executable(osmStaticMapGenerator main.cpp
        shared.cpp
        tiledownloader.cpp
        mapgenerator.cpp
        ${LEPTONICA_SRC}
        ${LIBTIFF_SRC}
    )

    target_include_directories(
        osmStaticMapGenerator 
        PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR} 
        ${Stb_INCLUDE_DIR} 
        ${DEPS}/leptonica/src
        ${DEPS}/libtiff/libtiff
        ${DEPS}/libtiff/port
    )

	set_target_properties(osmStaticMapGenerator PROPERTIES COMPILE_FLAGS "-O3 -Wall -Wformat -Wno-implicit-int -s DISABLE_EXCEPTION_CATCHING=0 --use-port=libjpeg --use-port=libpng")
    set_target_properties(osmStaticMapGenerator PROPERTIES LINK_FLAGS "--no-heap-copy -O3 -lembind -lhtml5.js -lhtml5_webgl.js -lglfw.js -s ENVIRONMENT='web' --use-port=libjpeg  --use-port=libpng -s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -sFETCH -s NO_EXIT_RUNTIME=1 -s STANDALONE_WASM=0 -s EXIT_RUNTIME=0 -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=2 -s DISABLE_EXCEPTION_CATCHING=0 -s SINGLE_FILE=1")
    set_target_properties(osmStaticMapGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../ts/src/lib/wasm)
    target_link_libraries(osmStaticMapGenerator PRIVATE nlohmann_json::nlohmann_json)
endif()
