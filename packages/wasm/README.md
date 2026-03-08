# osm-static-map-generator-wasm

WebAssembly build of osm-static-map-generator for browser use. Generates static map images from OpenStreetMap raster tiles entirely client-side.

## Install

```bash
npm install osm-static-map-generator-wasm
```

## Usage

```js
import createModule from 'osm-static-map-generator-wasm/lib/osmStaticMapGenerator.mjs';

const instance = await createModule({
  eventHandlers: {
    onMapGeneratorJobDone: (dataPtr, numBytes) => {
      const data = new Uint8Array(instance.HEAPU8.buffer, dataPtr, numBytes);
      const png = new Uint8Array(data); // copy before WASM memory is freed

      const blob = new Blob([png], { type: 'image/png' });
      document.getElementById('map').src = URL.createObjectURL(blob);
    },
    onMapGeneratorJobError: (errorMessage) => {
      console.error('Map generation failed:', errorMessage);
    }
  }
});

instance.generateMap(JSON.stringify({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13
}));
```

## How it works

The module uses Emscripten's Fetch API to download map tiles directly from the browser, composites them using Leptonica, and returns the result as a PNG via the `onMapGeneratorJobDone` callback.

`generateMap()` returns a job ID (negative on error). Rendering is asynchronous — results arrive via the event handlers.

## Attribution

Please follow the [OpenStreetMap attribution guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines) when using default tile servers.

## License

MIT
