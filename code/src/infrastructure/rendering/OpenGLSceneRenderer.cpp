// OpenGLSceneRenderer.cpp implements scene-to-drawable rendering adaptation.
// Upstream: ViewportWidget provides SceneGraph refresh and camera interaction.
// Downstream: diagnostics and future graphics buffer upload code consume snapshots.

#include "infrastructure/rendering/OpenGLSceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "domain/geometry/Triangle.h"
#include "domain/geometry/Vertex.h"
#include "domain/scene/SceneNode.h"

namespace {

// clampFloat confines a value to a closed range.
// Upstream: camera interaction uses it for pitch and zoom limits.
// Downstream: renderer state remains stable under repeated UI input.
float clampFloat(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

// vectorLength returns Euclidean length for a domain vector.
// Upstream: hasUsableNormals checks whether normals can support lighting.
// Downstream: drawable snapshots mark lighting readiness.
float vectorLength(const gis::domain::Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

// hasValidTriangleIndices checks whether every triangle references existing vertices.
// Upstream: makeDrawableSnapshot validates scene node topology.
// Downstream: invalid geometry is skipped instead of crashing the renderer.
bool hasValidTriangleIndices(const gis::domain::SceneNode& node)
{
    const std::size_t vertexCount = node.mesh.vertices.size();
    for (const gis::domain::Triangle& triangle : node.mesh.triangles) {
        if (triangle.v0 >= vertexCount || triangle.v1 >= vertexCount || triangle.v2 >= vertexCount) {
            return false;
        }
    }

    return true;
}

// hasUsableNormals checks whether every vertex has a non-zero normal.
// Upstream: makeDrawableSnapshot evaluates lighting readiness.
// Downstream: drawable snapshots count lit drawables only when normals exist.
bool hasUsableNormals(const gis::domain::SceneNode& node)
{
    if (node.mesh.vertices.empty()) {
        return false;
    }

    for (const gis::domain::Vertex& vertex : node.mesh.vertices) {
        if (vectorLength(vertex.normal) <= 0.000001F) {
            return false;
        }
    }

    return true;
}

// hasUsableTexture checks texture metadata and pixel storage.
// Upstream: makeDrawableSnapshot evaluates texture readiness.
// Downstream: drawable snapshots count textured drawables only when texture data exists.
bool hasUsableTexture(const gis::domain::SceneNode& node)
{
    if (node.texture.width <= 0 || node.texture.height <= 0 || node.texture.channels <= 0) {
        return false;
    }

    const auto expectedSize =
        static_cast<std::size_t>(node.texture.width) *
        static_cast<std::size_t>(node.texture.height) *
        static_cast<std::size_t>(node.texture.channels);
    return node.texture.pixels.size() == expectedSize;
}

// hasTexCoordsInRange checks whether every vertex texture coordinate is valid.
// Upstream: makeDrawableSnapshot evaluates terrain texture readiness.
// Downstream: render diagnostics avoid claiming texture support for invalid UVs.
bool hasTexCoordsInRange(const gis::domain::SceneNode& node)
{
    if (node.mesh.vertices.empty()) {
        return false;
    }

    for (const gis::domain::Vertex& vertex : node.mesh.vertices) {
        if (vertex.texCoord.x < 0.0F || vertex.texCoord.x > 1.0F) {
            return false;
        }
        if (vertex.texCoord.y < 0.0F || vertex.texCoord.y > 1.0F) {
            return false;
        }
    }

    return true;
}

// makeBounds initializes drawable bounds from mesh vertices.
// Upstream: makeDrawableSnapshot passes a valid non-empty scene node.
// Downstream: future camera framing and diagnostics use the returned bounds.
void makeBounds(const gis::domain::SceneNode& node, gis::domain::Vec3& minBounds, gis::domain::Vec3& maxBounds)
{
    minBounds = node.mesh.vertices.front().position;
    maxBounds = node.mesh.vertices.front().position;

    for (const gis::domain::Vertex& vertex : node.mesh.vertices) {
        minBounds.x = std::min(minBounds.x, vertex.position.x);
        minBounds.y = std::min(minBounds.y, vertex.position.y);
        minBounds.z = std::min(minBounds.z, vertex.position.z);
        maxBounds.x = std::max(maxBounds.x, vertex.position.x);
        maxBounds.y = std::max(maxBounds.y, vertex.position.y);
        maxBounds.z = std::max(maxBounds.z, vertex.position.z);
    }
}

// isDrawableNode checks whether a scene node can produce a drawable snapshot.
// Upstream: OpenGLSceneRenderer::render evaluates every scene node.
// Downstream: invalid or empty nodes are counted as skipped.
bool isDrawableNode(const gis::domain::SceneNode& node)
{
    return node.visible &&
        !node.mesh.vertices.empty() &&
        !node.mesh.triangles.empty() &&
        hasValidTriangleIndices(node);
}

// makeDrawableSnapshot converts one scene node into renderer-facing metadata.
// Upstream: OpenGLSceneRenderer::render calls it for each drawable node.
// Downstream: frame statistics and future graphics uploads use the snapshot.
gis::infrastructure::DrawableSnapshot makeDrawableSnapshot(
    const gis::domain::SceneNode& node,
    bool textureEnabled,
    bool lightingEnabled)
{
    gis::infrastructure::DrawableSnapshot snapshot;
    snapshot.name = node.name;
    snapshot.vertexCount = node.mesh.vertices.size();
    snapshot.triangleCount = node.mesh.triangles.size();
    snapshot.indexCount = snapshot.triangleCount * 3U;
    snapshot.textured = textureEnabled && hasUsableTexture(node) && hasTexCoordsInRange(node);
    snapshot.lit = lightingEnabled && hasUsableNormals(node);
    makeBounds(node, snapshot.minBounds, snapshot.maxBounds);
    return snapshot;
}

} // namespace

namespace gis::infrastructure {

// resizeViewport stores safe viewport dimensions for later projection work.
// Upstream: ViewportWidget forwards resize events.
// Downstream: render copies these values into frame diagnostics.
void OpenGLSceneRenderer::resizeViewport(int width, int height)
{
    viewportWidth_ = std::max(1, width);
    viewportHeight_ = std::max(1, height);
}

// render converts visible SceneGraph nodes into drawable snapshots and statistics.
// Upstream: ViewportWidget calls this after scene or camera changes.
// Downstream: command-line verification and future graphics code consume drawables_.
void OpenGLSceneRenderer::render(const gis::domain::SceneGraph& sceneGraph)
{
    drawables_.clear();
    lastFrameStats_ = RenderFrameStats{};
    lastFrameStats_.viewportWidth = viewportWidth_;
    lastFrameStats_.viewportHeight = viewportHeight_;
    lastFrameStats_.camera = camera_;

    for (const gis::domain::SceneNode& node : sceneGraph.nodes) {
        if (!node.visible) {
            ++lastFrameStats_.skippedNodeCount;
            continue;
        }

        ++lastFrameStats_.visibleNodeCount;
        if (!isDrawableNode(node)) {
            ++lastFrameStats_.skippedNodeCount;
            continue;
        }

        DrawableSnapshot snapshot = makeDrawableSnapshot(node, textureEnabled_, light_.enabled);
        lastFrameStats_.vertexCount += snapshot.vertexCount;
        lastFrameStats_.triangleCount += snapshot.triangleCount;
        if (snapshot.textured) {
            ++lastFrameStats_.texturedDrawableCount;
        }
        if (snapshot.lit) {
            ++lastFrameStats_.litDrawableCount;
        }

        drawables_.push_back(snapshot);
    }

    lastFrameStats_.drawableCount = drawables_.size();
}

// rotateCamera applies bounded orbit rotation.
// Upstream: ViewportWidget forwards pointer drag deltas.
// Downstream: render reports the updated camera in frame stats.
void OpenGLSceneRenderer::rotateCamera(float deltaYawDegrees, float deltaPitchDegrees)
{
    camera_.yawDegrees += deltaYawDegrees;
    camera_.pitchDegrees = clampFloat(camera_.pitchDegrees + deltaPitchDegrees, -89.0F, 89.0F);
}

// zoomCamera applies bounded multiplicative zoom.
// Upstream: ViewportWidget forwards wheel or pinch factors.
// Downstream: render reports the updated camera distance in frame stats.
void OpenGLSceneRenderer::zoomCamera(float zoomFactor)
{
    if (zoomFactor <= 0.0F) {
        return;
    }

    camera_.distance = clampFloat(camera_.distance * zoomFactor, 0.1F, 100000.0F);
}

// panCamera applies target-plane translation.
// Upstream: ViewportWidget forwards pan deltas.
// Downstream: render reports the updated camera pan offsets in frame stats.
void OpenGLSceneRenderer::panCamera(float deltaX, float deltaY)
{
    camera_.panX += deltaX;
    camera_.panY += deltaY;
}

// resetCamera restores the default orbit view.
// Upstream: ViewportWidget forwards reset view actions.
// Downstream: render uses the default camera state on the next pass.
void OpenGLSceneRenderer::resetCamera()
{
    camera_ = CameraState{};
}

// setTextureEnabled controls whether usable textures are marked active.
// Upstream: later UI controls can toggle texture display.
// Downstream: render computes texturedDrawableCount from this flag.
void OpenGLSceneRenderer::setTextureEnabled(bool enabled)
{
    textureEnabled_ = enabled;
}

// setLightingEnabled controls whether usable normals are marked lit.
// Upstream: later UI controls can toggle simple lighting.
// Downstream: render computes litDrawableCount from this flag.
void OpenGLSceneRenderer::setLightingEnabled(bool enabled)
{
    light_.enabled = enabled;
}

// lastFrameStats exposes diagnostics from the latest render call.
// Upstream: command-line checks call it after ViewportWidget::refresh.
// Downstream: status panels can display the same values later.
const RenderFrameStats& OpenGLSceneRenderer::lastFrameStats() const
{
    return lastFrameStats_;
}

// drawables exposes prepared scene-node snapshots from the latest render call.
// Upstream: command-line checks use it to verify drawability.
// Downstream: future graphics upload code can reuse this renderer-facing data.
const std::vector<DrawableSnapshot>& OpenGLSceneRenderer::drawables() const
{
    return drawables_;
}

} // namespace gis::infrastructure
