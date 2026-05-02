#pragma once

// Vec3.h defines a three-dimensional vector for positions, normals, and colors.
// Upstream: OBJ loading, DEM mesh building, and normal calculation produce Vec3.
// Downstream: Mesh, Material, Transform, and renderers consume Vec3.

namespace gis::domain {

// Vec3 stores a three-dimensional floating-point coordinate or direction.
// Upstream: model vertices, terrain vertices, and lighting colors.
// Downstream: Vertex::position, Vertex::normal, Material, and Transform.
struct Vec3 {
    float x = 0.0F; // x is the horizontal spatial component.
    float y = 0.0F; // y is the vertical spatial component.
    float z = 0.0F; // z is the elevation or depth spatial component.
};

} // namespace gis::domain
