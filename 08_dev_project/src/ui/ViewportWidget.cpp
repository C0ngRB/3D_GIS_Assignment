// ViewportWidget.cpp implements UI-layer viewport interaction forwarding.
// Upstream: current console checks and future MainWindow call this facade.
// Downstream: ISceneRenderer owns rendering and camera behavior.

#include "ui/ViewportWidget.h"

namespace gis::ui {

// The constructor stores references without taking ownership.
// Upstream: composition code creates SceneGraph and renderer for the viewport.
// Downstream: every viewport action uses these same references.
ViewportWidget::ViewportWidget(gis::domain::SceneGraph& sceneGraph, gis::domain::ISceneRenderer& renderer)
    : sceneGraph_(sceneGraph), renderer_(renderer)
{
}

// resize forwards viewport size to the renderer.
// Upstream: UI resize events provide width and height.
// Downstream: renderer updates its viewport state.
void ViewportWidget::resize(int width, int height)
{
    renderer_.resizeViewport(width, height);
}

// refresh forwards the current scene graph to the renderer.
// Upstream: UI calls this after scene or camera changes.
// Downstream: renderer prepares or draws visible scene nodes.
void ViewportWidget::refresh()
{
    renderer_.render(sceneGraph_);
}

// orbit forwards rotation deltas to the renderer.
// Upstream: UI pointer drag events provide horizontal and vertical deltas.
// Downstream: renderer updates orbit camera state.
void ViewportWidget::orbit(float deltaX, float deltaY)
{
    renderer_.rotateCamera(deltaX, deltaY);
}

// zoom forwards a zoom factor to the renderer.
// Upstream: UI wheel events provide a multiplicative zoom factor.
// Downstream: renderer clamps and stores camera distance.
void ViewportWidget::zoom(float zoomFactor)
{
    renderer_.zoomCamera(zoomFactor);
}

// pan forwards target-plane movement to the renderer.
// Upstream: UI drag events provide pan deltas.
// Downstream: renderer stores camera pan offsets.
void ViewportWidget::pan(float deltaX, float deltaY)
{
    renderer_.panCamera(deltaX, deltaY);
}

// resetView forwards a camera reset command to the renderer.
// Upstream: UI reset controls call this method.
// Downstream: renderer restores default camera state.
void ViewportWidget::resetView()
{
    renderer_.resetCamera();
}

} // namespace gis::ui
