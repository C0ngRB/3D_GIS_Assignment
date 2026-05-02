#pragma once

// IImageReader.h defines the remote-sensing image-reading port.
// Upstream: LoadTerrainUseCase depends on this interface for terrain texture.
// Downstream: GeoTiffImageReader implements it in the infrastructure layer later.

#include <string>

#include "domain/texture/TextureImage.h"

namespace gis::domain {

// IImageReader separates image file access from terrain texture flow.
// Upstream: Application asks for RGB image data.
// Downstream: Infrastructure returns TextureImage.
class IImageReader {
public:
    // The virtual destructor allows safe cleanup through the interface.
    // Upstream: Application owns or references the port abstraction.
    // Downstream: concrete image readers release their own resources.
    virtual ~IImageReader() = default;

    // read loads texture image data from a path.
    // Upstream: UI-selected imagePath is passed by LoadTerrainUseCase.
    // Downstream: GeoTiffImageReader produces TextureImage.
    virtual TextureImage read(const std::string& imagePath) = 0;
};

} // namespace gis::domain
