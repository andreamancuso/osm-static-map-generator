#include "mapgenerator.h"
#include "shared.h"

MapGenerator::MapGenerator(MapGeneratorOptions& options) {
    m_tileCounter = 0;

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

    m_tileRequestHeaders = options.m_tileRequestHeaders;
};

void MapGenerator::Render(std::tuple<double, double> center, int zoom) {
    m_center = center;
    m_zoom = zoom;

    if (m_zoom > m_zoomRangeMax) {
        m_zoom = m_zoomRangeMax;
    }

    m_centerX = lonToX(std::get<0>(center), m_zoom);
    m_centerY = latToY(std::get<1>(center), m_zoom);

    m_image = std::make_unique<Image>(m_width, m_height);

    for (auto& tileLayer : m_tileLayers) {
        DrawLayer(tileLayer);
    }
};

int MapGenerator::XToPx(int x) {
    return (int)round((x - m_centerX) * m_tileSize + m_width / 2);
};

int MapGenerator::YToPx(int y) {
    return (int)round((y - m_centerY) * m_tileSize + m_height / 2);
};

void MapGenerator::DrawLayer(TileServerConfig tileLayer) {
    int xMin = floor(m_centerX - (0.5 * m_width) / m_tileSize);
    int yMin = floor(m_centerY - (0.5 * m_height) / m_tileSize);
    int xMax = floor(m_centerX + (0.5 * m_width) / m_tileSize);
    int yMax = floor(m_centerY + (0.5 * m_height) / m_tileSize);

    for (int x = xMin; x < xMax; x++) {
        for (int y = yMin; y < yMax; y++) {
            int maxTile = std::pow(2, m_zoom);
            int tileX = (x + maxTile) % maxTile;
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

            m_tileDescriptors.emplace_back(tile);
        }
    }

    GetTiles();
};

void MapGenerator::GetTiles() {
    for (auto& tileDescriptor : m_tileDescriptors) {
        m_tileRequests[tileDescriptor.m_id] = std::optional<bool>();
    }

    for (auto& tileDescriptor : m_tileDescriptors) {
        download(tileDescriptor);
    }
};

void MapGenerator::MarkTileRequestFinished(int id, bool successOrFailure) {
    if (m_tileRequests.contains(id)) {
        const std::lock_guard<std::mutex> lock(m_tileRequestsMutex);
        
        m_tileRequests[id].emplace(successOrFailure);

        int successfullyFinishedRequests = 0;
        for (auto& [key, item] : m_tileRequests) {
            if (item.has_value() && item.value() == true) {
                successfullyFinishedRequests++;
            }
        }

        if (m_tileRequests.size() == successfullyFinishedRequests) {
            printf("All done\n");
        } else {
            printf("Still %d to go\n", (m_tileRequests.size() - successfullyFinishedRequests));
        }
    } else {
        printf("Could not find %d in map\n", id);
    }
};

