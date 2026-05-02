// LoadOsgbTilesUseCase.cpp implements OSGB tile import orchestration.
// Upstream: UI provides the OSGB folder and cache settings.
// Downstream: converted OBJ model assets can be inserted into SceneGraph.

#include "application/usecases/LoadOsgbTilesUseCase.h"

#include <algorithm>
#include <exception>

namespace {

// sortByOverviewPriority places coarse OSGB tiles before deep LOD children.
// Upstream: selectRepresentativeTiles passes all discovered .osgb files.
// Downstream: imports remain small and representative instead of loading thousands of leaf tiles.
void sortByOverviewPriority(std::vector<std::string>& filePaths)
{
    std::sort(filePaths.begin(), filePaths.end(), [](const std::string& left, const std::string& right) {
        if (left.size() != right.size()) {
            return left.size() < right.size();
        }
        return left < right;
    });
}

// selectRepresentativeTiles limits import to a stable set of coarse tiles.
// Upstream: LoadOsgbTilesUseCase::execute passes discovered files and request.maxTiles.
// Downstream: converter receives a small enough workload for the desktop UI.
std::vector<std::string> selectRepresentativeTiles(std::vector<std::string> filePaths, std::size_t maxTiles)
{
    sortByOverviewPriority(filePaths);
    if (maxTiles > 0 && filePaths.size() > maxTiles) {
        filePaths.resize(maxTiles);
    }
    return filePaths;
}

} // namespace

namespace gis::application {

// The constructor stores discovery, conversion, and loading ports.
// Upstream: composition code passes infrastructure adapters.
// Downstream: execute uses only stable Domain abstractions.
LoadOsgbTilesUseCase::LoadOsgbTilesUseCase(
    gis::domain::IModelBatchRepository& modelBatchRepository,
    gis::domain::IOsgbConverter& osgbConverter,
    gis::domain::IModelLoader& modelLoader)
    : modelBatchRepository_(modelBatchRepository),
      osgbConverter_(osgbConverter),
      modelLoader_(modelLoader)
{
}

// execute discovers OSGB files, converts selected tiles, and loads converted OBJ models.
// Upstream: callers provide folder path, cache folder, recursive flag, and max tile count.
// Downstream: partial conversion or loading failures remain reportable to the UI.
LoadOsgbTilesResult LoadOsgbTilesUseCase::execute(const LoadOsgbTilesRequest& request) const
{
    LoadOsgbTilesResult result;

    if (request.folderPath.empty()) {
        result.errorMessage = "OSGB folder path is empty.";
        return result;
    }
    if (request.conversionCacheFolderPath.empty()) {
        result.errorMessage = "OSGB conversion cache folder path is empty.";
        return result;
    }

    try {
        result.discoveredFilePaths = modelBatchRepository_.findModelFiles(
            request.folderPath,
            std::vector<std::string>{".osgb"},
            request.recursive);
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown error while discovering OSGB tile files.";
        return result;
    }

    if (result.discoveredFilePaths.empty()) {
        result.errorMessage = "No OSGB files were found in the selected folder.";
        return result;
    }

    result.selectedFilePaths = selectRepresentativeTiles(result.discoveredFilePaths, request.maxTiles);
    for (const std::string& osgbFilePath : result.selectedFilePaths) {
        try {
            gis::domain::OsgbConversionRequest conversionRequest;
            conversionRequest.osgbFilePath = osgbFilePath;
            conversionRequest.outputFolderPath = request.conversionCacheFolderPath;

            const gis::domain::OsgbConversionResult conversionResult = osgbConverter_.convertToObj(conversionRequest);
            if (!conversionResult.success) {
                result.failures.push_back(ModelLoadFailure{osgbFilePath, conversionResult.errorMessage});
                continue;
            }

            result.convertedObjPaths.push_back(conversionResult.outputObjPath);
            result.models.push_back(modelLoader_.load(conversionResult.outputObjPath));
        } catch (const std::exception& error) {
            result.failures.push_back(ModelLoadFailure{osgbFilePath, error.what()});
        } catch (...) {
            result.failures.push_back(ModelLoadFailure{osgbFilePath, "Unknown error while importing OSGB tile."});
        }
    }

    result.success = !result.models.empty();
    if (!result.success) {
        result.errorMessage = "No OSGB tiles were imported successfully.";
    }

    return result;
}

} // namespace gis::application
