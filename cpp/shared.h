#include <optional>
#include <string>
#include <tuple>

#include <allheaders.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#include <curl/curl.h>
#endif

#pragma once

class MapGenerator;

const static std::string tokenReservedChars = "{}";

// Replaces {tokens} in a string by calling the lookup function.
template <typename Lookup>
std::string replaceTokens(const std::string &source, const Lookup &lookup) {
    std::string result;
    result.reserve(source.size());

    auto pos = source.begin();
    const auto end = source.end();

    while (pos != end) {
        auto brace = std::find(pos, end, '{');
        result.append(pos, brace);
        pos = brace;
        if (pos != end) {
            for (brace++; brace != end && tokenReservedChars.find(*brace) == std::string::npos; brace++);
            if (brace != end && *brace == '}') {
                std::string key{pos + 1, brace};
                if (std::optional<std::string> replacement = lookup(key)) {
                    result.append(*replacement);
                } else {
                    result.append("{");
                    result.append(key);
                    result.append("}");
                }
                pos = brace + 1;
            } else {
                result.append(pos, brace);
                pos = brace;
            }
        }
    }

    return result;
}

/**
 * Transform longitude to tile number
 */
double lonToX(double lon, int zoom);

/**
 *  Transform latitude to tile number
 */
double latToY(double lat, int zoom);

double yToLat(double y, int zoom);

double xToLon(double x, int zoom);

double meterToPixel(double meter, int zoom, double lat);

struct TileDescriptor {
    int m_id;
    std::tuple<int, int, int, int> m_box;
    MapGenerator* m_mapGeneratorPtr;

    std::string m_url;
    std::optional<bool> m_success;
    unsigned short m_status;

    void* m_data;
    int m_numBytes;
    PIX* m_rawPix;

    PIX* m_clippedPix;
    int m_positionTop;
    int m_positionLeft;

    TileDescriptor(int id);

#ifdef __EMSCRIPTEN__
    void HandleSuccess(emscripten_fetch_t *fetch);
    void HandleFailure(emscripten_fetch_t *fetch);
#else
    void HandleSuccess(void *buffer, size_t sz, size_t n);
    void HandleFailure();
#endif

    void FreeResources();
};
