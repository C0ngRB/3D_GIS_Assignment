// LoadBatchModelsUseCase.cpp implements folder-based model loading orchestration.
// Upstream: UI provides a folder path and loading options.
// Downstream: successful ModelAsset values can be added to SceneGraph.

#include "application/usecases/LoadBatchModelsUseCase.h"

#include <exception>

namespace gis::application {

// The constructor stores discovery and loading ports.
// Upstream: composition code passes infrastructure adapters.
// Downstream: execute uses only the Domain abstractions.
LoadBatchModelsUseCase::LoadBatchModelsUseCase(
    gis::domain::IModelBatchRepository& modelBatchRepository,
    gis::domain::IModelLoader& modelLoader)
    : modelBatchRepository_(modelBatchRepository), modelLoader_(modelLoader)
{
}

// execute discovers model files and loads each one independently.
// Upstream: callers provide folder, extension list, and recursive mode.
// Downstream: partial failures are returned in result.failures instead of aborting the batch.
LoadBatchModelsResult LoadBatchModelsUseCase::execute(const LoadBatchModelsRequest& request) const
{
    LoadBatchModelsResult result;

    if (request.folderPath.empty()) {
        result.errorMessage = "Model folder path is empty.";
        return result;
    }

    try {
        result.discoveredFilePaths = modelBatchRepository_.findModelFiles(
            request.folderPath,
            request.extensions,
            request.recursive);
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown error while discovering batch model files.";
        return result;
    }

    if (result.discoveredFilePaths.empty()) {
        result.errorMessage = "No model files were found in the selected folder.";
        return result;
    }

    for (const std::string& filePath : result.discoveredFilePaths) {
        try {
            result.models.push_back(modelLoader_.load(filePath));
            result.loadedFilePaths.push_back(filePath);
        } catch (const std::exception& error) {
            result.failures.push_back(ModelLoadFailure{filePath, error.what()});
        } catch (...) {
            result.failures.push_back(ModelLoadFailure{filePath, "Unknown error while loading model."});
        }
    }

    result.success = !result.models.empty();
    if (!result.success) {
        result.errorMessage = "No models were loaded successfully.";
    }

    return result;
}

} // namespace gis::application
