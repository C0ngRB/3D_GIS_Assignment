// MainWindow.cpp implements the Batch G UI integration facade.
// Upstream: command-line demo mode or future widgets call these UI operations.
// Downstream: Application use cases, renderer snapshots, and screenshot files are produced.

#include "ui/MainWindow.h"

#include <sstream>

#include "application/usecases/AddModelToSceneUseCase.h"
#include "application/usecases/AddTerrainToSceneUseCase.h"
#include "application/usecases/BuildTerrainMeshUseCase.h"
#include "application/usecases/LoadSingleModelUseCase.h"
#include "application/usecases/LoadTerrainUseCase.h"
#include "domain/terrain/TerrainMeshBuilder.h"

namespace {

// makeSuccess creates a successful UI operation result.
// Upstream: MainWindow methods pass compact operation summaries.
// Downstream: command-line demo and future status panels display the message.
gis::ui::UiOperationResult makeSuccess(const std::string& message)
{
    gis::ui::UiOperationResult result;
    result.success = true;
    result.message = message;
    return result;
}

// makeFailure creates a failed UI operation result.
// Upstream: MainWindow methods pass validation or use-case errors.
// Downstream: command-line demo and future status panels display the message.
gis::ui::UiOperationResult makeFailure(const std::string& message)
{
    gis::ui::UiOperationResult result;
    result.success = false;
    result.message = message;
    return result;
}

// appendLoadFailures appends compact per-file failure warnings.
// Upstream: MainWindow::loadBatchModels passes batch loading failures.
// Downstream: UI result warnings explain partial batch failures.
void appendLoadFailures(
    gis::ui::UiOperationResult& uiResult,
    const std::vector<gis::application::ModelLoadFailure>& failures)
{
    for (const gis::application::ModelLoadFailure& failure : failures) {
        uiResult.warnings.push_back(failure.filePath + " -> " + failure.errorMessage);
    }
}

} // namespace

