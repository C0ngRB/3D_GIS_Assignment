#pragma once

// TextureCoordinateCalculator.h defines terrain texture coordinate generation.
// Upstream: TerrainMeshBuilder creates a mesh with planar positions.
// Downstream: renderers use Vertex::texCoord to drape the image over the mesh.

#include "domain/geometry/Mesh.h"

namespace gis::domain {

// TextureCoordinateCalculator maps mesh XY bounds into [0, 1] texture coordinates.
// Upstream: TerrainMeshBuilder supplies a terrain mesh with X/Y planar positions.
// Downstream: TextureImage sampling uses Vertex::texCoord.
class TextureCoordinateCalculator {
public:
    // computeTerrainTexCoords fills texCoord for each vertex using mesh planar bounds.
    // Upstream: TerrainMeshBuilder calls this after vertex positions are complete.
    // Downstream: Renderer can sample the remote-sensing texture on the terrain.
    void computeTerrainTexCoords(Mesh& mesh) const;
};

} // namespace gis::domain
