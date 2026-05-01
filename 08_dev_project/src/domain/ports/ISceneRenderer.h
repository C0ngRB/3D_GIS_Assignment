#pragma once

// ISceneRenderer.h defines the scene-rendering port.
// Upstream: ViewportWidget or another UI component triggers rendering.
// Downstream: a graphics scene renderer implements concrete drawing later.

#include "domain/scene/SceneGraph.h"

namespace gis::domain {

// ISceneRenderer separates scene data from a specific graphics API.
// Upstream: UI requests rendering of the current SceneGraph.
// Downstream: Infrastructure converts domain data into GPU resources.
class ISceneRenderer {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: UI owns or references the renderer abstraction.
    // Downstream: concrete renderers release graphics resources.
    virtual ~ISceneRenderer() = default;

    // render draws the provided scene graph.
    // Upstream: ViewportWidget calls this after refresh or interaction.
    // Downstream: concrete renderers traverse SceneGraph and submit draw commands.
    virtual void render(const SceneGraph& sceneGraph) = 0;
};

} // namespace gis::domain
