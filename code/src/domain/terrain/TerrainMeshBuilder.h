#pragma once

// TerrainMeshBuilder.h defines DEM-to-triangle-mesh construction.
// Upstream: BuildTerrainMeshUseCase provides TerrainRaster, TextureImage, and parameters.
// Downstream: TerrainMesh contains geometry, normals, texture coordinates, and image data.

#include "domain/terrain/TerrainMesh.h"
#include "domain/terrain/TerrainRaster.h"
#include "domain/texture/TextureImage.h"

namespace gis::domain {

// TerrainMeshBuilder converts a DEM raster into a textured triangle mesh.
// Upstream: Application layer controls sampling step and vertical scale.
// Downstream: SceneGraph and renderers consume the returned TerrainMesh.
class TerrainMeshBuilder {
public:
    // build creates vertices, triangles, normals, and texture coordinates.
    // Upstream: BuildTerrainMeshUseCase passes validated DEM and image data.
    // Downstream: AddTerrainToSceneUseCase can insert the returned TerrainMesh.
    TerrainMesh build(
        const TerrainRaster& dem,
        const TextureImage& image,
        int samplingStep,
        float verticalScale) const;
};

} // namespace gis::domain
