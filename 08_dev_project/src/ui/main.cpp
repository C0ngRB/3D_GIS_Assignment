// main.cpp is the current minimal UI-layer entry.
// Upstream: CMakeLists.txt creates the ThreeDGISApp target.
// Downstream: later UI batches will connect these use cases to real widgets.

#include <iostream>

#include "application/usecases/AddModelToSceneUseCase.h"
#include "application/usecases/LoadSingleModelUseCase.h"
#include "application/usecases/LoadTerrainUseCase.h"
#include "domain/scene/SceneGraph.h"
#include "infrastructure/model/ObjModelLoader.h"
#include "infrastructure/raster/GeoTiffDemReader.h"
#include "infrastructure/raster/GeoTiffImageReader.h"

// printUsage shows the current command-line verification modes.
// Upstream: main calls it when the user does not pass enough paths.
// Downstream: developers can run quick checks before real UI integration.
void printUsage()
{
    std::cout << "ThreeDGIS Batch C is ready." << std::endl;
    std::cout << "Usage for OBJ: ThreeDGISApp.exe <model.obj>" << std::endl;
    std::cout << "Usage for terrain: ThreeDGISApp.exe <dem.tif> <image.tif>" << std::endl;
}

// runModelLoadCheck verifies the Batch B single-model workflow.
// Upstream: main passes one OBJ path.
// Downstream: the loaded model is inserted into the SceneGraph.
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
    return 0;
}

// runTerrainLoadCheck verifies the Batch C DEM and image loading workflow.
// Upstream: main passes DEM and image GeoTIFF paths.
// Downstream: later Batch D will consume the loaded rasters to build TerrainMesh.
int runTerrainLoadCheck(const char* demPath, const char* imagePath)
{
    gis::infrastructure::GeoTiffDemReader demReader;
    gis::infrastructure::GeoTiffImageReader imageReader;
    gis::application::LoadTerrainUseCase loadTerrainUseCase(demReader, imageReader);

    gis::application::LoadTerrainRequest request;
    request.demPath = demPath;
    request.imagePath = imagePath;

    const gis::application::LoadTerrainResult result = loadTerrainUseCase.execute(request);
    if (!result.success) {
        std::cerr << "Failed to load terrain data: " << result.errorMessage << std::endl;
        return 1;
    }

    std::cout << "Loaded DEM: " << result.dem.width << " x " << result.dem.height << std::endl;
    std::cout << "DEM elevations: " << result.dem.elevations.size() << std::endl;
    std::cout << "DEM pixel size: " << result.dem.pixelSizeX << ", " << result.dem.pixelSizeY << std::endl;
    std::cout << "DEM no-data: ";
    if (result.dem.hasNoDataValue) {
        std::cout << result.dem.noDataValue << std::endl;
    } else {
        std::cout << "not defined" << std::endl;
    }

    std::cout << "Loaded image: " << result.image.width << " x " << result.image.height << std::endl;
    std::cout << "Image channels: " << result.image.channels << std::endl;
    std::cout << "Image bytes: " << result.image.pixels.size() << std::endl;
    std::cout << "Image pixel size: " << result.image.pixelSizeX << ", " << result.image.pixelSizeY << std::endl;

    for (const std::string& warning : result.warnings) {
        std::cout << "Warning: " << warning << std::endl;
    }

    return 0;
}

// main wires current command-line checks until the real desktop UI is added.
// Upstream: command-line arguments choose OBJ or terrain loading verification.
// Downstream: Application use cases keep parsing and validation out of UI code.
int main(int argc, char* argv[])
{
    if (argc == 2) {
        return runModelLoadCheck(argv[1]);
    }

    if (argc == 3) {
        return runTerrainLoadCheck(argv[1], argv[2]);
    }

    printUsage();
    return 0;
}
