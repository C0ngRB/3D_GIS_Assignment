// BuildTerrainMeshUseCase.cpp implements terrain mesh construction orchestration.
// Upstream: UI or workflow code supplies loaded DEM/image data and parameters.
// Downstream: Domain TerrainMeshBuilder returns a mesh with normals and texture coordinates.

#include "application/usecases/BuildTerrainMeshUseCase.h"

#include <exception>

namespace gis::application {

// The constructor stores the Domain builder dependency.
// Upstream: composition code injects TerrainMeshBuilder.
// Downstream: execute calls the same builder for every request.
BuildTerrainMeshUseCase::BuildTerrainMeshUseCase(gis::domain::TerrainMeshBuilder& terrainMeshBuilder)
    : terrainMeshBuilder_(terrainMeshBuilder)
{
}

// execute builds TerrainMesh and converts failures into BuildTerrainMeshResult.
// Upstream: callers provide DEM, image, sampling step, and vertical scale.
// Downstream: terrain rendering or scene insertion consumes the returned TerrainMesh.
BuildTerrainMeshResult BuildTerrainMeshUseCase::execute(const BuildTerrainMeshRequest& request) const
{
    BuildTerrainMeshResult result;

    try {
        result.terrainMesh = terrainMeshBuilder_.build(
            request.dem,
            request.image,
            request.samplingStep,
            request.verticalScale);
        result.success = true;
        return result;
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown error while building terrain mesh.";
        return result;
    }
}

} // namespace gis::application
