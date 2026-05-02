#pragma once

// Transform.h defines spatial transform data for models or scene nodes.
// Upstream: loading use cases, UI actions, or batch placement logic write Transform.
// Downstream: renderers convert Transform into a concrete model matrix.

#include "domain/math/Vec3.h"

namespace gis::domain {

// Transform stores translation, rotation, and scale without framework-specific types.
// Upstream: Application layer positions scene objects.
// Downstream: Infrastructure renderers convert it to graphics API matrices.
struct Transform {
    Vec3 translation;                   // translation moves the object in the scene.
    Vec3 rotationEulerDegrees;          // rotationEulerDegrees stores XYZ angles in degrees.
    Vec3 scale{1.0F, 1.0F, 1.0F};       // scale defaults to preserving original size.
};

} // namespace gis::domain
