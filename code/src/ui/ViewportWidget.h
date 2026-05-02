#pragma once

// ViewportWidget.h declares the UI-layer viewport interaction facade.
// Upstream: future MainWindow or the current console check owns the scene graph.
// Downstream: ViewportWidget forwards refresh and camera controls to ISceneRenderer.

#include "domain/ports/ISceneRenderer.h"
#include "domain/scene/SceneGraph.h"

namespace gis::ui {

// ViewportWidget is a UI-layer wrapper around scene refresh and basic 3D interaction.
// Upstream: UI events call resize, orbit, zoom, pan, resetView, and refresh.
// Downstream: ISceneRenderer receives all rendering and camera operations.
class ViewportWidget {
public:
    // The constructor stores scene and renderer references.
    // Upstream: composition code wires application scene state and renderer port.
    // Downstream: refresh renders the same scene through the same renderer.
    ViewportWidget(gis::domain::SceneGraph& sceneGraph, gis::domain::ISceneRenderer& renderer);

    // resize updates viewport dimensions.
    // Upstream: UI window resize events call this method.
    // Downstream: ISceneRenderer updates projection-related state.
    void resize(int width, int height);

    // refresh asks the renderer to process the current scene.
    // Upstream: UI calls this after scene changes or interaction.
    // Downstream: ISceneRenderer converts SceneGraph into draw commands or snapshots.
    void refresh();

    // orbit applies camera rotation from pointer movement.
    // Upstream: UI mouse drag handlers call this method.
    // Downstream: ISceneRenderer updates camera yaw and pitch.
    void orbit(float deltaX, float deltaY);

    // zoom applies camera zoom from wheel input.
    // Upstream: UI wheel or pinch handlers call this method.
    // Downstream: ISceneRenderer updates camera distance.
    void zoom(float zoomFactor);

    // pan applies camera target translation from pointer movement.
    // Upstream: UI pan drag handlers call this method.
    // Downstream: ISceneRenderer updates camera pan offsets.
    void pan(float deltaX, float deltaY);

    // resetView restores the default camera.
    // Upstream: UI reset view button calls this method.
    // Downstream: ISceneRenderer resets camera state.
    void resetView();

private:
    gis::domain::SceneGraph& sceneGraph_;     // sceneGraph_ is the active scene state.
    gis::domain::ISceneRenderer& renderer_;   // renderer_ is the rendering port used by the viewport.
};

} // namespace gis::ui
