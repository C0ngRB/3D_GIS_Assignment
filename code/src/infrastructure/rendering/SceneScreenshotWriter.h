#pragma once

// SceneScreenshotWriter.h declares a simple report screenshot exporter.
// Upstream: UI workflows provide renderer frame statistics and drawable snapshots.
// Downstream: report documentation can use the written BMP files as stable evidence.

#include <string>
#include <vector>

#include "infrastructure/rendering/OpenGLSceneRenderer.h"

namespace gis::infrastructure {

// SceneScreenshotWriter creates deterministic BMP previews from renderer snapshots.
// Upstream: MainWindow calls this after refreshing the viewport.
// Downstream: the filesystem receives a screenshot file that Windows can open directly.
class SceneScreenshotWriter {
public:
    // write saves a report screenshot for the current renderer state.
    // Upstream: UI screenshot actions pass the target path, stats, and drawables.
    // Downstream: the BMP file contains a compact visual summary of the active scene.
    bool write(
        const std::string& filePath,
        const RenderFrameStats& stats,
        const std::vector<DrawableSnapshot>& drawables,
        std::string& errorMessage) const;
};

} // namespace gis::infrastructure
