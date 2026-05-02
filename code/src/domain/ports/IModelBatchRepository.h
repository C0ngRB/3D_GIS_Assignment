#pragma once

// IModelBatchRepository.h defines the batch model discovery port.
// Upstream: LoadBatchModelsUseCase depends on it to find OBJ files.
// Downstream: FileSystemModelBatchRepository implements directory traversal.

#include <string>
#include <vector>

namespace gis::domain {

// IModelBatchRepository separates file-system traversal from batch loading logic.
// Upstream: Application provides folder, extension list, and recursive mode.
// Downstream: Infrastructure returns candidate model paths.
class IModelBatchRepository {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: Application owns or references the port abstraction.
    // Downstream: concrete repositories release their own resources.
    virtual ~IModelBatchRepository() = default;

    // findModelFiles returns model paths matching extensions under a folder.
    // Upstream: UI-selected folder is passed by the batch loading use case.
    // Downstream: the returned paths are fed into IModelLoader.
    virtual std::vector<std::string> findModelFiles(
        const std::string& folderPath,
        const std::vector<std::string>& extensions,
        bool recursive) = 0;
};

} // namespace gis::domain
