// AddTerrainToSceneUseCase.cpp implements TerrainMesh insertion into SceneGraph.
// Upstream: terrain mesh construction workflows call this after BuildTerrainMeshUseCase.
// Downstream: rendering workflows read the added terrain node later.

#include "application/usecases/AddTerrainToSceneUseCase.h"

#include "domain/scene/SceneNode.h"

namespace gis::application {

// The constructor stores the scene graph reference.
// Upstream: composition code creates or owns SceneGraph.
// Downstream: execute appends terrain nodes to that scene graph.
AddTerrainToSceneUseCase::AddTerrainToSceneUseCase(gis::domain::SceneGraph& sceneGraph)
    : sceneGraph_(sceneGraph)
{
}

// execute converts TerrainMesh into a SceneNode and appends it.
// Upstream: a successful BuildTerrainMeshUseCase result provides the terrain mesh.
// Downstream: SceneGraph contains geometry, texture, and visibility for rendering.
AddTerrainToSceneResult AddTerrainToSceneUseCase::execute(const AddTerrainToSceneRequest& request)
{
    AddTerrainToSceneResult result;

    if (request.terrainMesh.mesh.vertices.empty()) {
        result.errorMessage = "Terrain mesh has no vertices.";
        return result;
    }

    if (request.terrainMesh.mesh.triangles.empty()) {
        result.errorMessage = "Terrain mesh has no triangles.";
        return result;
    }

    if (request.terrainMesh.texture.pixels.empty()) {
        result.errorMessage = "Terrain texture is empty.";
        return result;
    }

    gis::domain::SceneNode node;
    node.name = request.name.empty() ? "Terrain" : request.name;
    node.mesh = request.terrainMesh.mesh;
    node.texture = request.terrainMesh.texture;
    node.visible = true;

    sceneGraph_.nodes.push_back(node);
    result.nodeIndex = sceneGraph_.nodes.size() - 1U;
    result.success = true;
    return result;
}

} // namespace gis::application
