# osm-static-map-generator-wasm

WebAssembly build of osm-static-map-generator for browser use. Generates static map images from OpenStreetMap raster tiles entirely client-side.

## Install

```bash
npm install osm-static-map-generator-wasm
```

## Usage

```js
import { createMapGenerator } from 'osm-static-map-generator-wasm';

const { generateMap } = await createMapGenerator();

const result = await generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13
});

// result.buffer — Uint8Array containing the rendered PNG
// result.failedTileCount — number of tiles that failed to download

const blob = new Blob([result.buffer], { type: 'image/png' });
document.getElementById('map').src = URL.createObjectURL(blob);
```

## Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | `number` | *required* | Image width in pixels |
| `height` | `number` | *required* | Image height in pixels |
| `center` | `{x, y}` | *required* | Map center (longitude, latitude) |
| `zoom` | `number` | *required* | Zoom level |
| `tileUrl` | `string` | OSM default | Tile server URL template with `{x}`, `{y}`, `{z}` tokens |
| `tileSize` | `number` | `256` | Tile size in pixels |
| `paddingX` | `number` | `0` | Horizontal padding in pixels |
| `paddingY` | `number` | `0` | Vertical padding in pixels |
| `tileRequestTimeout` | `number` | `0` | Timeout per tile request in seconds |
| `tileRequestLimit` | `number` | `2` | Max concurrent tile requests |
| `reverseY` | `boolean` | `false` | Reverse Y axis (TMS tile scheme) |
| `tileRequestHeaders` | `object` | `{}` | Custom HTTP headers for tile requests |
| `zoomRange` | `{min, max}` | `{1, 17}` | Zoom range constraints |
| `tileRetryCount` | `number` | `3` | Max retries per failed tile |
| `allowPartialRender` | `boolean` | `true` | Render even if some tiles fail |

## Advanced: Raw Module API

For full control over the WASM module, you can use `createModule` directly:

```js
import { createModule } from 'osm-static-map-generator-wasm';

const instance = await createModule({
  eventHandlers: {
    onMapGeneratorJobDone: (jobId, dataPtr, numBytes, failedTileCount) => {
      const data = new Uint8Array(instance.HEAPU8.buffer, dataPtr, numBytes);
      const png = new Uint8Array(data); // copy before WASM memory is freed
      // ...
    },
    onMapGeneratorJobError: (jobId, errorMessage) => {
      console.error(`Job ${jobId} failed:`, errorMessage);
    }
  }
});

const jobId = instance.generateMap(JSON.stringify({ /* options */ }));
```

## Attribution

Please follow the [OpenStreetMap attribution guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines) when using default tile servers.

## License

MIT
