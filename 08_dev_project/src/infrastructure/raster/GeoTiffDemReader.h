#pragma once

// GeoTiffDemReader.h declares the Infrastructure adapter for DEM GeoTIFF files.
// Upstream: Application depends on the Domain IDemReader port.
// Downstream: GeoTiffDemReader reads a single-band DEM into TerrainRaster.

#include <string>

#include "domain/ports/IDemReader.h"
#include "domain/terrain/TerrainRaster.h"

namespace gis::infrastructure {

// GeoTiffDemReader implements IDemReader using the installed raster library.
// Upstream: LoadTerrainUseCase invokes it through IDemReader.
// Downstream: TerrainRaster is returned without leaking library-specific types.
class GeoTiffDemReader final : public gis::domain::IDemReader {
public:
    // read opens a DEM GeoTIFF and reads band 1 as floating-point elevations.
    // Upstream: Application passes a DEM file path through IDemReader.
    // Downstream: TerrainMeshBuilder later consumes the returned TerrainRaster.
    gis::domain::TerrainRaster read(const std::string& demPath) override;
};

} // namespace gis::infrastructure
