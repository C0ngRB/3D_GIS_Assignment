#pragma once

// GeoTiffImageReader.h declares the Infrastructure adapter for RGB GeoTIFF files.
// Upstream: Application depends on the Domain IImageReader port.
// Downstream: GeoTiffImageReader reads image bands into TextureImage.

#include <string>

#include "domain/ports/IImageReader.h"
#include "domain/texture/TextureImage.h"

namespace gis::infrastructure {

// GeoTiffImageReader implements IImageReader for remote-sensing GeoTIFF images.
// Upstream: LoadTerrainUseCase invokes it through IImageReader.
// Downstream: TextureImage is returned without leaking library-specific types.
class GeoTiffImageReader final : public gis::domain::IImageReader {
public:
    // read opens an image GeoTIFF and reads up to three bands as interleaved pixels.
    // Upstream: Application passes an image path through IImageReader.
    // Downstream: terrain and rendering workflows consume the returned TextureImage.
    gis::domain::TextureImage read(const std::string& imagePath) override;
};

} // namespace gis::infrastructure
