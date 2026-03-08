import createModule from './osmStaticMapGenerator.mjs';

/**
 * Create a map generator instance with a promise-based API.
 *
 * @returns {Promise<{generateMap: function}>} Object with generateMap method
 */
export async function createMapGenerator() {
    const pendingJobs = new Map();

    const instance = await createModule({
        eventHandlers: {
            onMapGeneratorJobDone: (jobId, dataPtr, numBytes, failedTileCount) => {
                const job = pendingJobs.get(jobId);
                if (!job) return;
                pendingJobs.delete(jobId);

                const data = new Uint8Array(instance.HEAPU8.buffer, dataPtr, numBytes);
                const buffer = new Uint8Array(data); // copy before WASM frees

                job.resolve({ buffer, failedTileCount });
            },
            onMapGeneratorJobError: (jobId, errorMessage) => {
                const job = pendingJobs.get(jobId);
                if (!job) return;
                pendingJobs.delete(jobId);
                job.reject(new Error(errorMessage));
            }
        }
    });

    /**
     * Generate a static map image from OpenStreetMap tiles.
     *
     * @param {object} options - Map generation options
     * @param {number} options.width - Image width in pixels
     * @param {number} options.height - Image height in pixels
     * @param {object} options.center - Map center coordinates
     * @param {number} options.center.x - Longitude
     * @param {number} options.center.y - Latitude
     * @param {number} options.zoom - Zoom level
     * @param {string} [options.tileUrl] - Tile server URL template with {x}, {y}, {z} tokens
     * @param {number} [options.paddingX] - Horizontal padding in pixels
     * @param {number} [options.paddingY] - Vertical padding in pixels
     * @param {number} [options.tileSize] - Tile size in pixels (default: 256)
     * @param {number} [options.tileRequestTimeout] - Timeout per tile request in ms
     * @param {number} [options.tileRequestLimit] - Max concurrent tile requests
     * @param {boolean} [options.reverseY] - Reverse Y axis (TMS tile scheme)
     * @param {object} [options.tileRequestHeaders] - Custom HTTP headers for tile requests
     * @param {object} [options.zoomRange] - Zoom range constraints
     * @param {number} [options.zoomRange.min] - Minimum zoom level
     * @param {number} [options.zoomRange.max] - Maximum zoom level
     * @param {number} [options.tileRetryCount] - Max retries per failed tile (default: 3)
     * @param {boolean} [options.allowPartialRender] - If true (default), render even if some tiles fail
     * @returns {Promise<{buffer: Uint8Array, failedTileCount: number}>} Result with PNG data and count of failed tiles
     */
    async function generateMap(options) {
        if (!options || typeof options !== 'object') {
            throw new TypeError('options must be an object');
        }
        if (typeof options.width !== 'number' || typeof options.height !== 'number') {
            throw new TypeError('options.width and options.height must be numbers');
        }
        if (!options.center || typeof options.center.x !== 'number' || typeof options.center.y !== 'number') {
            throw new TypeError('options.center must be an object with numeric x and y properties');
        }
        if (typeof options.zoom !== 'number') {
            throw new TypeError('options.zoom must be a number');
        }

        const jobId = instance.generateMap(JSON.stringify(options));
        if (jobId < 0) {
            throw new Error('Failed to start map generation');
        }

        return new Promise((resolve, reject) => {
            pendingJobs.set(jobId, { resolve, reject });
        });
    }

    return { generateMap };
}

export { createModule };
