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

add_definitions(-std=c++23)
set(CMAKE_CXX_STANDARD 23)

find_package(Stb REQUIRED)
find_package(nlohmann_json REQUIRED)

find_path(LIBCORO_INCLUDE_DIRS "coro/coro.hpp")

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CMAKE_EXECUTABLE_SUFFIX ".mjs")

if (DEFINED EMSCRIPTEN)
	add_executable(osmStaticMapGenerator main.cpp
        shared.cpp
        tiledownloader.cpp
        image.cpp
        mapgenerator.cpp
    )

    target_include_directories(osmStaticMapGenerator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Stb_INCLUDE_DIR} ${LIBCORO_INCLUDE_DIRS} ${DEPS}/leptonica/src)

	set_target_properties(osmStaticMapGenerator PROPERTIES COMPILE_FLAGS "-O0 -Wall -Wformat -s DISABLE_EXCEPTION_CATCHING=0")
    set_target_properties(osmStaticMapGenerator PROPERTIES LINK_FLAGS "--no-heap-copy -O0 -lembind -lhtml5.js -lhtml5_webgl.js -lglfw.js -s ENVIRONMENT='web' -s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -sFETCH -s NO_EXIT_RUNTIME=1 -s STANDALONE_WASM=0 -s EXIT_RUNTIME=0 -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=2 -s DISABLE_EXCEPTION_CATCHING=0 -s SINGLE_FILE=1 ${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed/wasm32-emscripten/lib/libcoro.a")
    set_target_properties(osmStaticMapGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../ts/src/lib/wasm)
    target_link_libraries(osmStaticMapGenerator PRIVATE nlohmann_json::nlohmann_json)
endif()
