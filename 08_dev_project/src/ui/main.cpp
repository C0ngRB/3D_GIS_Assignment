// main.cpp is the current minimal UI-layer entry.
// Upstream: CMakeLists.txt creates the ThreeDGISApp target.
// Downstream: later UI batches will connect these use cases to real widgets.

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

#include "application/usecases/AddModelToSceneUseCase.h"
#include "application/usecases/AddTerrainToSceneUseCase.h"
#include "application/usecases/BuildTerrainMeshUseCase.h"
#include "application/usecases/LoadSingleModelUseCase.h"
#include "application/usecases/LoadTerrainUseCase.h"
#include "domain/scene/SceneGraph.h"
#include "domain/terrain/TerrainMeshBuilder.h"
#include "infrastructure/model/ObjModelLoader.h"
#include "infrastructure/raster/GeoTiffDemReader.h"
#include "infrastructure/raster/GeoTiffImageReader.h"
#include "infrastructure/rendering/OpenGLSceneRenderer.h"
#include "ui/ViewportWidget.h"

// printUsage shows the current command-line verification modes.
// Upstream: main calls it when the user does not pass enough paths.
// Downstream: developers can run quick checks before real UI integration.
void printUsage()
{
    std::cout << "ThreeDGIS Batch E is ready." << std::endl;
    std::cout << "Usage for OBJ: ThreeDGISApp.exe <model.obj>" << std::endl;
    std::cout << "Usage for terrain: ThreeDGISApp.exe <dem.tif> <image.tif> [samplingStep] [verticalScale]" << std::endl;
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
// Downstream: Batch E verification confirms drawable, texture, light, and camera state.
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

// runModelLoadCheck verifies the Batch B single-model workflow and Batch E renderer adapter.
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

// main wires current command-line checks until the real desktop UI is added.
// Upstream: command-line arguments choose OBJ or terrain loading/build/render verification.
// Downstream: Application use cases and renderer ports keep business logic out of UI widgets.
int main(int argc, char* argv[])
{
    try {
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
