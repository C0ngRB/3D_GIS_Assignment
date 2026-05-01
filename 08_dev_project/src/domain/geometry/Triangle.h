#pragma once

// Triangle.h defines an indexed triangle face.
// Upstream: OBJ loaders and TerrainMeshBuilder create Triangle values.
// Downstream: Mesh, normal calculation, and renderers use the vertex indices.

#include <cstddef>

namespace gis::domain {

// Triangle references three vertices in Mesh::vertices.
// Upstream: triangulation or OBJ face parsing.
// Downstream: geometry algorithms and renderers assemble triangle surfaces.
struct Triangle {
    std::size_t v0 = 0; // v0 is the first vertex index in Mesh::vertices.
    std::size_t v1 = 0; // v1 is the second vertex index in Mesh::vertices.
    std::size_t v2 = 0; // v2 is the third vertex index in Mesh::vertices.
};

} // namespace gis::domain
