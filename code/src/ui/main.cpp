// main.cpp is the current minimal UI-layer entry.
// Upstream: CMakeLists.txt creates the ThreeDGISApp target.
// Downstream: later UI batches will connect these use cases to real widgets.

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "application/usecases/AddModelToSceneUseCase.h"
#include "application/usecases/AddTerrainToSceneUseCase.h"
#include "application/usecases/BuildTerrainMeshUseCase.h"
#include "application/usecases/LoadBatchModelsUseCase.h"
#include "application/usecases/LoadOsgbTilesUseCase.h"
#include "application/usecases/LoadSingleModelUseCase.h"
#include "application/usecases/LoadTerrainUseCase.h"
#include "domain/scene/SceneGraph.h"
#include "domain/terrain/TerrainMeshBuilder.h"
#include "infrastructure/model/FileSystemModelBatchRepository.h"
#include "infrastructure/model/ObjModelLoader.h"
#include "infrastructure/model/OsgbToObjConverter.h"
#include "infrastructure/raster/GeoTiffDemReader.h"
#include "infrastructure/raster/GeoTiffImageReader.h"
#include "infrastructure/rendering/OpenGLSceneRenderer.h"
#include "ui/MainWindow.h"
#include "ui/ViewportWidget.h"
#include "ui/Win32DesktopWindow.h"

// printUsage shows the current command-line verification modes.
// Upstream: main calls it when the user does not pass enough paths.
// Downstream: developers can run quick checks before real UI integration.
void printUsage()
{
    std::cout << "ThreeDGIS Batch G is ready." << std::endl;
    std::cout << "Usage for OBJ: ThreeDGISApp.exe <model.obj>" << std::endl;
    std::cout << "Usage for terrain: ThreeDGISApp.exe <dem.tif> <image.tif> [samplingStep] [verticalScale]" << std::endl;
    std::cout << "Usage for batch OBJ: ThreeDGISApp.exe --batch <folder> [recursive]" << std::endl;
    std::cout << "Usage for OSGB tiles: ThreeDGISApp.exe --osgb <folder> [maxTiles]" << std::endl;
    std::cout << "Usage for UI demo screenshots: ThreeDGISApp.exe --demo-screenshots <outputFolder> <dataRoot>" << std::endl;
    std::cout << "Run without arguments to open the interactive desktop app." << std::endl;
}

// parseSamplingStep returns the requested DEM sampling step or a conservative default.
// Upstream: main passes command-line arguments from the UI-layer verification mode.
// Downstream: BuildTerrainMeshUseCase uses the value to control terrain mesh density.
int parseSamplingStep(int argc, char* argv[])
{
    if (argc < 4) {
        return 30;
    }

    return std::stoi(argv[3]);
}

// parseVerticalScale returns the requested vertical exaggeration or a safe default.
// Upstream: main passes command-line arguments from the UI-layer verification mode.
// Downstream: TerrainMeshBuilder uses the value to scale DEM elevations.
float parseVerticalScale(int argc, char* argv[])
{
    if (argc < 5) {
        return 1.0F;
    }

    return std::stof(argv[4]);
}

// parseRecursiveFlag returns whether batch folder search should include subfolders.
// Upstream: main passes the optional batch command argument.
// Downstream: FileSystemModelBatchRepository uses it to choose recursive traversal.
bool parseRecursiveFlag(int argc, char* argv[])
{
    if (argc < 4) {
        return false;
    }

    const std::string value = argv[3];
    return value == "1" || value == "true" || value == "yes" || value == "recursive";
}

// parseMaxTiles returns the requested OSGB tile import limit or a safe default.
// Upstream: main passes the optional --osgb command argument.
// Downstream: LoadOsgbTilesUseCase uses it to avoid loading thousands of tiles at once.
std::size_t parseMaxTiles(int argc, char* argv[])
{
    if (argc < 4) {
        return 4U;
    }

    return static_cast<std::size_t>(std::stoul(argv[3]));
}

// printUiOperationResult writes one UI action result to stdout.
// Upstream: runDemoScreenshots calls it after each MainWindow operation.
// Downstream: verification logs show success, warnings, and failures in order.
void printUiOperationResult(const gis::ui::UiOperationResult& result)
{
    std::cout << (result.success ? "UI success: " : "UI failure: ") << result.message << std::endl;
    for (const std::string& warning : result.warnings) {
        std::cout << "UI warning: " << warning << std::endl;
    }
}

