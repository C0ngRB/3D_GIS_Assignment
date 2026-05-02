// OsgbToObjConverter.cpp implements OSGB conversion through OpenSceneGraph osgconv.
// Upstream: Application requests conversion of selected OSGB tiles.
// Downstream: generated OBJ files are loaded by the existing OBJ model loader.

#include "infrastructure/model/OsgbToObjConverter.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

// getenvString reads an environment variable into a std::string.
// Upstream: resolveConverterExecutable checks THREEDGIS_OSGCONV.
// Downstream: users can configure osgconv without recompiling the project.
std::string getenvString(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::string();
    }
    return std::string(value);
}

// quoteCommandPath wraps a command argument in quotes for Windows paths with spaces.
// Upstream: buildConversionCommand passes executable and file paths.
// Downstream: std::system receives a command line that preserves path boundaries.
std::string quoteCommandPath(const std::string& value)
{
    return "\"" + value + "\"";
}

// sanitizeFileName replaces path-hostile characters in cache file names.
// Upstream: makeOutputObjPath passes the source tile path stem.
// Downstream: generated OBJ names remain valid on Windows filesystems.
std::string sanitizeFileName(std::string value)
{
    for (char& character : value) {
        const bool safe =
            (character >= '0' && character <= '9') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            character == '_' ||
            character == '-' ||
            character == '+';
        if (!safe) {
            character = '_';
        }
    }
    return value;
}

// resolveConverterExecutable chooses the converter command.
// Upstream: OsgbToObjConverter::convertToObj calls it before building the command.
// Downstream: std::system can run an explicit path, THREEDGIS_OSGCONV, or PATH lookup.
std::string resolveConverterExecutable(const std::string& configuredPath)
{
    if (!configuredPath.empty()) {
        return configuredPath;
    }

    const std::string environmentPath = getenvString("THREEDGIS_OSGCONV");
    if (!environmentPath.empty()) {
        return environmentPath;
    }

    const std::string userProfile = getenvString("USERPROFILE");
    if (!userProfile.empty()) {
        const std::vector<std::filesystem::path> candidatePaths{
            std::filesystem::path(userProfile) / "anaconda3" / "envs" / "three_gis_osg" / "Library" / "bin" / "osgconv.exe",
            std::filesystem::path(userProfile) / "anaconda3" / "Library" / "bin" / "osgconv.exe"
        };

        for (const std::filesystem::path& candidatePath : candidatePaths) {
            if (std::filesystem::exists(candidatePath)) {
                return candidatePath.string();
            }
        }
    }

    return "osgconv.exe";
}

// makeOutputObjPath creates a deterministic OBJ cache path for one OSGB tile.
// Upstream: OsgbToObjConverter::convertToObj passes source and cache paths.
// Downstream: existing conversion output can be overwritten or reused safely.
std::filesystem::path makeOutputObjPath(const std::string& osgbFilePath, const std::string& outputFolderPath)
{
    const std::filesystem::path inputPath(osgbFilePath);
    const std::string outputName = sanitizeFileName(inputPath.stem().string()) + ".obj";
    return std::filesystem::path(outputFolderPath) / outputName;
}

// buildConversionCommand creates the osgconv command line.
// Upstream: OsgbToObjConverter::convertToObj provides converter and file paths.
// Downstream: std::system executes the command.
std::string buildConversionCommand(
    const std::string& converterExecutable,
    const std::string& inputPath,
    const std::string& outputPath)
{
    std::ostringstream command;
    command << "cmd /C \""
            << quoteCommandPath(converterExecutable)
            << " "
            << quoteCommandPath(inputPath)
            << " "
            << quoteCommandPath(outputPath)
            << "\"";
    return command.str();
}

// makeConverterMissingMessage explains how to make OSGB import operational.
// Upstream: OsgbToObjConverter::convertToObj calls it when osgconv fails.
// Downstream: UI warnings tell the user exactly what dependency is missing.
std::string makeConverterMissingMessage(const std::string& converterExecutable)
{
    return "OSGB conversion failed using '" + converterExecutable +
        "'. Install OpenSceneGraph osgconv.exe, add it to PATH, or set THREEDGIS_OSGCONV to the full osgconv.exe path.";
}

} // namespace

namespace gis::infrastructure {

// The constructor stores an optional osgconv executable path.
// Upstream: UI composition may pass an explicit path later.
// Downstream: convertToObj resolves the final executable path at runtime.
OsgbToObjConverter::OsgbToObjConverter(std::string converterExecutablePath)
    : converterExecutablePath_(std::move(converterExecutablePath))
{
}

// convertToObj creates the cache folder, invokes osgconv, and verifies the OBJ output.
// Upstream: LoadOsgbTilesUseCase passes one OSGB tile at a time.
// Downstream: ObjModelLoader consumes the returned OBJ path.
gis::domain::OsgbConversionResult OsgbToObjConverter::convertToObj(const gis::domain::OsgbConversionRequest& request)
{
    gis::domain::OsgbConversionResult result;

    if (request.osgbFilePath.empty()) {
        result.errorMessage = "OSGB file path is empty.";
        return result;
    }
    if (!std::filesystem::exists(request.osgbFilePath)) {
        result.errorMessage = "OSGB file does not exist: " + request.osgbFilePath;
        return result;
    }

    std::error_code errorCode;
    std::filesystem::create_directories(request.outputFolderPath, errorCode);
    if (errorCode) {
        result.errorMessage = "Cannot create OSGB conversion cache folder: " + request.outputFolderPath;
        return result;
    }

    const std::filesystem::path outputObjPath = makeOutputObjPath(request.osgbFilePath, request.outputFolderPath);
    const std::string converterExecutable = resolveConverterExecutable(converterExecutablePath_);
    const std::string command = buildConversionCommand(converterExecutable, request.osgbFilePath, outputObjPath.string());

    const int exitCode = std::system(command.c_str());
    if (exitCode != 0 || !std::filesystem::exists(outputObjPath)) {
        result.errorMessage = makeConverterMissingMessage(converterExecutable);
        return result;
    }

    result.success = true;
    result.outputObjPath = outputObjPath.string();
    return result;
}

} // namespace gis::infrastructure
