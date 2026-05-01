#pragma once

// BuildTerrainMeshUseCase.h defines the Application use case for terrain mesh construction.
// Upstream: LoadTerrainUseCase provides DEM and image data.
// Downstream: TerrainMeshBuilder creates a TerrainMesh for scene insertion.

#include <string>

#include "domain/terrain/TerrainMesh.h"
#include "domain/terrain/TerrainMeshBuilder.h"
#include "domain/terrain/TerrainRaster.h"
#include "domain/texture/TextureImage.h"

namespace gis::application {

// BuildTerrainMeshRequest carries loaded raster data and mesh construction parameters.
// Upstream: UI or workflow code creates it after DEM/image loading succeeds.
// Downstream: BuildTerrainMeshUseCase passes the values to TerrainMeshBuilder.
struct BuildTerrainMeshRequest {
    gis::domain::TerrainRaster dem;    // dem is the elevation raster used for mesh heights.
    gis::domain::TextureImage image;   // image is the texture draped over the terrain.
    int samplingStep = 1;              // samplingStep controls DEM downsampling.
    float verticalScale = 1.0F;        // verticalScale controls vertical exaggeration.
};

// BuildTerrainMeshResult wraps terrain construction success, error, and output mesh.
// Upstream: BuildTerrainMeshUseCase fills it after invoking the Domain builder.
// Downstream: UI can show errors or pass terrainMesh to AddTerrainToSceneUseCase.
struct BuildTerrainMeshResult {
    bool success = false;                 // success is true only when terrainMesh is valid.
    std::string errorMessage;             // errorMessage explains validation or build failures.
    gis::domain::TerrainMesh terrainMesh; // terrainMesh is valid only when success is true.
};

// BuildTerrainMeshUseCase orchestrates DEM-to-mesh conversion.
// Upstream: UI passes loaded terrain data and user parameters.
// Downstream: Domain TerrainMeshBuilder performs geometry construction.
class BuildTerrainMeshUseCase {
public:
    // The constructor receives the Domain terrain mesh builder.
    // Upstream: composition code creates the builder.
    // Downstream: execute delegates geometry work to the builder.
    explicit BuildTerrainMeshUseCase(gis::domain::TerrainMeshBuilder& terrainMeshBuilder);

    // execute validates the request, builds terrain, and returns a safe result.
    // Upstream: callers provide BuildTerrainMeshRequest.
    // Downstream: callers receive BuildTerrainMeshResult instead of exceptions.
    BuildTerrainMeshResult execute(const BuildTerrainMeshRequest& request) const;

private:
    gis::domain::TerrainMeshBuilder& terrainMeshBuilder_; // terrainMeshBuilder_ performs Domain mesh construction.
};

} // namespace gis::application
