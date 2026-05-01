#pragma once

// IModelLoader.h defines the model-loading port.
// Upstream: Application model-loading use cases depend on this interface.
// Downstream: Infrastructure ObjModelLoader implements this interface.

#include <string>

#include "domain/model/ModelAsset.h"

namespace gis::domain {

// IModelLoader separates model parsing from business use cases.
// Upstream: LoadSingleModelUseCase or LoadBatchModelsUseCase calls load.
// Downstream: ObjModelLoader reads OBJ data and returns ModelAsset.
class IModelLoader {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: Application owns or references the port abstraction.
    // Downstream: concrete loaders release their own resources.
    virtual ~IModelLoader() = default;

    // load reads one model asset from a path.
    // Upstream: UI-selected path is passed through an Application use case.
    // Downstream: Infrastructure parses the file and produces ModelAsset.
    virtual ModelAsset load(const std::string& filePath) = 0;
};

} // namespace gis::domain
