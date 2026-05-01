#pragma once

// TerrainMesh.h defines a textured 3D terrain mesh.
// Upstream: TerrainMeshBuilder creates it from TerrainRaster and TextureImage.
// Downstream: SceneGraph and renderers display it as a terrain node.

#include "domain/geometry/Mesh.h"
#include "domain/texture/TextureImage.h"

namespace gis::domain {

// TerrainMesh combines terrain geometry and its remote-sensing texture.
// Upstream: terrain building use cases.
// Downstream: scene insertion and rendering.
struct TerrainMesh {
    Mesh mesh;            // mesh is the triangulated DEM surface.
    TextureImage texture; // texture is the image draped over the terrain.
};

} // namespace gis::domain
