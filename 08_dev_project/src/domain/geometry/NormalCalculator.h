#pragma once

// NormalCalculator.h defines vertex normal generation for triangle meshes.
// Upstream: TerrainMeshBuilder creates a Mesh with positions and triangles.
// Downstream: renderers use Vertex::normal for lighting and surface shading.

#include "domain/geometry/Mesh.h"

namespace gis::domain {

// NormalCalculator computes per-vertex normals by accumulating triangle normals.
// Upstream: terrain mesh construction or model post-processing supplies a Mesh.
// Downstream: the same Mesh contains normalized Vertex::normal values.
class NormalCalculator {
public:
    // computeVertexNormals resets and fills normals for every vertex in the mesh.
    // Upstream: TerrainMeshBuilder calls this after adding all triangles.
    // Downstream: TextureCoordinateCalculator and renderers can use the updated mesh.
    void computeVertexNormals(Mesh& mesh) const;
};

} // namespace gis::domain
