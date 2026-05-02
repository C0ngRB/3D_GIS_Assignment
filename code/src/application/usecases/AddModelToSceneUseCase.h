#pragma once

// AddModelToSceneUseCase.h defines the Application use case for scene insertion.
// Upstream: model loading use cases produce ModelAsset values.
// Downstream: the use case creates SceneNode values inside SceneGraph.

#include <cstddef>
#include <string>

#include "domain/model/ModelAsset.h"
#include "domain/scene/SceneGraph.h"

namespace gis::application {

// AddModelToSceneRequest carries the model that should become a scene node.
// Upstream: UI passes a successfully loaded ModelAsset.
// Downstream: AddModelToSceneUseCase validates and inserts it.
struct AddModelToSceneRequest {
    gis::domain::ModelAsset model; // model is the asset to add into the scene graph.
};

// AddModelToSceneResult reports whether insertion succeeded and where the node was stored.
// Upstream: AddModelToSceneUseCase fills it after validating the model.
// Downstream: UI can show errors or select the inserted node by nodeIndex.
struct AddModelToSceneResult {
    bool success = false;          // success is true when a node was appended to the scene.
    std::string errorMessage;      // errorMessage explains why insertion failed.
    std::size_t nodeIndex = 0;     // nodeIndex is valid only when success is true.
};

// AddModelToSceneUseCase appends a loaded model to the active SceneGraph.
// Upstream: UI or a higher-level workflow provides a loaded ModelAsset.
// Downstream: Renderer later consumes SceneGraph through its rendering port.
class AddModelToSceneUseCase {
public:
    // The constructor receives the scene graph that this use case mutates.
    // Upstream: composition code owns the application scene state.
    // Downstream: execute appends one SceneNode to that state.
    explicit AddModelToSceneUseCase(gis::domain::SceneGraph& sceneGraph);

    // execute validates model geometry and appends a matching SceneNode.
    // Upstream: callers pass AddModelToSceneRequest after loading a model.
    // Downstream: SceneGraph contains the node for future rendering.
    AddModelToSceneResult execute(const AddModelToSceneRequest& request);

private:
    gis::domain::SceneGraph& sceneGraph_; // sceneGraph_ is the active scene state.
};

} // namespace gis::application
