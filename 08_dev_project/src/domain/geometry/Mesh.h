#pragma once

// Mesh.h defines a generic triangle mesh.
// Upstream: OBJ loading, DEM terrain building, and geometry algorithms create Mesh.
// Downstream: ModelAsset, TerrainMesh, SceneNode, and renderers consume Mesh.

#include <vector>

#include "domain/geometry/Triangle.h"
#include "domain/geometry/Vertex.h"

namespace gis::domain {

// Mesh stores vertex data and indexed triangle faces.
// Upstream: model loaders or terrain mesh builders.
// Downstream: scene nodes and rendering adapters.
struct Mesh {
    std::vector<Vertex> vertices;    // vertices are the geometry points.
    std::vector<Triangle> triangles; // triangles describe the mesh surface.
};

} // namespace gis::domain
