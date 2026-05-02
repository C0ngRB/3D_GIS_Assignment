#pragma once

// LoadBatchModelsUseCase.h defines the Application use case for folder-based model loading.
// Upstream: UI provides a folder path, extension list, and recursive flag.
// Downstream: the use case asks a repository for files and loads each model through IModelLoader.

#include <string>
#include <vector>

#include "domain/model/ModelAsset.h"
#include "domain/ports/IModelBatchRepository.h"
#include "domain/ports/IModelLoader.h"

namespace gis::application {

// LoadBatchModelsRequest carries model discovery and loading parameters.
// Upstream: UI creates it after the user selects a model folder.
// Downstream: LoadBatchModelsUseCase passes the folder query to IModelBatchRepository.
struct LoadBatchModelsRequest {
    std::string folderPath;                    // folderPath is the root folder to search.
    std::vector<std::string> extensions{".obj"}; // extensions are model file suffixes to include.
    bool recursive = false;                    // recursive controls whether subfolders are searched.
};

// ModelLoadFailure records one file that could not be loaded.
// Upstream: LoadBatchModelsUseCase creates it when IModelLoader throws.
// Downstream: UI can show per-file failure details without aborting the batch.
struct ModelLoadFailure {
    std::string filePath;      // filePath is the model file that failed.
    std::string errorMessage;  // errorMessage explains the failure for this file.
};

// LoadBatchModelsResult reports discovered files, successful models, and failures.
// Upstream: LoadBatchModelsUseCase fills it after scanning and loading.
// Downstream: UI adds successful models to SceneGraph and reports failure counts.
struct LoadBatchModelsResult {
    bool success = false;                             // success is true when at least one model loaded.
    std::string errorMessage;                         // errorMessage explains fatal or all-failed batches.
    std::vector<std::string> discoveredFilePaths;     // discoveredFilePaths are all candidate model files.
    std::vector<std::string> loadedFilePaths;         // loadedFilePaths match the successfully loaded models.
    std::vector<gis::domain::ModelAsset> models;      // models are successfully loaded assets.
    std::vector<ModelLoadFailure> failures;           // failures are non-fatal per-file errors.
};

// LoadBatchModelsUseCase orchestrates batch model discovery and loading.
// Upstream: UI provides a LoadBatchModelsRequest.
// Downstream: repository scans files, loader parses each model, result carries partial success.
class LoadBatchModelsUseCase {
public:
    // The constructor receives the discovery repository and model loading port.
    // Upstream: composition code wires infrastructure implementations into the use case.
    // Downstream: execute depends only on Domain interfaces.
    LoadBatchModelsUseCase(
        gis::domain::IModelBatchRepository& modelBatchRepository,
        gis::domain::IModelLoader& modelLoader);

    // execute scans for candidate models and loads them independently.
    // Upstream: callers provide folder, extensions, and recursive mode.
    // Downstream: callers receive successes and failures without uncaught exceptions.
    LoadBatchModelsResult execute(const LoadBatchModelsRequest& request) const;

private:
    gis::domain::IModelBatchRepository& modelBatchRepository_; // modelBatchRepository_ discovers candidate files.
    gis::domain::IModelLoader& modelLoader_;                   // modelLoader_ parses each discovered model file.
};

} // namespace gis::application
