#pragma once

// ModelAsset.h defines one loaded 3D model asset.
// Upstream: IModelLoader implementations create ModelAsset from OBJ or similar files.
// Downstream: Application use cases add ModelAsset to the SceneGraph.

#include <string>

#include "domain/geometry/Mesh.h"
#include "domain/model/Material.h"
#include "domain/model/Transform.h"

namespace gis::domain {

// ModelAsset groups a model name, mesh, material, and initial transform.
// Upstream: model file loading.
// Downstream: scene node creation and rendering.
struct ModelAsset {
    std::string name;    // name is the asset name, usually from the file name.
    Mesh mesh;           // mesh is the model geometry.
    Material material;   // material is the model surface description.
    Transform transform; // transform is the default placement in the scene.
};

} // namespace gis::domain