// requireUiSuccess prints a UI result and returns whether the workflow can continue.
// Upstream: runDemoScreenshots uses it as a compact guard after each required action.
// Downstream: demo mode exits early on the first failed UI operation.
bool requireUiSuccess(const gis::ui::UiOperationResult& result)
{
    printUiOperationResult(result);
    return result.success;
}

// isNormalNonZero checks whether a vertex normal has useful length.
// Upstream: printTerrainMeshDiagnostics passes terrain vertex normals.
// Downstream: command-line verification reports whether normal calculation succeeded.
bool isNormalNonZero(const gis::domain::Vec3& normal)
{
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    return length > 0.000001F;
}

// areAllNormalsNonZero checks every generated vertex normal.
// Upstream: printTerrainMeshDiagnostics passes the built terrain mesh.
// Downstream: command-line verification reports whether normal calculation met acceptance.
bool areAllNormalsNonZero(const gis::domain::TerrainMesh& terrainMesh)
{
    for (const gis::domain::Vertex& vertex : terrainMesh.mesh.vertices) {
        if (!isNormalNonZero(vertex.normal)) {
            return false;
        }
    }

    return true;
}

// areTriangleIndicesInRange checks whether every triangle references existing vertices.
// Upstream: printTerrainMeshDiagnostics passes the built terrain mesh.
// Downstream: command-line verification reports whether mesh topology is valid.
bool areTriangleIndicesInRange(const gis::domain::TerrainMesh& terrainMesh)
{
    const std::size_t vertexCount = terrainMesh.mesh.vertices.size();
    for (const gis::domain::Triangle& triangle : terrainMesh.mesh.triangles) {
        if (triangle.v0 >= vertexCount || triangle.v1 >= vertexCount || triangle.v2 >= vertexCount) {
            return false;
        }
    }

    return true;
}

// areTexCoordsInRange checks whether all generated UV coordinates are inside [0, 1].
// Upstream: printTerrainMeshDiagnostics passes the built terrain mesh.
// Downstream: command-line verification reports whether texture coordinate generation succeeded.
bool areTexCoordsInRange(const gis::domain::TerrainMesh& terrainMesh)
{
    for (const gis::domain::Vertex& vertex : terrainMesh.mesh.vertices) {
        if (vertex.texCoord.x < 0.0F || vertex.texCoord.x > 1.0F) {
            return false;
        }
        if (vertex.texCoord.y < 0.0F || vertex.texCoord.y > 1.0F) {
            return false;
        }
    }

    return true;
}

// printTerrainMeshDiagnostics writes terrain mesh acceptance data to stdout.
// Upstream: runTerrainBuildCheck calls it after BuildTerrainMeshUseCase succeeds.
// Downstream: developers can verify vertex count, triangle count, normals, and UV range.
void printTerrainMeshDiagnostics(const gis::domain::TerrainMesh& terrainMesh)
{
    std::cout << "Terrain vertices: " << terrainMesh.mesh.vertices.size() << std::endl;
    std::cout << "Terrain triangles: " << terrainMesh.mesh.triangles.size() << std::endl;
    std::cout << "Triangle indices in range: " << (areTriangleIndicesInRange(terrainMesh) ? "yes" : "no") << std::endl;

    if (!terrainMesh.mesh.vertices.empty()) {
        const gis::domain::Vec3& normal = terrainMesh.mesh.vertices.front().normal;
        std::cout << "First normal: " << normal.x << ", " << normal.y << ", " << normal.z << std::endl;
        std::cout << "First normal non-zero: " << (isNormalNonZero(normal) ? "yes" : "no") << std::endl;
    }

    std::cout << "All normals non-zero: " << (areAllNormalsNonZero(terrainMesh) ? "yes" : "no") << std::endl;
    std::cout << "Texture coordinates in [0,1]: " << (areTexCoordsInRange(terrainMesh) ? "yes" : "no") << std::endl;
}

