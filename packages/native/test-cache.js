const { generateMap } = require('./');

const options = {
  width: 800,
  height: 600,
  center: { x: -0.1276, y: 51.5074 },
  zoom: 13,
  tileRequestHeaders: {
    "User-Agent": "osm-static-map-generator/0.1.0 (cache test)"
  }
};

async function run() {
  // First call — downloads all tiles, populates cache
  const start1 = Date.now();
  const result1 = await generateMap(options);
  const elapsed1 = Date.now() - start1;
  console.log(`First call:  ${result1.buffer.length} bytes in ${elapsed1}ms`);

  // Second call — should be served from cache
  const start2 = Date.now();
  const result2 = await generateMap(options);
  const elapsed2 = Date.now() - start2;
  console.log(`Second call: ${result2.buffer.length} bytes in ${elapsed2}ms`);

  // Verify cache hit is significantly faster
  const speedup = elapsed1 / elapsed2;
  console.log(`Speedup: ${speedup.toFixed(1)}x`);

  if (elapsed2 >= elapsed1) {
    console.error('FAIL: second call was not faster than first');
    process.exit(1);
  }

  if (result1.buffer.length !== result2.buffer.length) {
    console.error('FAIL: output sizes differ');
    process.exit(1);
  }

  console.log('PASS: cache is working');
}

run().catch(err => {
  console.error('Failed:', err);
  process.exit(1);
});
