#pragma once

// LoadSingleModelUseCase.h defines the Application use case for one model file.
// Upstream: UI or command-line entry provides a model path.
// Downstream: the use case calls IModelLoader and returns a safe result object.

#include <string>

#include "domain/model/ModelAsset.h"
#include "domain/ports/IModelLoader.h"

namespace gis::application {

// LoadModelRequest carries the input path for loading one model.
// Upstream: UI layer creates it after the user selects an OBJ file.
// Downstream: LoadSingleModelUseCase validates it before calling the model loader.
struct LoadModelRequest {
    std::string filePath; // filePath is the OBJ path selected by the caller.
};

// LoadModelResult wraps success, error, and model data for the UI layer.
// Upstream: LoadSingleModelUseCase fills it after attempting to load a model.
// Downstream: UI decides whether to show an error or add the model to the scene.
struct LoadModelResult {
    bool success = false;              // success is true only when model contains valid geometry.
    std::string errorMessage;          // errorMessage explains validation or loader failures.
    gis::domain::ModelAsset model;     // model is valid only when success is true.
};

// LoadSingleModelUseCase orchestrates single OBJ model loading.
// Upstream: UI passes a LoadModelRequest.
// Downstream: IModelLoader performs file-format parsing in Infrastructure.
class LoadSingleModelUseCase {
public:
    // The constructor receives the model loading port.
    // Upstream: composition code wires an infrastructure loader into the use case.
    // Downstream: execute uses the port without knowing the concrete file parser.
    explicit LoadSingleModelUseCase(gis::domain::IModelLoader& modelLoader);

    // execute validates the request, calls the loader, and catches loader failures.
    // Upstream: UI or tests provide LoadModelRequest.
    // Downstream: callers receive LoadModelResult instead of exceptions.
    LoadModelResult execute(const LoadModelRequest& request) const;

private:
    gis::domain::IModelLoader& modelLoader_; // modelLoader_ is the Domain port used by this use case.
};

} // namespace gis::application
