#include <exception>
#include <mutex>
#include <stdexcept>
#include <optional>
#include <tuple>
#include <string>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

#include <emscripten/fetch.h>

#include <nlohmann/json.hpp>

#include "tiledownloader.h"
#include "shared.h"
#include "image.h"

#pragma once

using json = nlohmann::json;

struct TileDescriptor;

struct TileServerConfig {
    std::string m_tileUrl;

    TileServerConfig() {
        m_tileUrl = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    }

    TileServerConfig(std::string url) {
        m_tileUrl = url;
    }
};

struct MapGeneratorOptions {
    int m_width;
    int m_height;
    std::optional<bool> m_reverseY;
    std::optional<int> m_paddingX;
    std::optional<int> m_paddingY;
    std::optional<int> m_tileSize;
    std::optional<int> m_tileRequestTimeout;
    std::optional<int> m_tileRequestLimit;
    std::optional<int> m_zoomRangeMin;
    std::optional<int> m_zoomRangeMax;
    std::unordered_map<std::string, std::string> m_tileRequestHeaders;
    std::optional<std::string> m_tileUrl;
    std::vector<TileServerConfig> m_tileLayers;

    MapGeneratorOptions(const json& options) {
        if (options.is_object()) {
            if (options.contains("tileUrl") && options["tileUrl"].is_string()) {
                m_tileUrl = options["tileUrl"].template get<std::string>();
            }

            if (options.contains("tileLayers") && options["tileLayers"].is_array()) {
                for (auto& [key, item] : options["tileLayers"].items()) {
                    if (item.is_object()) {
                        TileServerConfig tileServerConfig;
                        tileServerConfig.m_tileUrl = item.template get<std::string>();
                    }
                }
            }

            m_width = options["width"].template get<int>();
            m_height = options["height"].template get<int>();

            if (options.contains("paddingX") && options["paddingX"].is_number_unsigned()) {
                m_paddingX.emplace(options["paddingX"].template get<int>());
            }

            if (options.contains("paddingY") && options["paddingY"].is_number_unsigned()) {
                m_paddingY.emplace(options["paddingY"].template get<int>());
            }

            if (options.contains("tileSize") && options["tileSize"].is_number_unsigned()) {
                m_tileSize.emplace(options["tileSize"].template get<int>());
            }

            if (options.contains("tileRequestTimeout") && options["tileRequestTimeout"].is_number_unsigned()) {
                m_tileRequestTimeout.emplace(options["tileRequestTimeout"].template get<int>());
            }

            if (options.contains("tileRequestLimit") && options["tileRequestLimit"].is_number_unsigned()) {
                m_tileRequestLimit.emplace(options["tileRequestLimit"].template get<int>());
            }

            if (options.contains("reverseY") && options["reverseY"].is_boolean()) {
                m_reverseY.emplace(options["reverseY"].template get<bool>());
            }

            if (options.contains("tileRequestHeaders") && options["tileRequestHeaders"].is_object()) {
                for (auto& [key, item] : options["tileRequestHeaders"].items()) {
                    m_tileRequestHeaders[key] = item.template get<std::string>();
                }
            }

            if (options.contains("zoomRange") && options["zoomRange"].is_object()) {
                m_zoomRangeMin.emplace(options["zoomRange"]["min"].template get<int>());
                m_zoomRangeMax.emplace(options["zoomRange"]["max"].template get<int>());
            }
        }
    }
};

class MapGenerator {
private:
    std::mutex m_tileRequestsMutex;

    int m_tileCounter;

    std::vector<TileDescriptor> m_tileDescriptors;

    std::vector<TileServerConfig> m_tileLayers;
    int m_width;
    int m_height;
    int m_paddingX = 0;
    int m_paddingY = 0;
    int m_tileSize = 256;
    int m_tileRequestTimeout;
    int m_tileRequestLimit = 2;
    std::unordered_map<std::string, std::string> m_tileRequestHeaders;
    bool m_reverseY = false;
    int m_zoomRangeMin = 1;
    int m_zoomRangeMax = 17;
    std::tuple<double, double> m_center;
    double m_centerX = 0;
    double m_centerY = 0;
    int m_zoom = 0;
    std::unique_ptr<Image> m_image;

public:
    std::unordered_map<int, std::optional<bool>> m_tileRequests;

    MapGenerator(MapGeneratorOptions& options);

    void Render(std::tuple<double, double> center, int zoom);

    int XToPx(int x);

    int YToPx(int y);

    void DrawLayer(TileServerConfig tileLayer);

    void GetTiles();

    void MarkTileRequestFinished(int id, bool successOrFailure);

    // void StitchTilesTogether() {

    // }
};
