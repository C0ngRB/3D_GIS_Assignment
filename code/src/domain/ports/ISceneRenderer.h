#pragma once

// ISceneRenderer.h defines the scene-rendering port.
// Upstream: ViewportWidget or another UI component triggers rendering and camera control.
// Downstream: Infrastructure converts domain scene data into drawable resources.

#include "domain/scene/SceneGraph.h"

namespace gis::domain {

// ISceneRenderer separates scene data and viewport interaction from a specific graphics backend.
// Upstream: UI requests rendering of the current SceneGraph and forwards user interaction.
// Downstream: Infrastructure renderers update camera state and drawable resources.
class ISceneRenderer {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: UI owns or references the renderer abstraction.
    // Downstream: concrete renderers release graphics resources.
    virtual ~ISceneRenderer() = default;

    // resizeViewport updates the target drawing area.
    // Upstream: ViewportWidget calls this when its size changes.
    // Downstream: concrete renderers update projection-related state.
    virtual void resizeViewport(int width, int height) = 0;

    // render draws or prepares the provided scene graph for drawing.
    // Upstream: ViewportWidget calls this after refresh or interaction.
    // Downstream: concrete renderers traverse SceneGraph and submit or cache draw commands.
    virtual void render(const SceneGraph& sceneGraph) = 0;

    // rotateCamera applies orbit-style camera rotation.
    // Upstream: ViewportWidget calls this from mouse drag interaction.
    // Downstream: concrete renderers update camera yaw and pitch.
    virtual void rotateCamera(float deltaYawDegrees, float deltaPitchDegrees) = 0;

    // zoomCamera applies camera distance scaling.
    // Upstream: ViewportWidget calls this from wheel or zoom interaction.
    // Downstream: concrete renderers update camera distance.
    virtual void zoomCamera(float zoomFactor) = 0;

    // panCamera applies viewport-plane camera translation.
    // Upstream: ViewportWidget calls this from pan drag interaction.
    // Downstream: concrete renderers update camera pan offsets.
    virtual void panCamera(float deltaX, float deltaY) = 0;

    // resetCamera restores the default camera.
    // Upstream: ViewportWidget calls this from reset view UI actions.
    // Downstream: concrete renderers reset camera state before the next render.
    virtual void resetCamera() = 0;
};

} // namespace gis::domain