// printRendererStats writes renderer adapter diagnostics to stdout.
// Upstream: model and terrain command checks call it after ViewportWidget::refresh.
// Downstream: Batch E/F verification confirms drawable, texture, light, and camera state.
void printRendererStats(const gis::infrastructure::RenderFrameStats& stats)
{
    std::cout << "Renderer viewport: " << stats.viewportWidth << " x " << stats.viewportHeight << std::endl;
    std::cout << "Renderer drawables: " << stats.drawableCount << std::endl;
    std::cout << "Renderer vertices: " << stats.vertexCount << std::endl;
    std::cout << "Renderer triangles: " << stats.triangleCount << std::endl;
    std::cout << "Renderer textured drawables: " << stats.texturedDrawableCount << std::endl;
    std::cout << "Renderer lit drawables: " << stats.litDrawableCount << std::endl;
    std::cout << "Renderer skipped nodes: " << stats.skippedNodeCount << std::endl;
    std::cout << "Camera yaw/pitch/distance: "
              << stats.camera.yawDegrees << ", "
              << stats.camera.pitchDegrees << ", "
              << stats.camera.distance << std::endl;
    std::cout << "Camera pan: " << stats.camera.panX << ", " << stats.camera.panY << std::endl;
}

// printUiRendererStats writes renderer diagnostics after a UI workflow step.
// Upstream: runDemoScreenshots calls it after each screenshot is saved.
// Downstream: Batch G verification confirms the UI produced drawable scene states.
void printUiRendererStats(const gis::ui::MainWindow& mainWindow)
{
    printRendererStats(mainWindow.renderer().lastFrameStats());
    std::cout << "UI scene nodes: " << mainWindow.scene().nodes.size() << std::endl;
}

// printLoadFailures writes a compact list of per-file batch loading failures.
// Upstream: runBatchModelLoadCheck passes LoadBatchModelsResult::failures.
// Downstream: command-line verification can confirm partial failure behavior.
void printLoadFailures(const std::vector<gis::application::ModelLoadFailure>& failures)
{
    const std::size_t maxPrintedFailures = std::min<std::size_t>(failures.size(), 5U);
    for (std::size_t index = 0; index < maxPrintedFailures; ++index) {
        std::cout << "Batch failure: " << failures[index].filePath << " -> " << failures[index].errorMessage << std::endl;
    }

    if (failures.size() > maxPrintedFailures) {
        std::cout << "Batch failures omitted: " << failures.size() - maxPrintedFailures << std::endl;
    }
}

// refreshViewport prepares renderer state for the current scene.
// Upstream: command checks call this after adding nodes to SceneGraph.
// Downstream: OpenGLSceneRenderer stores drawable snapshots and frame stats.
void refreshViewport(
    gis::domain::SceneGraph& scene,
    gis::infrastructure::OpenGLSceneRenderer& renderer,
    gis::ui::ViewportWidget& viewport)
{
    viewport.resize(1280, 720);
    viewport.refresh();
    printRendererStats(renderer.lastFrameStats());
}

// runModelLoadCheck verifies the Batch B single-model workflow and renderer adapter.
// Upstream: main passes one OBJ path.
// Downstream: the loaded model is inserted into the SceneGraph and prepared for drawing.
int runModelLoadCheck(const char* modelPath)
{
    gis::domain::SceneGraph scene;
    gis::infrastructure::ObjModelLoader modelLoader;
    gis::application::LoadSingleModelUseCase loadModelUseCase(modelLoader);
    gis::application::AddModelToSceneUseCase addModelToSceneUseCase(scene);

    gis::application::LoadModelRequest loadRequest;
    loadRequest.filePath = modelPath;

    const gis::application::LoadModelResult loadResult = loadModelUseCase.execute(loadRequest);
    if (!loadResult.success) {
        std::cerr << "Failed to load model: " << loadResult.errorMessage << std::endl;
        return 1;
    }

    gis::application::AddModelToSceneRequest addRequest;
    addRequest.model = loadResult.model;

    const gis::application::AddModelToSceneResult addResult = addModelToSceneUseCase.execute(addRequest);
    if (!addResult.success) {
        std::cerr << "Failed to add model to scene: " << addResult.errorMessage << std::endl;
        return 1;
    }

    std::cout << "Loaded model: " << loadResult.model.name << std::endl;
    std::cout << "Vertices: " << loadResult.model.mesh.vertices.size() << std::endl;
    std::cout << "Triangles: " << loadResult.model.mesh.triangles.size() << std::endl;
    std::cout << "Scene nodes: " << scene.nodes.size() << std::endl;
    std::cout << "Inserted node index: " << addResult.nodeIndex << std::endl;

    gis::infrastructure::OpenGLSceneRenderer renderer;
    gis::ui::ViewportWidget viewport(scene, renderer);
    refreshViewport(scene, renderer, viewport);
    return 0;
}

