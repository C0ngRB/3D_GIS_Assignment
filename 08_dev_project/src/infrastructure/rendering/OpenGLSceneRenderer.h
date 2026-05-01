#pragma once

// OpenGLSceneRenderer.h declares the rendering adapter for scene graph drawing.
// Upstream: ViewportWidget drives it through the ISceneRenderer port.
// Downstream: later desktop integration can bind the cached drawable data to a real graphics context.

#include <cstddef>
#include <string>
#include <vector>

#include "domain/math/Vec3.h"
#include "domain/ports/ISceneRenderer.h"

namespace gis::infrastructure {

// CameraState stores orbit camera parameters independent of any UI toolkit.
// Upstream: OpenGLSceneRenderer updates it from ViewportWidget interaction calls.
// Downstream: render diagnostics and future graphics matrices consume these values.
struct CameraState {
    float yawDegrees = 0.0F;      // yawDegrees rotates the camera around the vertical axis.
    float pitchDegrees = 35.0F;   // pitchDegrees tilts the camera above the terrain.
    float distance = 5.0F;        // distance controls camera zoom from the scene center.
    float panX = 0.0F;            // panX offsets the camera target horizontally.
    float panY = 0.0F;            // panY offsets the camera target vertically.
};

// LightState stores simple directional lighting parameters.
// Upstream: OpenGLSceneRenderer owns a default light for mesh diagnostics.
// Downstream: future shader code can translate these fields into uniforms.
struct LightState {
    bool enabled = true;                             // enabled controls whether lighting affects drawable stats.
    gis::domain::Vec3 direction{0.3F, 0.5F, 1.0F};   // direction is the default directional light vector.
    float intensity = 1.0F;                          // intensity is the simple light strength.
};

// DrawableSnapshot is the renderer-facing copy of one visible SceneNode.
// Upstream: OpenGLSceneRenderer builds snapshots from SceneGraph nodes.
// Downstream: diagnostics and future graphics upload code consume the snapshots.
struct DrawableSnapshot {
    std::string name;               // name identifies the drawable source node.
    std::size_t vertexCount = 0;    // vertexCount is the number of vertices to upload or draw.
    std::size_t triangleCount = 0;  // triangleCount is the number of indexed triangles.
    std::size_t indexCount = 0;     // indexCount is triangleCount multiplied by three.
    bool textured = false;          // textured is true when texture data and UVs are usable.
    bool lit = false;               // lit is true when lighting is enabled and normals are usable.
    gis::domain::Vec3 minBounds;    // minBounds is the drawable minimum position.
    gis::domain::Vec3 maxBounds;    // maxBounds is the drawable maximum position.
};

// RenderFrameStats reports the last renderer pass.
// Upstream: OpenGLSceneRenderer fills it during render.
// Downstream: command-line verification and later UI status panels display it.
struct RenderFrameStats {
    int viewportWidth = 1;                    // viewportWidth is the current draw area width.
    int viewportHeight = 1;                   // viewportHeight is the current draw area height.
    std::size_t visibleNodeCount = 0;         // visibleNodeCount is the number of visible scene nodes inspected.
    std::size_t drawableCount = 0;            // drawableCount is the number of drawable snapshots prepared.
    std::size_t skippedNodeCount = 0;         // skippedNodeCount is the number of invalid or hidden nodes skipped.
    std::size_t vertexCount = 0;              // vertexCount is the total drawable vertex count.
    std::size_t triangleCount = 0;            // triangleCount is the total drawable triangle count.
    std::size_t texturedDrawableCount = 0;    // texturedDrawableCount counts drawables using a texture.
    std::size_t litDrawableCount = 0;         // litDrawableCount counts drawables eligible for lighting.
    CameraState camera;                       // camera is the camera state used for the pass.
};

// OpenGLSceneRenderer adapts SceneGraph data to renderable mesh snapshots and camera state.
// Upstream: UI talks to this class only through ISceneRenderer, except diagnostics.
// Downstream: a future real graphics context can replace snapshot preparation with buffer uploads.
class OpenGLSceneRenderer final : public gis::domain::ISceneRenderer {
public:
    // resizeViewport updates renderer viewport dimensions.
    // Upstream: ViewportWidget forwards resize events.
    // Downstream: future projection matrix code uses the dimensions.
    void resizeViewport(int width, int height) override;

    // render prepares drawable snapshots from the provided scene graph.
    // Upstream: ViewportWidget calls render after scene or camera changes.
    // Downstream: diagnostics and future draw calls use the prepared snapshots.
    void render(const gis::domain::SceneGraph& sceneGraph) override;

    // rotateCamera changes orbit camera yaw and pitch.
    // Upstream: ViewportWidget forwards mouse drag deltas.
    // Downstream: render uses the updated camera state in frame stats.
    void rotateCamera(float deltaYawDegrees, float deltaPitchDegrees) override;

    // zoomCamera changes camera distance by a multiplicative factor.
    // Upstream: ViewportWidget forwards wheel or pinch zoom input.
    // Downstream: render uses the updated camera state in frame stats.
    void zoomCamera(float zoomFactor) override;

    // panCamera changes camera target offset.
    // Upstream: ViewportWidget forwards pan drag deltas.
    // Downstream: render uses the updated camera state in frame stats.
    void panCamera(float deltaX, float deltaY) override;

    // resetCamera restores default orbit camera state.
    // Upstream: ViewportWidget forwards reset view actions.
    // Downstream: the next render uses the default view.
    void resetCamera() override;

    // setTextureEnabled toggles texture use for drawable preparation.
    // Upstream: later UI controls can enable or disable terrain image texturing.
    // Downstream: render marks drawables as textured only when this flag is enabled.
    void setTextureEnabled(bool enabled);

    // setLightingEnabled toggles simple lighting use for drawable preparation.
    // Upstream: later UI controls can enable or disable lighting.
    // Downstream: render marks drawables as lit only when this flag is enabled.
    void setLightingEnabled(bool enabled);

    // lastFrameStats returns the most recent render diagnostics.
    // Upstream: command-line verification reads the stats after refresh.
    // Downstream: UI status text can display the same values.
    const RenderFrameStats& lastFrameStats() const;

    // drawables returns renderer-facing snapshots from the most recent render.
    // Upstream: command-line verification inspects prepared drawable data.
    // Downstream: future graphics code can use the same data for buffer uploads.
    const std::vector<DrawableSnapshot>& drawables() const;

private:
    int viewportWidth_ = 1;                         // viewportWidth_ stores the current viewport width.
    int viewportHeight_ = 1;                        // viewportHeight_ stores the current viewport height.
    bool textureEnabled_ = true;                    // textureEnabled_ controls texture use in render().
    LightState light_;                              // light_ stores simple lighting configuration.
    CameraState camera_;                            // camera_ stores orbit camera interaction state.
    RenderFrameStats lastFrameStats_;              // lastFrameStats_ stores the latest renderer diagnostics.
    std::vector<DrawableSnapshot> drawables_;       // drawables_ stores prepared drawable snapshots.
};

} // namespace gis::infrastructure
