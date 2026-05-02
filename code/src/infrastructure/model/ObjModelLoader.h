#pragma once

// ObjModelLoader.h declares the Infrastructure adapter for OBJ model files.
// Upstream: Application depends on the Domain IModelLoader port.
// Downstream: ObjModelLoader parses OBJ text and returns Domain ModelAsset data.

#include <string>

#include "domain/model/ModelAsset.h"
#include "domain/ports/IModelLoader.h"

namespace gis::infrastructure {

// ObjModelLoader implements IModelLoader for a minimal OBJ subset.
// Upstream: LoadSingleModelUseCase invokes it through IModelLoader.
// Downstream: Domain receives ModelAsset without depending on file-system APIs.
class ObjModelLoader final : public gis::domain::IModelLoader {
public:
    // load reads vertices, optional texture coordinates/normals, and faces from OBJ.
    // Upstream: Application passes a file path through IModelLoader.
    // Downstream: ModelAsset is added to SceneGraph by AddModelToSceneUseCase.
    gis::domain::ModelAsset load(const std::string& filePath) override;
};

} // namespace gis::infrastructure
