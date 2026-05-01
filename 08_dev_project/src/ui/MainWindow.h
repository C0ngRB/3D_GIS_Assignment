#pragma once

// MainWindow.h declares the UI integration facade for course demonstration.
// Upstream: command-line demo mode or a later real desktop window calls these operations.
// Downstream: Application use cases, ViewportWidget, renderer, and screenshot writer do the work.

#include <string>
#include <vector>

#include "application/usecases/LoadBatchModelsUseCase.h"
#include "domain/scene/SceneGraph.h"
#include "domain/terrain/TerrainRaster.h"
#include "domain/texture/TextureImage.h"
#include "infrastructure/model/FileSystemModelBatchRepository.h"
#include "infrastructure/model/ObjModelLoader.h"
#include "infrastructure/raster/GeoTiffDemReader.h"
#include "infrastructure/raster/GeoTiffImageReader.h"
#include "infrastructure/rendering/OpenGLSceneRenderer.h"
#include "infrastructure/rendering/SceneScreenshotWriter.h"
#include "ui/ViewportWidget.h"

namespace gis::ui {

// UiOperationResult reports one UI action without throwing exceptions.
// Upstream: MainWindow methods fill it after user-triggered actions.
// Downstream: command-line demo and future widgets display the message and counts.
struct UiOperationResult {
    bool success = false;                  // success is true when the UI action completed.
    std::string message;                   // message is a compact user-facing summary.
    std::vector<std::string> warnings;     // warnings carry non-fatal reader or batch messages.
};

// MainWindow is the current UI-layer coordinator for all required Batch G functions.
// Upstream: main or future toolkit event handlers call this class.
// Downstream: use cases mutate SceneGraph and renderer state through stable ports.
class MainWindow {
public:
    // The constructor wires Infrastructure adapters into Application use cases.
    // Upstream: UI composition code creates one MainWindow for the app session.
    // Downstream: all user actions share the same scene, renderer, and viewport.
    MainWindow();

    // openSingleModel loads one OBJ and adds it to the scene.
    // Upstream: the UI open-model command passes a selected OBJ path.
    // Downstream: LoadSingleModelUseCase and AddModelToSceneUseCase update SceneGraph.
    UiOperationResult openSingleModel(const std::string& modelPath);

    // openTerrainData loads DEM and image rasters for later terrain construction.
    // Upstream: UI file selectors pass selected GeoTIFF paths.
    // Downstream: buildTerrain consumes the cached DEM and image data.
    UiOperationResult openTerrainData(const std::string& demPath, const std::string& imagePath);

    // buildTerrain constructs a textured terrain mesh and adds it to the scene.
    // Upstream: UI build-terrain command uses current sampling and vertical-scale settings.
    // Downstream: BuildTerrainMeshUseCase and AddTerrainToSceneUseCase update SceneGraph.
    UiOperationResult buildTerrain();

    // loadBatchModels loads every OBJ from a folder and adds successful models to the scene.
    // Upstream: UI batch-load command passes a folder and recursive option.
    // Downstream: LoadBatchModelsUseCase handles partial failures and SceneGraph receives successes.
    UiOperationResult loadBatchModels(const std::string& folderPath, bool recursive);

    // clearScene removes all scene nodes.
    // Upstream: UI clear button calls this when the user wants a fresh scene.
    // Downstream: renderer refresh sees an empty SceneGraph.
    UiOperationResult clearScene();

    // resetView restores the viewport camera to its default orientation.
    // Upstream: UI reset-view button calls this method.
    // Downstream: ViewportWidget forwards the reset to the renderer.
    UiOperationResult resetView();

    // orbitView rotates the active camera around the scene.
    // Upstream: desktop window arrow controls pass camera rotation deltas.
    // Downstream: ViewportWidget forwards the orbit command to the renderer.
    UiOperationResult orbitView(float deltaYawDegrees, float deltaPitchDegrees);

    // zoomView moves the camera closer or farther from the scene.
    // Upstream: desktop window zoom controls pass a multiplicative zoom factor.
    // Downstream: ViewportWidget forwards the zoom command to the renderer.
    UiOperationResult zoomView(float zoomFactor);

    // panView shifts the camera target on the view plane.
    // Upstream: desktop window pan controls pass target movement deltas.
    // Downstream: ViewportWidget forwards the pan command to the renderer.
    UiOperationResult panView(float deltaX, float deltaY);

    // setSamplingStep stores the DEM downsampling interval.
    // Upstream: UI numeric control passes the requested sampling interval.
    // Downstream: buildTerrain sends the value to BuildTerrainMeshUseCase.
    UiOperationResult setSamplingStep(int samplingStep);

    // setVerticalScale stores the DEM vertical exaggeration.
    // Upstream: UI numeric control passes the requested scale.
    // Downstream: buildTerrain sends the value to BuildTerrainMeshUseCase.
    UiOperationResult setVerticalScale(float verticalScale);

    // saveScreenshot refreshes the viewport and writes a BMP report screenshot.
    // Upstream: UI screenshot button passes the output file path.
    // Downstream: SceneScreenshotWriter serializes renderer snapshots to disk.
    UiOperationResult saveScreenshot(const std::string& filePath);

    // refresh updates renderer snapshots from the current scene.
    // Upstream: UI operations call it after scene or camera changes.
    // Downstream: renderer frame stats and screenshot data stay current.
    void refresh();

    // scene returns the active scene graph for diagnostics.
    // Upstream: command-line demo reads the node count.
    // Downstream: callers can verify UI actions without mutating internals.
    const gis::domain::SceneGraph& scene() const;

    // renderer returns the current renderer adapter for diagnostics.
    // Upstream: command-line demo reads frame stats after UI actions.
    // Downstream: callers can report drawables without bypassing UI workflows.
    const gis::infrastructure::OpenGLSceneRenderer& renderer() const;

private:
    gis::domain::SceneGraph sceneGraph_;                                      // sceneGraph_ stores all UI-loaded scene nodes.
    gis::infrastructure::ObjModelLoader modelLoader_;                         // modelLoader_ reads OBJ files for single and batch workflows.
    gis::infrastructure::FileSystemModelBatchRepository batchRepository_;      // batchRepository_ discovers OBJ files in folders.
    gis::infrastructure::GeoTiffDemReader demReader_;                         // demReader_ reads GeoTIFF DEM rasters.
    gis::infrastructure::GeoTiffImageReader imageReader_;                     // imageReader_ reads GeoTIFF RGB imagery.
    gis::infrastructure::OpenGLSceneRenderer renderer_;                       // renderer_ stores drawable snapshots and camera state.
    gis::infrastructure::SceneScreenshotWriter screenshotWriter_;             // screenshotWriter_ writes report BMP files.
    ViewportWidget viewport_;                                                  // viewport_ forwards UI view actions to the renderer.
    gis::domain::TerrainRaster loadedDem_;                                     // loadedDem_ caches the latest opened DEM.
    gis::domain::TextureImage loadedImage_;                                    // loadedImage_ caches the latest opened image.
    bool hasTerrainData_ = false;                                              // hasTerrainData_ guards buildTerrain before rasters are loaded.
    int samplingStep_ = 30;                                                    // samplingStep_ is the current UI terrain sampling setting.
    float verticalScale_ = 1.0F;                                               // verticalScale_ is the current UI terrain height scale setting.
};

} // namespace gis::ui
