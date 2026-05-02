#pragma once

// OsgbToObjConverter.h declares the Infrastructure adapter for OSGB-to-OBJ conversion.
// Upstream: LoadOsgbTilesUseCase calls it through the IOsgbConverter port.
// Downstream: OpenSceneGraph osgconv performs binary OSGB parsing outside this project.

#include <string>

#include "domain/ports/IOsgbConverter.h"

namespace gis::infrastructure {

// OsgbToObjConverter converts OSGB tiles by invoking OpenSceneGraph osgconv.
// Upstream: Application asks for conversion through IOsgbConverter.
// Downstream: generated OBJ files are consumed by ObjModelLoader.
class OsgbToObjConverter final : public gis::domain::IOsgbConverter {
public:
    // The constructor stores an optional converter executable path.
    // Upstream: UI composition can pass an explicit osgconv path or leave it empty.
    // Downstream: convertToObj resolves THREEDGIS_OSGCONV or falls back to osgconv.exe.
    explicit OsgbToObjConverter(std::string converterExecutablePath = "");

    // convertToObj invokes osgconv for one OSGB tile and returns the output OBJ path.
    // Upstream: LoadOsgbTilesUseCase provides source tile and cache folder.
    // Downstream: ObjModelLoader loads the generated OBJ when conversion succeeds.
    gis::domain::OsgbConversionResult convertToObj(const gis::domain::OsgbConversionRequest& request) override;

private:
    std::string converterExecutablePath_; // converterExecutablePath_ is the optional osgconv executable path.
};

} // namespace gis::infrastructure