// runBatchModelLoadCheck verifies folder discovery, partial loading, scene insertion, and rendering.
// Upstream: main passes a model folder and optional recursive flag.
// Downstream: successful models are added to SceneGraph while failures remain reportable.
int runBatchModelLoadCheck(const char* folderPath, bool recursive)
{
    gis::domain::SceneGraph scene;
    gis::infrastructure::FileSystemModelBatchRepository modelBatchRepository;
    gis::infrastructure::ObjModelLoader modelLoader;
    gis::application::LoadBatchModelsUseCase loadBatchModelsUseCase(modelBatchRepository, modelLoader);
    gis::application::AddModelToSceneUseCase addModelToSceneUseCase(scene);

    gis::application::LoadBatchModelsRequest request;
    request.folderPath = folderPath;
    request.extensions = std::vector<std::string>{".obj"};
    request.recursive = recursive;

    const gis::application::LoadBatchModelsResult result = loadBatchModelsUseCase.execute(request);
    if (!result.success) {
        std::cerr << "Failed to load batch models: " << result.errorMessage << std::endl;
        printLoadFailures(result.failures);
        return 1;
    }

    std::size_t addFailureCount = 0;
    for (const gis::domain::ModelAsset& model : result.models) {
        gis::application::AddModelToSceneRequest addRequest;
        addRequest.model = model;

        const gis::application::AddModelToSceneResult addResult = addModelToSceneUseCase.execute(addRequest);
        if (!addResult.success) {
            ++addFailureCount;
            std::cout << "Scene add failure: " << model.name << " -> " << addResult.errorMessage << std::endl;
        }
    }

    std::cout << "Batch folder: " << folderPath << std::endl;
    std::cout << "Batch recursive: " << (recursive ? "yes" : "no") << std::endl;
    std::cout << "Batch discovered files: " << result.discoveredFilePaths.size() << std::endl;
    std::cout << "Batch loaded models: " << result.models.size() << std::endl;
    std::cout << "Batch load failures: " << result.failures.size() << std::endl;
    std::cout << "Batch scene add failures: " << addFailureCount << std::endl;
    std::cout << "Scene nodes: " << scene.nodes.size() << std::endl;
    printLoadFailures(result.failures);

    gis::infrastructure::OpenGLSceneRenderer renderer;
    gis::ui::ViewportWidget viewport(scene, renderer);
    refreshViewport(scene, renderer, viewport);
    return addFailureCount == 0 ? 0 : 1;
}

