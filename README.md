# osm-static-map-generator

**IMPORTANT: If you plan to use this library then please remember to follow [OpenStreetMap's Licence/Attribution Guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines).**

## What is it?

A small C++ library for WebAssembly which generates a map using OpenStreetMap's raster tiles.

## Why did you write it?

I have been working on React bindings for Dear Imgui and I need a C++ map component. This library will enable me to build one. That said, I figured someone else might find it useful.

## Which dependencies have you used?

- [leptonica](https://github.com/DanBloomberg/leptonica)
- [libpng](https://github.com/emscripten-ports/libpng)
- [libtiff](https://gitlab.com/libtiff/libtiff)
- [libjpeg](https://emscripten.org/docs/tools_reference/settings_reference.html#use-libjpeg)

## Why did you use leptonica for image manipulation?

Based on my research, it is battle-tested and is the only library I got to work in WebAssembly without getting too many headaches.

## What inspired you when writing the library?

I came across: https://github.com/StephanGeorg/staticmaps , a brilliant library for Node.js. I ported it to the browser, replacing [Sharp](https://github.com/lovell/sharp) with [Jimp](https://github.com/jimp-dev/jimp).

I then decided to port it to WASM.

## What's next in the roadmap?

- proper error handling/recovery
- tests
- throttling
- multithreading support (once I figure out how to tame pthreads in emscripten)
- coroutines (once I figure out how to make them work)
