#pragma once

// LoadTerrainUseCase.h defines the Application use case for DEM and image loading.
// Upstream: UI provides DEM and remote-sensing image paths.
// Downstream: the use case calls reader ports and returns Domain raster data.

#include <string>
#include <vector>

#include "domain/ports/IDemReader.h"
#include "domain/ports/IImageReader.h"
#include "domain/terrain/TerrainRaster.h"
#include "domain/texture/TextureImage.h"

namespace gis::application {

// LoadTerrainRequest carries the two files needed before terrain mesh construction.
// Upstream: UI creates it after the user selects a DEM and an image file.
// Downstream: LoadTerrainUseCase validates paths and invokes reader ports.
struct LoadTerrainRequest {
    std::string demPath;   // demPath is the GeoTIFF DEM file path.
    std::string imagePath; // imagePath is the GeoTIFF RGB image file path.
};

// LoadTerrainResult wraps DEM data, image data, errors, and non-fatal warnings.
// Upstream: LoadTerrainUseCase fills it after reading both files.
// Downstream: later mesh-building use cases consume dem and image when success is true.
struct LoadTerrainResult {
    bool success = false;                    // success is true when both DEM and image are usable.
    std::string errorMessage;                // errorMessage explains fatal loading or validation failures.
    std::vector<std::string> warnings;       // warnings report non-fatal metadata or extent issues.
    gis::domain::TerrainRaster dem;          // dem contains elevation raster data when success is true.
    gis::domain::TextureImage image;         // image contains texture pixels when success is true.
};

// LoadTerrainUseCase orchestrates DEM and image loading.
// Upstream: UI passes selected file paths.
// Downstream: IDemReader and IImageReader perform file-format reading in Infrastructure.
class LoadTerrainUseCase {
public:
    // The constructor receives both raster reader ports.
    // Upstream: composition code wires infrastructure readers into the use case.
    // Downstream: execute calls the ports without knowing the concrete GeoTIFF API.
    LoadTerrainUseCase(gis::domain::IDemReader& demReader, gis::domain::IImageReader& imageReader);

    // execute reads, validates, and lightly compares DEM and image metadata.
    // Upstream: UI or tests provide LoadTerrainRequest.
    // Downstream: callers receive LoadTerrainResult instead of exceptions.
    LoadTerrainResult execute(const LoadTerrainRequest& request) const;

private:
    gis::domain::IDemReader& demReader_;     // demReader_ is the Domain port for DEM files.
    gis::domain::IImageReader& imageReader_; // imageReader_ is the Domain port for image files.
};

} // namespace gis::application
