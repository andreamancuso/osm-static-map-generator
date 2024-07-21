#include <optional>
#include <string>
#include <tuple>

#include <emscripten/fetch.h>

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
    std::string m_url;
    std::tuple<int, int, int, int> m_box;
    MapGenerator* m_mapGeneratorPtr;
    std::optional<bool> m_success;
    const char* m_data;
    int m_numBytes;
    unsigned short m_status;

    TileDescriptor(int id);

    void HandleSuccess(emscripten_fetch_t *fetch);
    void HandleFailure(emscripten_fetch_t *fetch);
};
