#pragma once

// Material.h defines the minimum domain representation of model material data.
// Upstream: OBJ or MTL loaders will populate material fields.
// Downstream: renderers read material color for basic drawing.

#include <string>

#include "domain/math/Vec3.h"

namespace gis::domain {

// Material stores graphics-API-independent material data.
// Upstream: model loading infrastructure.
// Downstream: rendering adapters convert it into shader parameters.
struct Material {
    std::string name;                     // name is the material identifier.
    Vec3 diffuseColor{1.0F, 1.0F, 1.0F};  // diffuseColor is the base surface color.
};

} // namespace gis::domain
