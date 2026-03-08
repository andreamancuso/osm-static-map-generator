# osm-static-map-generator

Native Node.js addon for generating static map images from OpenStreetMap raster tiles. Renders a map centered on given coordinates at a specified zoom level, compositing tiles into a single PNG image.

Prebuilt binaries are included for linux-x64, linux-arm64, darwin-arm64, and win32-x64.

## Install

```bash
npm install osm-static-map-generator
```

## Usage

```js
const { generateMap } = require('osm-static-map-generator');
const fs = require('fs');

const result = await generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 }, // longitude, latitude
  zoom: 13,
  tileRequestHeaders: {
    "User-Agent": "my-app/1.0"
  }
});

fs.writeFileSync('map.png', result.buffer);
console.log(`Generated ${result.buffer.length} bytes, ${result.failedTileCount} failed tiles`);
```

## Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | number | *required* | Image width in pixels |
| `height` | number | *required* | Image height in pixels |
| `center` | object | *required* | `{ x: longitude, y: latitude }` |
| `zoom` | number | *required* | Zoom level |
| `tileUrl` | string | OSM default | Tile server URL template with `{x}`, `{y}`, `{z}` tokens |
| `tileSize` | number | 256 | Tile size in pixels |
| `paddingX` | number | 0 | Horizontal padding in pixels |
| `paddingY` | number | 0 | Vertical padding in pixels |
| `tileRequestTimeout` | number | — | Timeout per tile request in ms |
| `tileRequestLimit` | number | — | Max concurrent tile requests |
| `tileRetryCount` | number | 3 | Max retries per failed tile |
| `tileRequestHeaders` | object | — | Custom HTTP headers for tile requests |
| `reverseY` | boolean | false | Reverse Y axis (TMS tile scheme) |
| `zoomRange` | object | — | `{ min, max }` zoom constraints |
| `allowPartialRender` | boolean | true | If false, rejects when any tile fails |

## Result

The returned promise resolves with:

- `buffer` — `Buffer` containing the PNG image
- `failedTileCount` — number of tiles that failed to download

## Attribution

Please follow the [OpenStreetMap attribution guidelines](https://osmfoundation.org/wiki/Licence/Attribution_Guidelines) when using default tile servers.

## License

MIT
