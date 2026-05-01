#pragma once

// SceneGraph.h defines the collection of scene nodes.
// Upstream: Application use cases add model and terrain nodes.
// Downstream: renderers read SceneGraph and convert nodes to drawable objects.

#include <vector>

#include "domain/scene/SceneNode.h"

namespace gis::domain {

// SceneGraph stores every object currently in the 3D scene.
// Upstream: add-model, add-terrain, and clear-scene operations.
// Downstream: ViewportWidget asks the renderer to traverse and draw nodes.
struct SceneGraph {
    std::vector<SceneNode> nodes; // nodes are all visible or hidden scene objects.
};

} // namespace gis::domain
