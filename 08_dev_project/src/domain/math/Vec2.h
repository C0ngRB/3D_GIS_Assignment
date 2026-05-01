#pragma once

// Vec2.h defines a two-dimensional vector for texture coordinates.
// Upstream: texture coordinate generation and OBJ vt records produce Vec2 values.
// Downstream: Vertex, texture coordinate calculators, and renderers consume Vec2.

namespace gis::domain {

// Vec2 stores a two-dimensional floating-point coordinate.
// Upstream: DEM texture mapping or model texture coordinate loading.
// Downstream: Vertex::texCoord and rendering adapters.
struct Vec2 {
    float x = 0.0F; // x is the horizontal texture or coordinate component.
    float y = 0.0F; // y is the vertical texture or coordinate component.
};

} // namespace gis::domain
