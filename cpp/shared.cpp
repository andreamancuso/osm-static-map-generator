#include <string>

#define _USE_MATH_DEFINES
#include <math.h>

#include <allheaders.h>
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

TileDescriptor::~TileDescriptor() {
    // not entirely sure that's how it's done
    if (m_data != nullptr) {
        free((void*)m_data);
    }

    if (m_slicedTileData != nullptr) {
        free((void*)m_slicedTileData);
    }

    if (m_slicedTileData != nullptr) {
        printf("About to free clipped PIX*\n");
        pixFreeData(m_clippedPix);
    }

    if (m_slicedTileData != nullptr) {
        printf("About to free raw PIX*\n");
        pixFreeData(m_rawPix);
    }
}

void TileDescriptor::HandleSuccess(emscripten_fetch_t *fetch) {
    m_success.emplace(true);
    m_numBytes = fetch->numBytes;
    m_status = fetch->status;
    
    m_data = malloc(fetch->numBytes);
    memcpy((void*)m_data, fetch->data, fetch->numBytes);

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, true);
};

void TileDescriptor::HandleFailure(emscripten_fetch_t *fetch) {
    m_success.emplace(false);
    m_status = fetch->status;

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, false);
};
