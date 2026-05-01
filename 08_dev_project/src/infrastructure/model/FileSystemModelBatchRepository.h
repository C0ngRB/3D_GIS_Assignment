#pragma once

// FileSystemModelBatchRepository.h declares the Infrastructure adapter for model file discovery.
// Upstream: LoadBatchModelsUseCase depends on IModelBatchRepository.
// Downstream: this adapter searches the local filesystem and returns candidate model paths.

#include <string>
#include <vector>

#include "domain/ports/IModelBatchRepository.h"

namespace gis::infrastructure {

// FileSystemModelBatchRepository implements model discovery using local folders.
// Upstream: LoadBatchModelsUseCase calls it through the Domain repository port.
// Downstream: discovered file paths are passed to IModelLoader.
class FileSystemModelBatchRepository final : public gis::domain::IModelBatchRepository {
public:
    // findModelFiles searches a folder for files with matching extensions.
    // Upstream: Application passes folder, extension list, and recursive flag.
    // Downstream: LoadBatchModelsUseCase loads each returned path independently.
    std::vector<std::string> findModelFiles(
        const std::string& folderPath,
        const std::vector<std::string>& extensions,
        bool recursive) override;
};

} // namespace gis::infrastructure
