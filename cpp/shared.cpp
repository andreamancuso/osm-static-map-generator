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
    double meterPerPixel = (156543.03392 * cos(latitudeRadians)) / std::pow(2, zoom);
    return meter / meterPerPixel;
}

TileDescriptor::TileDescriptor(int id) : m_id(id) {};

void TileDescriptor::FreeResources() {
    if (m_data) {
        free((void*)m_data);
    }

    if (m_clippedPix) {
        pixFreeData(m_clippedPix);
    }

    if (m_rawPix) {
        pixFreeData(m_rawPix);
    }
};

void TileDescriptor::HandleSuccess(emscripten_fetch_t *fetch) {
    m_success.emplace(true);
    m_numBytes = fetch->numBytes;
    m_status = fetch->status;
    
    m_data = malloc(fetch->numBytes);
    if (m_data == nullptr) {
        // Handle memory allocation failure
        m_mapGeneratorPtr->MarkTileRequestFinished(m_id, false);
        return;
    }
    memcpy((void*)m_data, fetch->data, fetch->numBytes);

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, true);
};

void TileDescriptor::HandleFailure(emscripten_fetch_t *fetch) {
    m_success.emplace(false);
    m_status = fetch->status;

    m_mapGeneratorPtr->MarkTileRequestFinished(m_id, false);
};
