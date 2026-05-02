#pragma once

// LoadOsgbTilesUseCase.h defines the Application use case for OSGB tile import.
// Upstream: UI provides an OSGB folder, cache folder, recursive flag, and tile limit.
// Downstream: the use case discovers OSGB files, converts selected tiles to OBJ, and loads models.

#include <cstddef>
#include <string>
#include <vector>

#include "application/usecases/LoadBatchModelsUseCase.h"
#include "domain/model/ModelAsset.h"
#include "domain/ports/IModelBatchRepository.h"
#include "domain/ports/IModelLoader.h"
#include "domain/ports/IOsgbConverter.h"

namespace gis::application {

// LoadOsgbTilesRequest carries OSGB folder import settings.
// Upstream: UI creates it when the user selects a Songqing OSGB folder.
// Downstream: LoadOsgbTilesUseCase scans, limits, converts, and loads model tiles.
struct LoadOsgbTilesRequest {
    std::string folderPath;                 // folderPath is the root folder containing .osgb tiles.
    std::string conversionCacheFolderPath;  // conversionCacheFolderPath stores generated OBJ files.
    bool recursive = true;                  // recursive controls whether child tile folders are scanned.
    std::size_t maxTiles = 4;               // maxTiles limits import size for responsive UI operation.
};

// LoadOsgbTilesResult reports discovered, selected, converted, loaded, and failed tiles.
// Upstream: LoadOsgbTilesUseCase fills it after the OSGB import workflow.
// Downstream: UI inserts models into SceneGraph and displays warnings.
struct LoadOsgbTilesResult {
    bool success = false;                         // success is true when at least one tile model loaded.
    std::string errorMessage;                     // errorMessage explains fatal or all-failed imports.
    std::vector<std::string> discoveredFilePaths; // discoveredFilePaths are all .osgb tiles found.
    std::vector<std::string> selectedFilePaths;   // selectedFilePaths are the bounded tiles attempted.
    std::vector<std::string> convertedObjPaths;   // convertedObjPaths are generated OBJ paths.
    std::vector<gis::domain::ModelAsset> models;  // models are successfully loaded converted tiles.
    std::vector<ModelLoadFailure> failures;       // failures record conversion or OBJ loading errors.
};

// LoadOsgbTilesUseCase orchestrates folder-based OSGB import through conversion.
// Upstream: UI provides folder and bounded import settings.
// Downstream: repository scans files, converter produces OBJ, loader parses each OBJ model.
class LoadOsgbTilesUseCase {
public:
    // The constructor receives discovery, conversion, and loading ports.
    // Upstream: composition code wires infrastructure implementations into the use case.
    // Downstream: execute depends only on Domain interfaces.
    LoadOsgbTilesUseCase(
        gis::domain::IModelBatchRepository& modelBatchRepository,
        gis::domain::IOsgbConverter& osgbConverter,
        gis::domain::IModelLoader& modelLoader);

    // execute scans OSGB files, chooses representative tiles, converts them, and loads them.
    // Upstream: callers pass LoadOsgbTilesRequest.
    // Downstream: callers receive loaded models and per-tile failures without uncaught exceptions.
    LoadOsgbTilesResult execute(const LoadOsgbTilesRequest& request) const;

private:
    gis::domain::IModelBatchRepository& modelBatchRepository_; // modelBatchRepository_ discovers .osgb tile files.
    gis::domain::IOsgbConverter& osgbConverter_;               // osgbConverter_ converts each selected tile to OBJ.
    gis::domain::IModelLoader& modelLoader_;                   // modelLoader_ parses converted OBJ files.
};

} // namespace gis::application
