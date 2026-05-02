#pragma once

// AddTerrainToSceneUseCase.h defines the Application use case for terrain scene insertion.
// Upstream: BuildTerrainMeshUseCase produces TerrainMesh.
// Downstream: the use case appends a terrain SceneNode to SceneGraph.

#include <cstddef>
#include <string>

#include "domain/scene/SceneGraph.h"
#include "domain/terrain/TerrainMesh.h"

namespace gis::application {

// AddTerrainToSceneRequest carries one terrain mesh and its scene name.
// Upstream: UI creates it after terrain mesh construction succeeds.
// Downstream: AddTerrainToSceneUseCase validates and inserts it.
struct AddTerrainToSceneRequest {
    gis::domain::TerrainMesh terrainMesh; // terrainMesh is the textured DEM mesh to add.
    std::string name = "Terrain";         // name identifies the terrain node in the scene.
};

// AddTerrainToSceneResult reports scene insertion success and node index.
// Upstream: AddTerrainToSceneUseCase fills it after appending a node.
// Downstream: UI can select or report the inserted node.
struct AddTerrainToSceneResult {
    bool success = false;      // success is true when a terrain node was appended.
    std::string errorMessage;  // errorMessage explains why insertion failed.
    std::size_t nodeIndex = 0; // nodeIndex is valid only when success is true.
};

// AddTerrainToSceneUseCase appends TerrainMesh as a SceneNode.
// Upstream: caller provides a built terrain mesh.
// Downstream: SceneGraph contains geometry and texture for later rendering.
class AddTerrainToSceneUseCase {
public:
    // The constructor receives the active scene graph.
    // Upstream: composition code owns the scene state.
    // Downstream: execute mutates the same SceneGraph instance.
    explicit AddTerrainToSceneUseCase(gis::domain::SceneGraph& sceneGraph);

    // execute validates terrain geometry and appends a SceneNode.
    // Upstream: callers pass AddTerrainToSceneRequest after mesh construction.
    // Downstream: renderers can later draw the terrain node.
    AddTerrainToSceneResult execute(const AddTerrainToSceneRequest& request);

private:
    gis::domain::SceneGraph& sceneGraph_; // sceneGraph_ is the active scene state.
};

} // namespace gis::application
