#pragma once

// IDemReader.h defines the DEM-reading port.
// Upstream: LoadTerrainUseCase depends on this interface for elevations.
// Downstream: GeoTiffDemReader implements it in the infrastructure layer later.

#include <string>

#include "domain/terrain/TerrainRaster.h"

namespace gis::domain {

// IDemReader separates DEM file access from terrain business flow.
// Upstream: Application asks for DEM raster data.
// Downstream: Infrastructure returns TerrainRaster.
class IDemReader {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: Application owns or references the port abstraction.
    // Downstream: concrete DEM readers release their own resources.
    virtual ~IDemReader() = default;

    // read loads DEM raster data from a path.
    // Upstream: UI-selected demPath is passed by LoadTerrainUseCase.
    // Downstream: GeoTiffDemReader produces TerrainRaster.
    virtual TerrainRaster read(const std::string& demPath) = 0;
};

} // namespace gis::domain
