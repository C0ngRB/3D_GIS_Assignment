// AddModelToSceneUseCase.cpp implements model insertion into SceneGraph.
// Upstream: model loading workflows call this after obtaining ModelAsset.
// Downstream: rendering workflows read SceneGraph nodes later.

#include "application/usecases/AddModelToSceneUseCase.h"

#include "domain/scene/SceneNode.h"

namespace gis::application {

// The constructor stores the active scene graph reference.
// Upstream: composition code creates or owns the SceneGraph.
// Downstream: execute mutates the same SceneGraph instance.
AddModelToSceneUseCase::AddModelToSceneUseCase(gis::domain::SceneGraph& sceneGraph)
    : sceneGraph_(sceneGraph)
{
}

// execute converts a loaded ModelAsset into a SceneNode and appends it.
// Upstream: a successful LoadSingleModelUseCase result provides the model.
// Downstream: the node becomes available for future renderer traversal.
AddModelToSceneResult AddModelToSceneUseCase::execute(const AddModelToSceneRequest& request)
{
    AddModelToSceneResult result;

    if (request.model.mesh.vertices.empty()) {
        result.errorMessage = "Model has no vertices.";
        return result;
    }

    if (request.model.mesh.triangles.empty()) {
        result.errorMessage = "Model has no triangles.";
        return result;
    }

    gis::domain::SceneNode node;
    node.name = request.model.name.empty() ? "UnnamedModel" : request.model.name;
    node.mesh = request.model.mesh;
    node.transform = request.model.transform;
    node.visible = true;

    sceneGraph_.nodes.push_back(node);
    result.nodeIndex = sceneGraph_.nodes.size() - 1U;
    result.success = true;
    return result;
}

} // namespace gis::application
