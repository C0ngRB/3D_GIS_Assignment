#pragma once

// Vertex.h defines the domain data for one mesh vertex.
// Upstream: OBJ loaders and TerrainMeshBuilder create Vertex values.
// Downstream: Mesh, normal calculation, texture mapping, and renderers read Vertex.

#include "domain/math/Vec2.h"
#include "domain/math/Vec3.h"

namespace gis::domain {

// Vertex groups position, normal, and texture coordinate data.
// Upstream: model file loading or DEM triangulation.
// Downstream: geometry algorithms update it and renderers upload it.
struct Vertex {
    Vec3 position; // position is the vertex coordinate in the 3D scene.
    Vec3 normal;   // normal is the lighting normal for this vertex.
    Vec2 texCoord; // texCoord is the texture sampling coordinate.
};

} // namespace gis::domain
