#pragma once

// SceneNode.h defines one node in the 3D scene.
// Upstream: AddModelToSceneUseCase and AddTerrainToSceneUseCase create SceneNode.
// Downstream: SceneGraph stores nodes and renderers draw them.

#include <string>

#include "domain/geometry/Mesh.h"
#include "domain/model/Transform.h"
#include "domain/texture/TextureImage.h"

namespace gis::domain {

// SceneNode stores renderable scene data at the domain level.
// Upstream: Application layer converts models or terrain into nodes.
// Downstream: rendering adapters read mesh, texture, and transform.
struct SceneNode {
    std::string name;     // name identifies the node for UI display or debugging.
    Mesh mesh;            // mesh is the geometry drawn for this node.
    TextureImage texture; // texture is optional; empty data means no texture.
    Transform transform;  // transform places the node in the scene.
    bool visible = true;  // visible controls whether the renderer draws this node.
};

} // namespace gis::domain
