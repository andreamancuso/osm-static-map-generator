#include <string>

#define _USE_MATH_DEFINES
#include <math.h>

#include <emscripten/fetch.h>

#include "shared.h"
#include "mapgenerator.h"

/**
 * Transform longitude to tile number
 */
double lonToX(double lon, int zoom) {
    return ((lon + 180) / 360) * (std::pow(2, zoom));
}

/**
 *  Transform latitude to tile number
 */
double latToY(double lat, int zoom) {
    return (1 - log(tan(lat * M_PI / 180) + 1
        / cos(lat * M_PI / 180)) / M_PI) / 2 * (std::pow(2, zoom));
}

double yToLat(double y, int zoom) {
    return atan(sinh(M_PI * (1 - 2 * y / (std::pow(2, zoom))))) / M_PI * 180;
}

double xToLon(double x, int zoom) {
    return x / (std::pow(2, zoom)) * 360 - 180;
}

double meterToPixel(double meter, int zoom, double lat) {
    double latitudeRadians = lat * (M_PI / 180);
    double meterProPixel = (156543.03392 * cos(latitudeRadians)) / std::pow(2, zoom);
    return meter / meterProPixel;
}

TileDescriptor::TileDescriptor(int id) {
    m_id = id;
};

void TileDescriptor::HandleSuccess(emscripten_fetch_t *fetch) {
    // printf("HandleSuccess a\n");
    m_success.emplace(true);
    // printf("HandleSuccess b\n");
    m_numBytes = fetch->numBytes;
    // printf("HandleSuccess c\n");
    m_status = fetch->status;
    printf("HandleSuccess %d %s\n", m_id, fetch->url);

    // memcpy((void*)m_data, fetch->data, fetch->numBytes);

    // m_mapGeneratorPtr->m_tileRequests[m_url].emplace(true);

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, true);
    // printf("HandleSuccess e\n");
};

void TileDescriptor::HandleFailure(emscripten_fetch_t *fetch) {
    m_success.emplace(false);
    m_status = fetch->status;

    // m_mapGeneratorPtr->m_tileRequests[m_url].emplace(false);

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, false);
};
