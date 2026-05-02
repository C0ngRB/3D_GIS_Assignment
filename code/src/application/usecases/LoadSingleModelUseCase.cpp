// LoadSingleModelUseCase.cpp implements single model loading orchestration.
// Upstream: UI layer creates requests and injects IModelLoader.
// Downstream: Infrastructure loaders do the file parsing, while UI receives result objects.

#include "application/usecases/LoadSingleModelUseCase.h"

#include <exception>

namespace gis::application {

// The constructor stores the model loading port for later execution.
// Upstream: composition code supplies an infrastructure implementation.
// Downstream: execute calls the port through the Domain abstraction.
LoadSingleModelUseCase::LoadSingleModelUseCase(gis::domain::IModelLoader& modelLoader)
    : modelLoader_(modelLoader)
{
}

// execute loads one model and converts all expected failures into LoadModelResult.
// Upstream: UI passes the selected file path in LoadModelRequest.
// Downstream: UI can add result.model to the scene only when success is true.
LoadModelResult LoadSingleModelUseCase::execute(const LoadModelRequest& request) const
{
    LoadModelResult result;

    if (request.filePath.empty()) {
        result.errorMessage = "Model file path is empty.";
        return result;
    }

    try {
        result.model = modelLoader_.load(request.filePath);
        result.success = true;
        return result;
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown error while loading model.";
        return result;
    }
}

} // namespace gis::application
