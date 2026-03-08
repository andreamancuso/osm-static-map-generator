const { generateMap } = require('./');
const fs = require('fs');

const start = Date.now();

generateMap({
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13,
  tileRequestHeaders: {
    "User-Agent": "osm-static-map-generator/0.1.0 (smoke test)"
  }
}).then(buf => {
  const elapsed = Date.now() - start;
  fs.writeFileSync('test-output.png', buf);
  console.log(`OK: ${buf.length} bytes in ${elapsed}ms`);
}).catch(err => {
  console.error('Failed:', err);
  process.exit(1);
});
