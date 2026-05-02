#pragma once

// IOsgbConverter.h defines the Domain port for converting OSGB tiles to OBJ models.
// Upstream: Application OSGB loading use cases request conversion through this port.
// Downstream: Infrastructure can use OpenSceneGraph osgconv or another converter.

#include <string>

namespace gis::domain {

// OsgbConversionRequest carries one OSGB tile and one cache folder.
// Upstream: LoadOsgbTilesUseCase creates it for each selected tile.
// Downstream: IOsgbConverter implementations create an OBJ file for ObjModelLoader.
struct OsgbConversionRequest {
    std::string osgbFilePath;      // osgbFilePath is the source OpenSceneGraph binary tile.
    std::string outputFolderPath;  // outputFolderPath is the cache folder for converted OBJ files.
};

// OsgbConversionResult reports whether one OSGB tile became an OBJ file.
// Upstream: IOsgbConverter implementations fill it after invoking their converter.
// Downstream: LoadOsgbTilesUseCase loads outputObjPath when success is true.
struct OsgbConversionResult {
    bool success = false;        // success is true when outputObjPath exists and can be loaded next.
    std::string errorMessage;    // errorMessage explains converter discovery or execution failure.
    std::string outputObjPath;   // outputObjPath is the converted OBJ file path.
};

// IOsgbConverter converts OSGB tiles into OBJ files without exposing tooling to Application.
// Upstream: Application depends on this stable port.
// Downstream: Infrastructure owns process execution and filesystem cache details.
class IOsgbConverter {
public:
    // The virtual destructor allows deleting concrete converters through this port.
    // Upstream: UI composition owns converter implementations.
    // Downstream: no manual cleanup leaks through the interface.
    virtual ~IOsgbConverter() = default;

    // convertToObj converts one OSGB tile into one OBJ file.
    // Upstream: LoadOsgbTilesUseCase passes source tile and cache folder.
    // Downstream: callers receive outputObjPath or a clear failure message.
    virtual OsgbConversionResult convertToObj(const OsgbConversionRequest& request) = 0;
};

} // namespace gis::domain
