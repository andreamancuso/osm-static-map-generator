#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <allheaders.h>

#include "image.h"

Image::Image(int width, int height) {
    m_width = width;
    m_height = height;
};

void Image::PrepareTileParts(void* data, int dataLength) {
    int tileWidth;
    int tileHeight;

    auto tileRawData = stbi_load_from_memory((stbi_uc const *)data, dataLength, &tileWidth, &tileHeight, NULL, 4);
    auto rawPix = pixCreate(tileWidth, tileHeight, 32);
    pixSetData(rawPix, (unsigned int*)tileRawData);

    // TODO
};