// runOsgbLoadCheck verifies OSGB discovery, conversion, loading, scene insertion, and rendering.
// Upstream: main passes an OSGB folder and optional tile limit.
// Downstream: converted OBJ models are added to SceneGraph when osgconv is available.
int runOsgbLoadCheck(const char* folderPath, std::size_t maxTiles)
{
    gis::domain::SceneGraph scene;
    gis::infrastructure::FileSystemModelBatchRepository modelBatchRepository;
    gis::infrastructure::OsgbToObjConverter osgbConverter;
    gis::infrastructure::ObjModelLoader modelLoader;
    gis::application::LoadOsgbTilesUseCase loadOsgbTilesUseCase(modelBatchRepository, osgbConverter, modelLoader);
    gis::application::AddModelToSceneUseCase addModelToSceneUseCase(scene);

    gis::application::LoadOsgbTilesRequest request;
    request.folderPath = folderPath;
    request.conversionCacheFolderPath = std::string(folderPath) + "\\_converted_obj_cache";
    request.recursive = true;
    request.maxTiles = maxTiles;

    const gis::application::LoadOsgbTilesResult result = loadOsgbTilesUseCase.execute(request);
    if (!result.success) {
        std::cerr << "Failed to import OSGB tiles: " << result.errorMessage << std::endl;
        printLoadFailures(result.failures);
        std::cout << "OSGB discovered files: " << result.discoveredFilePaths.size() << std::endl;
        std::cout << "OSGB selected files: " << result.selectedFilePaths.size() << std::endl;
        return 1;
    }

    std::size_t addFailureCount = 0;
    for (const gis::domain::ModelAsset& model : result.models) {
        gis::application::AddModelToSceneRequest addRequest;
        addRequest.model = model;
        const gis::application::AddModelToSceneResult addResult = addModelToSceneUseCase.execute(addRequest);
        if (!addResult.success) {
            ++addFailureCount;
        }
    }

    std::cout << "OSGB folder: " << folderPath << std::endl;
    std::cout << "OSGB discovered files: " << result.discoveredFilePaths.size() << std::endl;
    std::cout << "OSGB selected files: " << result.selectedFilePaths.size() << std::endl;
    std::cout << "OSGB converted OBJ files: " << result.convertedObjPaths.size() << std::endl;
    std::cout << "OSGB loaded models: " << result.models.size() << std::endl;
    std::cout << "OSGB import failures: " << result.failures.size() << std::endl;
    std::cout << "OSGB scene add failures: " << addFailureCount << std::endl;
    std::cout << "Scene nodes: " << scene.nodes.size() << std::endl;
    printLoadFailures(result.failures);

    gis::infrastructure::OpenGLSceneRenderer renderer;
    gis::ui::ViewportWidget viewport(scene, renderer);
    refreshViewport(scene, renderer, viewport);
    return addFailureCount == 0 ? 0 : 1;
}

