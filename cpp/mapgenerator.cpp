#include <allheaders.h>

#include "mapgenerator.h"
#include "shared.h"

#ifndef __EMSCRIPTEN__
#include "tilecache.h"
#endif

MapGenerator::MapGenerator(MapGeneratorOptions& options, mapGeneratorCallback cb) :
m_tileDescriptors(), 
m_tileRequests() {
    if (options.m_width <= 0 || options.m_height <= 0) {
        throw std::invalid_argument("Width and height must be positive values.");
    }

    if (options.m_tileSize.has_value() && options.m_tileSize.value() <= 0) {
        throw std::invalid_argument("Tile size must be a positive value.");
    }

    if (options.m_tileRequestTimeout.has_value() && options.m_tileRequestTimeout.value() <= 0) {
        throw std::invalid_argument("Tile request timeout must be a positive value.");
    }

    if (options.m_tileRequestLimit.has_value() && options.m_tileRequestLimit.value() <= 0) {
        throw std::invalid_argument("Tile request limit must be a positive value.");
    }

    m_tileCounter = 0;

    m_cb = cb;

    if (options.m_tileLayers.size() == 0) {
        TileServerConfig tileLayer;

        if (options.m_tileUrl.has_value()) {
            tileLayer.m_tileUrl = options.m_tileUrl.value();
        }

        m_tileLayers.push_back(tileLayer);
    } else {
        for (auto& tileLayer : options.m_tileLayers) {
            m_tileLayers.push_back(tileLayer);
        }
    }

    m_width = options.m_width;
    m_height = options.m_height;

    if (options.m_paddingX.has_value()) {
        m_paddingX = options.m_paddingX.value();
    }

    if (options.m_paddingY.has_value()) {
        m_paddingY = options.m_paddingY.value();
    }

    if (options.m_tileSize.has_value()) {
        m_tileSize = options.m_tileSize.value();
    }

    if (options.m_tileRequestTimeout.has_value()) {
        m_tileRequestTimeout = options.m_tileRequestTimeout.value();
    }

    if (options.m_tileRequestLimit.has_value()) {
        m_tileRequestLimit = options.m_tileRequestLimit.value();
    }

    if (options.m_reverseY.has_value()) {
        m_reverseY = options.m_reverseY.value();
    }

    if (options.m_zoomRangeMin.has_value()) {
        m_zoomRangeMin = options.m_zoomRangeMin.value();
    }

    if (options.m_zoomRangeMax.has_value()) {
        m_zoomRangeMax = options.m_zoomRangeMax.value();
    }

    if (options.m_tileRetryCount.has_value()) {
        m_tileRetryCount = options.m_tileRetryCount.value();
    }

    if (options.m_allowPartialRender.has_value()) {
        m_allowPartialRender = options.m_allowPartialRender.value();
    }

    m_tileRequestHeaders = options.m_tileRequestHeaders;

#ifndef __EMSCRIPTEN__
    if (options.m_tileCacheMaxEntries.has_value() || options.m_tileCacheTtlMs.has_value()) {
        auto& cache = TileCache::getGlobalInstance();
        cache.configure(
            options.m_tileCacheMaxEntries.value_or(256),
            options.m_tileCacheTtlMs.value_or(3600000)
        );
    }
#endif
};

void MapGenerator::Render(std::tuple<double, double> center, int zoom) {
    m_center = center;
    m_zoom = zoom;

    if (m_zoom > m_zoomRangeMax) {
        m_zoom = m_zoomRangeMax;
    }

    if (m_zoom < m_zoomRangeMin) {
        m_zoom = m_zoomRangeMin;
    }

    m_centerX = lonToX(std::get<0>(center), m_zoom);
    m_centerY = latToY(std::get<1>(center), m_zoom);

    for (auto& tileLayer : m_tileLayers) {
        DrawLayer(tileLayer);
    }
};

int MapGenerator::XToPx(int x) {
    return (int)round((x - m_centerX) * m_tileSize + (m_width / 2));
};

int MapGenerator::YToPx(int y) {
    return (int)round((y - m_centerY) * m_tileSize + (m_height / 2));
};

void MapGenerator::DrawLayer(TileServerConfig tileLayer) {
    int xMin = floor(m_centerX - (0.5 * m_width) / m_tileSize);
    int yMin = floor(m_centerY - (0.5 * m_height) / m_tileSize);
    int xMax = ceil(m_centerX + (0.5 * m_width) / m_tileSize);
    int yMax = ceil(m_centerY + (0.5 * m_height) / m_tileSize);

    int maxTile = std::pow(2, m_zoom);

    int x, y;

    for (x = xMin; x < xMax; x++) {
        int tileX = (x + maxTile) % maxTile;

        for (y = yMin; y < yMax; y++) {
            int tileY = (y + maxTile) % maxTile;

            if (m_reverseY) {
                tileY = (1 << m_zoom) - tileY - 1;
            }

            m_tileCounter++;

            TileDescriptor tile(m_tileCounter);
            tile.m_mapGeneratorPtr = this;

            tile.m_url = replaceTokens(tileLayer.m_tileUrl,
                [&](const std::string& token) -> std::optional<std::string> {
                    if (token == "z") {
                        return std::to_string(m_zoom);
                    } else if (token == "x") {
                        return std::to_string(tileX);
                    } else if (token == "y") {
                        return std::to_string(tileY);
                    } else {
                        return {};
                    }
                });

            tile.m_box = std::make_tuple<int, int, int, int>(XToPx(x), YToPx(y), XToPx(x + 1), YToPx(y + 1));

            m_tileDescriptors.emplace_back(std::make_unique<TileDescriptor>(tile));
        }
    }

    DownloadTiles();
};

