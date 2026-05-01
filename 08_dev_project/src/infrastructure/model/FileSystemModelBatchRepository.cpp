// FileSystemModelBatchRepository.cpp implements local model file discovery.
// Upstream: LoadBatchModelsUseCase requests candidate files through IModelBatchRepository.
// Downstream: returned paths are fed into model loaders one by one.

#include "infrastructure/model/FileSystemModelBatchRepository.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

// toLowerCopy creates a lowercase copy for case-insensitive extension matching.
// Upstream: normalizeExtension and hasAllowedExtension pass extension text.
// Downstream: comparisons work consistently on Windows and other filesystems.
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

// normalizeExtension ensures extensions are lowercase and start with a dot.
// Upstream: normalizeExtensions passes user-provided extension filters.
// Downstream: hasAllowedExtension compares normalized file suffixes.
std::string normalizeExtension(const std::string& extension)
{
    if (extension.empty()) {
        return ".obj";
    }

    std::string normalized = toLowerCopy(extension);
    if (normalized.front() != '.') {
        normalized.insert(normalized.begin(), '.');
    }

    return normalized;
}

// normalizeExtensions returns a usable extension list with .obj as fallback.
// Upstream: findModelFiles passes the request extension list.
// Downstream: file discovery filters entries by the returned values.
std::vector<std::string> normalizeExtensions(const std::vector<std::string>& extensions)
{
    if (extensions.empty()) {
        return std::vector<std::string>{".obj"};
    }

    std::vector<std::string> normalizedExtensions;
    normalizedExtensions.reserve(extensions.size());
    for (const std::string& extension : extensions) {
        normalizedExtensions.push_back(normalizeExtension(extension));
    }

    return normalizedExtensions;
}

// hasAllowedExtension checks whether a filesystem path matches the extension filter.
// Upstream: appendModelFile passes each regular file path.
// Downstream: matching paths are returned to the batch loading use case.
bool hasAllowedExtension(const std::filesystem::path& path, const std::vector<std::string>& extensions)
{
    const std::string fileExtension = toLowerCopy(path.extension().string());
    return std::find(extensions.begin(), extensions.end(), fileExtension) != extensions.end();
}

// appendModelFile appends one matching regular file path.
// Upstream: scanFolder passes entries from directory iterators.
// Downstream: findModelFiles returns the accumulated path list.
void appendModelFile(
    const std::filesystem::directory_entry& entry,
    const std::vector<std::string>& extensions,
    std::vector<std::string>& modelFilePaths)
{
    std::error_code error;
    if (!entry.is_regular_file(error) || error) {
        return;
    }

    if (hasAllowedExtension(entry.path(), extensions)) {
        modelFilePaths.push_back(entry.path().string());
    }
}

// scanNonRecursive searches only the selected folder.
// Upstream: scanFolder chooses this path when recursive is false.
// Downstream: matching file paths are appended to modelFilePaths.
void scanNonRecursive(
    const std::filesystem::path& folderPath,
    const std::vector<std::string>& extensions,
    std::vector<std::string>& modelFilePaths)
{
    std::error_code iteratorError;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(folderPath, std::filesystem::directory_options::skip_permission_denied, iteratorError)) {
        appendModelFile(entry, extensions, modelFilePaths);
    }

    if (iteratorError) {
        throw std::runtime_error("Failed to scan model folder: " + iteratorError.message());
    }
}

// scanRecursive searches the selected folder and all subfolders.
// Upstream: scanFolder chooses this path when recursive is true.
// Downstream: matching file paths are appended to modelFilePaths.
void scanRecursive(
    const std::filesystem::path& folderPath,
    const std::vector<std::string>& extensions,
    std::vector<std::string>& modelFilePaths)
{
    std::error_code iteratorError;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(folderPath, std::filesystem::directory_options::skip_permission_denied, iteratorError)) {
        appendModelFile(entry, extensions, modelFilePaths);
    }

    if (iteratorError) {
        throw std::runtime_error("Failed to scan model folder recursively: " + iteratorError.message());
    }
}

// assertFolderExists validates that the selected path is a usable directory.
// Upstream: findModelFiles passes the requested folder path.
// Downstream: scan functions can safely create directory iterators.
void assertFolderExists(const std::filesystem::path& folderPath)
{
    std::error_code error;
    if (!std::filesystem::exists(folderPath, error) || error) {
        throw std::runtime_error("Model folder does not exist: " + folderPath.string());
    }

    if (!std::filesystem::is_directory(folderPath, error) || error) {
        throw std::runtime_error("Model path is not a folder: " + folderPath.string());
    }
}

} // namespace

namespace gis::infrastructure {

// findModelFiles returns matching model files in stable sorted order.
// Upstream: LoadBatchModelsUseCase requests model candidates.
// Downstream: IModelLoader loads each returned path independently.
std::vector<std::string> FileSystemModelBatchRepository::findModelFiles(
    const std::string& folderPath,
    const std::vector<std::string>& extensions,
    bool recursive)
{
    const std::filesystem::path rootFolder(folderPath);
    assertFolderExists(rootFolder);

    const std::vector<std::string> normalizedExtensions = normalizeExtensions(extensions);
    std::vector<std::string> modelFilePaths;

    if (recursive) {
        scanRecursive(rootFolder, normalizedExtensions, modelFilePaths);
    } else {
        scanNonRecursive(rootFolder, normalizedExtensions, modelFilePaths);
    }

    std::sort(modelFilePaths.begin(), modelFilePaths.end());
    return modelFilePaths;
}

} // namespace gis::infrastructure
