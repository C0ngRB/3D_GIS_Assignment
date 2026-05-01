#pragma once

// TerrainRaster.h defines DEM raster elevation data.
// Upstream: DEM GeoTIFF readers create TerrainRaster.
// Downstream: TerrainMeshBuilder reads elevations and creates triangles.

#include <vector>

namespace gis::domain {

// TerrainRaster stores DEM dimensions, sampling metadata, and elevation values.
// Upstream: IDemReader implementations.
// Downstream: terrain mesh construction and texture coordinate calculation.
struct TerrainRaster {
    int width = 0;                 // width is the DEM column count.
    int height = 0;                // height is the DEM row count.
    double originX = 0.0;          // originX is the horizontal raster origin.
    double originY = 0.0;          // originY is the vertical raster origin.
    double pixelSizeX = 1.0;       // pixelSizeX is the horizontal cell size.
    double pixelSizeY = 1.0;       // pixelSizeY is the vertical cell size.
    bool hasGeoTransform = false;  // hasGeoTransform is true when origin and pixel sizes came from the file.
    double noDataValue = -9999.0;  // noDataValue marks invalid elevations.
    bool hasNoDataValue = false;   // hasNoDataValue is true when the source file defines a no-data value.
    std::vector<float> elevations; // elevations are stored in row-major order.
};

} // namespace gis::domain