void MapGenerator::DownloadTiles() {
    for (auto& tileDescriptor : m_tileDescriptors) {
        m_tileRequests[tileDescriptor->m_id] = std::optional<bool>();
    }

#ifdef __EMSCRIPTEN__
    for (auto& tileDescriptor : m_tileDescriptors) {
        downloadTile(tileDescriptor.get());
    }
#else
    auto& cache = TileCache::getGlobalInstance();

    // Serve tiles from cache where possible
    for (auto& td : m_tileDescriptors) {
        auto cached = cache.get(td->m_url);
        if (cached) {
            td->m_data = malloc(cached->size());
            if (td->m_data) {
                memcpy(td->m_data, cached->data(), cached->size());
                td->m_numBytes = static_cast<int>(cached->size());
                td->m_success.emplace(true);
                m_tileRequests[td->m_id].emplace(true);
            }
        }
    }

    // Check if all tiles were served from cache
    bool allCached = std::all_of(
        m_tileRequests.begin(), m_tileRequests.end(),
        [](const auto& req) { return req.second.has_value(); }
    );

    if (allCached) {
        DrawImage();
        return;
    }

    // Download remaining tiles (cache.put() happens inside downloadTiles
    // before MarkTileRequestFinished, while tile data is still alive)
    downloadTiles(m_tileDescriptors, m_tileRequestHeaders,
                  m_tileRequestTimeout, m_tileRequestLimit, m_tileRetryCount);
#endif
};

void MapGenerator::MarkTileRequestFinished(int id, bool successOrFailure) {
    if (!m_tileRequests.contains(id)) {
        printf("Could not find %d in map\n", id);
        return;
    }

    const std::lock_guard<std::mutex> lock(m_tileRequestsMutex);

    m_tileRequests[id].emplace(successOrFailure);

    if (!successOrFailure) {
        m_failedTileCount++;
    }

    bool allFinished = std::all_of(
        m_tileRequests.begin(), m_tileRequests.end(),
        [](const auto& req) { return req.second.has_value(); }
    );

    if (allFinished) {
        if (!m_allowPartialRender && m_failedTileCount > 0) {
            m_cb(nullptr, 0);
        } else {
            DrawImage();
        }
    }
};

bool MapGenerator::PrepareTile(TileDescriptor* tileDescriptor) {
    auto initialRawPix = pixReadMemPng((l_uint8*) tileDescriptor->m_data, tileDescriptor->m_numBytes);

    if (!initialRawPix) {
        return false;
    }

    int rawPixDepth = pixGetDepth(initialRawPix);

    if (rawPixDepth == 4) {
        tileDescriptor->m_rawPix = pixConvert8To32(pixConvert4To8(initialRawPix, TRUE));
    } else if (rawPixDepth == 8) {
        tileDescriptor->m_rawPix = pixConvert8To32(initialRawPix);
    } else {
        tileDescriptor->m_rawPix = pixClone(initialRawPix);
    }
    
    int tileWidth = pixGetWidth(tileDescriptor->m_rawPix);
    int tileHeight = pixGetHeight(tileDescriptor->m_rawPix);

    int x = std::get<0>(tileDescriptor->m_box);
    int y = std::get<1>(tileDescriptor->m_box);
    int sx = x < 0 ? 0 : x;
    int sy = y < 0 ? 0 : y;
    int dx = x < 0 ? -x : 0;
    int dy = y < 0 ? -y : 0;
    int extraWidth = x + (tileWidth - m_width);
    int extraHeight = y + (tileHeight - m_height);
    int w = tileWidth + (x < 0 ? x : 0) - (extraWidth > 0 ? extraWidth : 0);
    int h = tileHeight + (y < 0 ? y : 0) - (extraHeight > 0 ? extraHeight : 0);

    tileDescriptor->m_positionTop = sy;
    tileDescriptor->m_positionLeft = sx;

    BOX* box = boxCreate(dx, dy, w, h);
    tileDescriptor->m_clippedPix = pixClipRectangle(tileDescriptor->m_rawPix, box, nullptr);

    pixFreeData(initialRawPix);

    return true;
};

void MapGenerator::DrawImage() {
    auto texturePix = pixCreate(m_width, m_height, 32);

    for (auto& tileDescriptor : m_tileDescriptors) {
        if (PrepareTile(tileDescriptor.get())) {
            pixRasterop(
                texturePix, 
                tileDescriptor->m_positionLeft, 
                tileDescriptor->m_positionTop, 
                pixGetWidth(tileDescriptor->m_clippedPix), 
                pixGetHeight(tileDescriptor->m_clippedPix), 
                PIX_SRC,
                tileDescriptor->m_clippedPix, 
                0, 
                0
            );
        } else {
            // TODO: log?
        }
    }

    l_uint8* data;
    size_t size;

    pixWriteMem(&data, &size, texturePix, IFF_PNG);
    
    m_cb(data, size);

    free(data);
    pixFreeData(texturePix);

    for (auto& tileDescriptor : m_tileDescriptors) {
        tileDescriptor->FreeResources();
    }
};
