const { generateMap } = require('./');

console.log('Test 1: allowPartialRender: false with invalid tiles should reject');

generateMap({
  width: 256,
  height: 256,
  center: { x: 0, y: 0 },
  zoom: 1,
  tileUrl: 'https://invalid.example.test/{z}/{x}/{y}.png',
  allowPartialRender: false,
  tileRetryCount: 0,
  tileRequestTimeout: 3
}).then(r => {
  console.error('FAIL: Expected rejection but got resolve with failedTileCount:', r.failedTileCount);
  process.exit(1);
}).catch(err => {
  console.log('PASS: Correctly rejected:', err.message);
});