// runTerrainBuildCheck verifies DEM/image loading, terrain mesh construction, scene insertion, and rendering.
// Upstream: main passes DEM and image GeoTIFF paths plus optional build parameters.
// Downstream: renderer adapter prepares the terrain node with texture and lighting metadata.
int runTerrainBuildCheck(const char* demPath, const char* imagePath, int samplingStep, float verticalScale)
{
    gis::domain::SceneGraph scene;
    gis::infrastructure::GeoTiffDemReader demReader;
    gis::infrastructure::GeoTiffImageReader imageReader;
    gis::application::LoadTerrainUseCase loadTerrainUseCase(demReader, imageReader);

    gis::application::LoadTerrainRequest loadRequest;
    loadRequest.demPath = demPath;
    loadRequest.imagePath = imagePath;

    const gis::application::LoadTerrainResult loadResult = loadTerrainUseCase.execute(loadRequest);
    if (!loadResult.success) {
        std::cerr << "Failed to load terrain data: " << loadResult.errorMessage << std::endl;
        return 1;
    }

    std::cout << "Loaded DEM: " << loadResult.dem.width << " x " << loadResult.dem.height << std::endl;
    std::cout << "Loaded image: " << loadResult.image.width << " x " << loadResult.image.height << std::endl;
    std::cout << "Sampling step: " << samplingStep << std::endl;
    std::cout << "Vertical scale: " << verticalScale << std::endl;

    for (const std::string& warning : loadResult.warnings) {
        std::cout << "Warning: " << warning << std::endl;
    }

    gis::domain::TerrainMeshBuilder terrainMeshBuilder;
    gis::application::BuildTerrainMeshUseCase buildTerrainMeshUseCase(terrainMeshBuilder);

    gis::application::BuildTerrainMeshRequest buildRequest;
    buildRequest.dem = loadResult.dem;
    buildRequest.image = loadResult.image;
    buildRequest.samplingStep = samplingStep;
    buildRequest.verticalScale = verticalScale;

    const gis::application::BuildTerrainMeshResult buildResult = buildTerrainMeshUseCase.execute(buildRequest);
    if (!buildResult.success) {
        std::cerr << "Failed to build terrain mesh: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    printTerrainMeshDiagnostics(buildResult.terrainMesh);

    gis::application::AddTerrainToSceneUseCase addTerrainToSceneUseCase(scene);
    gis::application::AddTerrainToSceneRequest addRequest;
    addRequest.terrainMesh = buildResult.terrainMesh;
    addRequest.name = "DalianTerrain";

    const gis::application::AddTerrainToSceneResult addResult = addTerrainToSceneUseCase.execute(addRequest);
    if (!addResult.success) {
        std::cerr << "Failed to add terrain to scene: " << addResult.errorMessage << std::endl;
        return 1;
    }

    std::cout << "Scene nodes: " << scene.nodes.size() << std::endl;
    std::cout << "Inserted terrain node index: " << addResult.nodeIndex << std::endl;

    gis::infrastructure::OpenGLSceneRenderer renderer;
    gis::ui::ViewportWidget viewport(scene, renderer);
    refreshViewport(scene, renderer, viewport);

    viewport.orbit(15.0F, -5.0F);
    viewport.zoom(0.8F);
    viewport.pan(2.0F, -1.0F);
    viewport.refresh();
    std::cout << "After interaction:" << std::endl;
    printRendererStats(renderer.lastFrameStats());

    viewport.resetView();
    viewport.refresh();
    std::cout << "After reset:" << std::endl;
    printRendererStats(renderer.lastFrameStats());
    return 0;
}

// runDemoScreenshots exercises the Batch G UI facade and writes three report BMP screenshots.
// Upstream: main calls it for --demo-screenshots verification mode.
// Downstream: outputFolder receives single_model.bmp, terrain.bmp, and batch_models.bmp.
int runDemoScreenshots(const std::string& outputFolder, const std::string& dataRoot)
{
    const std::filesystem::path outputPath(outputFolder);
    const std::filesystem::path rootPath(dataRoot);
    const std::filesystem::path modelPath = rootPath / "mesh_obj" / "Mesh.obj";
    const std::filesystem::path demPath = rootPath / "dem_image_processed" / "DalianDem_Clip.tif";
    const std::filesystem::path imagePath = rootPath / "dem_image_processed" / "Dalian_Clip.tif";

    gis::ui::MainWindow mainWindow;

    if (!requireUiSuccess(mainWindow.openSingleModel(modelPath.string()))) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.saveScreenshot((outputPath / "single_model.bmp").string()))) {
        return 1;
    }
    printUiRendererStats(mainWindow);

    if (!requireUiSuccess(mainWindow.clearScene())) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.setSamplingStep(30))) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.setVerticalScale(1.0F))) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.openTerrainData(demPath.string(), imagePath.string()))) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.buildTerrain())) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.resetView())) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.saveScreenshot((outputPath / "terrain.bmp").string()))) {
        return 1;
    }
    printUiRendererStats(mainWindow);

    if (!requireUiSuccess(mainWindow.clearScene())) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.loadBatchModels(rootPath.string(), true))) {
        return 1;
    }
    if (!requireUiSuccess(mainWindow.saveScreenshot((outputPath / "batch_models.bmp").string()))) {
        return 1;
    }
    printUiRendererStats(mainWindow);

    return 0;
}

// main wires current command-line checks until the real desktop UI is added.
// Upstream: command-line arguments choose OBJ, batch OBJ, or terrain verification.
// Downstream: Application use cases and renderer ports keep business logic out of UI widgets.
int main(int argc, char* argv[])
{
    try {
        if (argc == 1) {
            return gis::ui::runWin32DesktopWindow();
        }

        if (argc >= 2 && argc <= 4 && std::string(argv[1]) == "--demo-screenshots") {
            const std::string outputFolder = argc >= 3 ? argv[2] : "07_3d_screenshots";
            const std::string dataRoot = argc >= 4 ? argv[3] : "data";
            return runDemoScreenshots(outputFolder, dataRoot);
        }

        if (argc >= 3 && argc <= 4 && std::string(argv[1]) == "--batch") {
            return runBatchModelLoadCheck(argv[2], parseRecursiveFlag(argc, argv));
        }

        if (argc >= 3 && argc <= 4 && std::string(argv[1]) == "--osgb") {
            return runOsgbLoadCheck(argv[2], parseMaxTiles(argc, argv));
        }

        if (argc == 2) {
            return runModelLoadCheck(argv[1]);
        }

        if (argc >= 3 && argc <= 5) {
            return runTerrainBuildCheck(argv[1], argv[2], parseSamplingStep(argc, argv), parseVerticalScale(argc, argv));
        }
    } catch (const std::exception& error) {
        std::cerr << "Invalid command line: " << error.what() << std::endl;
        return 1;
    }

    printUsage();
    return 0;
}