namespace gis::ui {

// The constructor initializes the viewport with the owned scene and renderer.
// Upstream: main creates MainWindow at application startup.
// Downstream: viewport actions always operate on this object's sceneGraph_.
MainWindow::MainWindow()
    : viewport_(sceneGraph_, renderer_)
{
    viewport_.resize(1280, 720);
    viewport_.refresh();
}

// openSingleModel loads one OBJ, inserts it, and refreshes renderer snapshots.
// Upstream: UI model-open action passes modelPath.
// Downstream: SceneGraph and OpenGLSceneRenderer reflect the added model.
UiOperationResult MainWindow::openSingleModel(const std::string& modelPath)
{
    gis::application::LoadSingleModelUseCase loadModelUseCase(modelLoader_);
    gis::application::LoadModelRequest loadRequest;
    loadRequest.filePath = modelPath;

    const gis::application::LoadModelResult loadResult = loadModelUseCase.execute(loadRequest);
    if (!loadResult.success) {
        return makeFailure("Failed to load model: " + loadResult.errorMessage);
    }

    gis::application::AddModelToSceneUseCase addModelToSceneUseCase(sceneGraph_);
    gis::application::AddModelToSceneRequest addRequest;
    addRequest.model = loadResult.model;

    const gis::application::AddModelToSceneResult addResult = addModelToSceneUseCase.execute(addRequest);
    if (!addResult.success) {
        return makeFailure("Failed to add model to scene: " + addResult.errorMessage);
    }

    refresh();
    std::ostringstream message;
    message << "Loaded OBJ model '" << loadResult.model.name << "' at node " << addResult.nodeIndex;
    return makeSuccess(message.str());
}

// openTerrainData reads DEM and image rasters and stores them for buildTerrain.
// Upstream: UI DEM/image open actions pass both selected paths.
// Downstream: buildTerrain uses loadedDem_ and loadedImage_ with current settings.
UiOperationResult MainWindow::openTerrainData(const std::string& demPath, const std::string& imagePath)
{
    gis::application::LoadTerrainUseCase loadTerrainUseCase(demReader_, imageReader_);
    gis::application::LoadTerrainRequest loadRequest;
    loadRequest.demPath = demPath;
    loadRequest.imagePath = imagePath;

    const gis::application::LoadTerrainResult loadResult = loadTerrainUseCase.execute(loadRequest);
    if (!loadResult.success) {
        return makeFailure("Failed to load terrain data: " + loadResult.errorMessage);
    }

    loadedDem_ = loadResult.dem;
    loadedImage_ = loadResult.image;
    hasTerrainData_ = true;

    std::ostringstream message;
    message << "Loaded DEM " << loadedDem_.width << " x " << loadedDem_.height
            << " and image " << loadedImage_.width << " x " << loadedImage_.height;

    UiOperationResult uiResult = makeSuccess(message.str());
    uiResult.warnings = loadResult.warnings;
    return uiResult;
}

// buildTerrain converts cached rasters into one textured terrain scene node.
// Upstream: UI build-terrain action calls this after openTerrainData.
// Downstream: renderer sees a textured, lit terrain drawable after refresh.
UiOperationResult MainWindow::buildTerrain()
{
    if (!hasTerrainData_) {
        return makeFailure("Terrain data has not been loaded.");
    }

    gis::domain::TerrainMeshBuilder terrainMeshBuilder;
    gis::application::BuildTerrainMeshUseCase buildTerrainMeshUseCase(terrainMeshBuilder);
    gis::application::BuildTerrainMeshRequest buildRequest;
    buildRequest.dem = loadedDem_;
    buildRequest.image = loadedImage_;
    buildRequest.samplingStep = samplingStep_;
    buildRequest.verticalScale = verticalScale_;

    const gis::application::BuildTerrainMeshResult buildResult = buildTerrainMeshUseCase.execute(buildRequest);
    if (!buildResult.success) {
        return makeFailure("Failed to build terrain: " + buildResult.errorMessage);
    }

    gis::application::AddTerrainToSceneUseCase addTerrainToSceneUseCase(sceneGraph_);
    gis::application::AddTerrainToSceneRequest addRequest;
    addRequest.terrainMesh = buildResult.terrainMesh;
    addRequest.name = "DalianTerrain";

    const gis::application::AddTerrainToSceneResult addResult = addTerrainToSceneUseCase.execute(addRequest);
    if (!addResult.success) {
        return makeFailure("Failed to add terrain to scene: " + addResult.errorMessage);
    }

    refresh();
    std::ostringstream message;
    message << "Built terrain at node " << addResult.nodeIndex
            << " with step " << samplingStep_
            << " and vertical scale " << verticalScale_;
    return makeSuccess(message.str());
}

// loadBatchModels scans a folder, loads OBJ files independently, and inserts successful models.
// Upstream: UI batch-load action passes the folder and recursive mode.
// Downstream: SceneGraph receives all successfully parsed models.
UiOperationResult MainWindow::loadBatchModels(const std::string& folderPath, bool recursive)
{
    gis::application::LoadBatchModelsUseCase loadBatchModelsUseCase(batchRepository_, modelLoader_);
    gis::application::LoadBatchModelsRequest loadRequest;
    loadRequest.folderPath = folderPath;
    loadRequest.extensions = std::vector<std::string>{".obj"};
    loadRequest.recursive = recursive;

    const gis::application::LoadBatchModelsResult loadResult = loadBatchModelsUseCase.execute(loadRequest);
    if (!loadResult.success) {
        UiOperationResult failure = makeFailure("Failed to load batch models: " + loadResult.errorMessage);
        appendLoadFailures(failure, loadResult.failures);
        return failure;
    }

    std::size_t addFailureCount = 0;
    gis::application::AddModelToSceneUseCase addModelToSceneUseCase(sceneGraph_);
    for (const gis::domain::ModelAsset& model : loadResult.models) {
        gis::application::AddModelToSceneRequest addRequest;
        addRequest.model = model;
        const gis::application::AddModelToSceneResult addResult = addModelToSceneUseCase.execute(addRequest);
        if (!addResult.success) {
            ++addFailureCount;
        }
    }

    refresh();
    std::ostringstream message;
    message << "Batch discovered " << loadResult.discoveredFilePaths.size()
            << ", loaded " << loadResult.models.size()
            << ", load failures " << loadResult.failures.size()
            << ", scene add failures " << addFailureCount;

    UiOperationResult uiResult = makeSuccess(message.str());
    appendLoadFailures(uiResult, loadResult.failures);
    return uiResult;
}

// clearScene removes all nodes and refreshes an empty renderer state.
// Upstream: UI clear-scene button calls this method.
// Downstream: screenshots and frame stats report zero drawables.
UiOperationResult MainWindow::clearScene()
{
    sceneGraph_.nodes.clear();
    refresh();
    return makeSuccess("Scene cleared.");
}

// resetView restores camera state and refreshes renderer snapshots.
// Upstream: UI reset-view button calls this method.
// Downstream: renderer frame stats contain the default camera.
UiOperationResult MainWindow::resetView()
{
    viewport_.resetView();
    refresh();
    return makeSuccess("View reset.");
}

// orbitView rotates the camera and refreshes renderer snapshots.
// Upstream: desktop window view controls call this method.
// Downstream: renderer frame stats and preview drawing reflect the new camera.
UiOperationResult MainWindow::orbitView(float deltaYawDegrees, float deltaPitchDegrees)
{
    viewport_.orbit(deltaYawDegrees, deltaPitchDegrees);
    refresh();
    return makeSuccess("View rotated.");
}

// zoomView zooms the camera and refreshes renderer snapshots.
// Upstream: desktop window zoom controls call this method.
// Downstream: renderer frame stats and preview drawing reflect the new camera distance.
UiOperationResult MainWindow::zoomView(float zoomFactor)
{
    viewport_.zoom(zoomFactor);
    refresh();
    return makeSuccess("View zoomed.");
}

// panView pans the camera target and refreshes renderer snapshots.
// Upstream: desktop window pan controls call this method.
// Downstream: renderer frame stats and preview drawing reflect the new pan offsets.
UiOperationResult MainWindow::panView(float deltaX, float deltaY)
{
    viewport_.pan(deltaX, deltaY);
    refresh();
    return makeSuccess("View panned.");
}

// setSamplingStep validates and stores the terrain sampling interval.
// Upstream: UI numeric input provides samplingStep.
// Downstream: buildTerrain uses samplingStep_ for mesh density.
UiOperationResult MainWindow::setSamplingStep(int samplingStep)
{
    if (samplingStep <= 0) {
        return makeFailure("Sampling step must be greater than zero.");
    }

    samplingStep_ = samplingStep;
    return makeSuccess("Sampling step updated.");
}

// setVerticalScale validates and stores the terrain height multiplier.
// Upstream: UI numeric input provides verticalScale.
// Downstream: buildTerrain uses verticalScale_ for elevation exaggeration.
UiOperationResult MainWindow::setVerticalScale(float verticalScale)
{
    if (verticalScale <= 0.0F) {
        return makeFailure("Vertical scale must be greater than zero.");
    }

    verticalScale_ = verticalScale;
    return makeSuccess("Vertical scale updated.");
}

// saveScreenshot refreshes renderer snapshots and writes a BMP file.
// Upstream: UI screenshot command passes the target file path.
// Downstream: report assets folder receives a deterministic screenshot.
UiOperationResult MainWindow::saveScreenshot(const std::string& filePath)
{
    refresh();

    std::string errorMessage;
    const bool written = screenshotWriter_.write(filePath, renderer_.lastFrameStats(), renderer_.drawables(), errorMessage);
    if (!written) {
        return makeFailure(errorMessage);
    }

    return makeSuccess("Screenshot saved: " + filePath);
}

// refresh updates renderer snapshots for the current scene graph.
// Upstream: scene-changing UI actions call this method.
// Downstream: renderer diagnostics, drawables, and screenshots remain synchronized.
void MainWindow::refresh()
{
    viewport_.refresh();
}

// scene exposes read-only scene state for verification.
// Upstream: command-line demo reads node counts.
// Downstream: no caller can mutate sceneGraph_ through this method.
const gis::domain::SceneGraph& MainWindow::scene() const
{
    return sceneGraph_;
}

// renderer exposes read-only renderer state for verification.
// Upstream: command-line demo reads frame stats and drawables.
// Downstream: no caller can mutate renderer_ through this method.
const gis::infrastructure::OpenGLSceneRenderer& MainWindow::renderer() const
{
    return renderer_;
}

} // namespace gis::ui
