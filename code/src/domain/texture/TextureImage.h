#pragma once

// TextureImage.h defines API-independent texture image data.
// Upstream: GeoTIFF image readers create TextureImage.
// Downstream: TerrainMesh, SceneNode, and renderers consume TextureImage.

#include <cstdint>
#include <vector>

namespace gis::domain {

// TextureImage stores RGB or grayscale pixels without binding to any graphics API.
// Upstream: image reading infrastructure.
// Downstream: terrain loading, scene data, and rendering adapters.
struct TextureImage {
    int width = 0;                    // width is the image column count.
    int height = 0;                   // height is the image row count.
    int channels = 0;                 // channels is the number of bytes per pixel.
    double originX = 0.0;             // originX is the horizontal raster origin when available.
    double originY = 0.0;             // originY is the vertical raster origin when available.
    double pixelSizeX = 1.0;          // pixelSizeX is the horizontal cell size when available.
    double pixelSizeY = 1.0;          // pixelSizeY is the vertical cell size when available.
    bool hasGeoTransform = false;     // hasGeoTransform is true when georeference metadata exists.
    std::vector<std::uint8_t> pixels; // pixels are interleaved and stored in row-major order.
};

} // namespace gis::domain
