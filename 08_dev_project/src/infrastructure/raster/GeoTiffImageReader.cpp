// GeoTiffImageReader.cpp implements RGB GeoTIFF reading in Infrastructure.
// Upstream: LoadTerrainUseCase requests TextureImage through IImageReader.
// Downstream: Domain and Application layers receive plain interleaved pixels.

#include "infrastructure/raster/GeoTiffImageReader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpl_error.h"
#include "gdal.h"

namespace {

// GdalDatasetHandle owns a GDAL dataset handle for exception-safe cleanup.
// Upstream: openDataset creates it from a path.
// Downstream: reader functions access the raw handle through get().
class GdalDatasetHandle {
public:
    // The constructor stores the opened dataset handle.
    // Upstream: openDataset passes a non-null GDALDatasetH.
    // Downstream: the destructor closes the same handle.
    explicit GdalDatasetHandle(GDALDatasetH dataset) : dataset_(dataset)
    {
    }

    // The destructor closes the GDAL dataset when leaving scope.
    // Upstream: read functions rely on RAII during normal and exceptional exits.
    // Downstream: GDAL releases file handles and internal buffers.
    ~GdalDatasetHandle()
    {
        if (dataset_ != nullptr) {
            GDALClose(dataset_);
        }
    }

    GdalDatasetHandle(const GdalDatasetHandle&) = delete;
    GdalDatasetHandle& operator=(const GdalDatasetHandle&) = delete;

    // get returns the raw GDAL dataset handle for C API calls.
    // Upstream: reader functions need access to dimensions, bands, and metadata.
    // Downstream: GDAL C API functions receive the handle.
    GDALDatasetH get() const
    {
        return dataset_;
    }

private:
    GDALDatasetH dataset_ = nullptr; // dataset_ is the owned GDAL dataset handle.
};

// openDataset opens a raster file for read-only access.
// Upstream: GeoTiffImageReader::read passes the requested image path.
// Downstream: GdalDatasetHandle owns the opened dataset.
GdalDatasetHandle openDataset(const std::string& path)
{
    GDALAllRegister();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDatasetH dataset = GDALOpen(path.c_str(), GA_ReadOnly);
    CPLPopErrorHandler();
    if (dataset == nullptr) {
        throw std::runtime_error("Cannot open image GeoTIFF: " + path);
    }

    return GdalDatasetHandle(dataset);
}

// applyGeoTransform copies georeference metadata into TextureImage when available.
// Upstream: GeoTiffImageReader::read calls it after opening the dataset.
// Downstream: LoadTerrainUseCase can compare DEM and image extents.
void applyGeoTransform(GDALDatasetH dataset, gis::domain::TextureImage& image)
{
    double transform[6] = {};
    if (GDALGetGeoTransform(dataset, transform) == CE_None) {
        image.originX = transform[0];
        image.pixelSizeX = transform[1];
        image.originY = transform[3];
        image.pixelSizeY = transform[5];
        image.hasGeoTransform = true;
    }
}

// copyBandToInterleavedPixels writes one GDAL band into an interleaved image buffer.
// Upstream: GeoTiffImageReader::read calls it once per output channel.
// Downstream: TextureImage::pixels stores RGB or grayscale bytes row-major.
void copyBandToInterleavedPixels(
    GDALRasterBandH band,
    int width,
    int height,
    int channels,
    int channelIndex,
    std::vector<std::uint8_t>& pixels)
{
    const auto pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> bandPixels(pixelCount);

    const CPLErr readResult = GDALRasterIO(
        band,
        GF_Read,
        0,
        0,
        width,
        height,
        bandPixels.data(),
        width,
        height,
        GDT_Byte,
        0,
        0);

    if (readResult != CE_None) {
        throw std::runtime_error("Failed to read image band.");
    }

    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        pixels[pixelIndex * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channelIndex)] = bandPixels[pixelIndex];
    }
}

} // namespace

namespace gis::infrastructure {

// read opens a GeoTIFF image and reads up to three bands into interleaved pixels.
// Upstream: LoadTerrainUseCase invokes this method through IImageReader.
// Downstream: terrain texturing and rendering use the returned TextureImage.
gis::domain::TextureImage GeoTiffImageReader::read(const std::string& imagePath)
{
    GdalDatasetHandle dataset = openDataset(imagePath);

    gis::domain::TextureImage image;
    image.width = GDALGetRasterXSize(dataset.get());
    image.height = GDALGetRasterYSize(dataset.get());

    if (image.width <= 0 || image.height <= 0) {
        throw std::runtime_error("Image GeoTIFF has invalid dimensions: " + imagePath);
    }

    const int bandCount = GDALGetRasterCount(dataset.get());
    if (bandCount <= 0) {
        throw std::runtime_error("Image GeoTIFF has no raster bands: " + imagePath);
    }

    image.channels = std::min(3, bandCount);
    applyGeoTransform(dataset.get(), image);

    const auto pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    image.pixels.assign(pixelCount * static_cast<std::size_t>(image.channels), 0U);

    for (int channelIndex = 0; channelIndex < image.channels; ++channelIndex) {
        GDALRasterBandH band = GDALGetRasterBand(dataset.get(), channelIndex + 1);
        if (band == nullptr) {
            throw std::runtime_error("Image GeoTIFF has a missing raster band: " + imagePath);
        }

        copyBandToInterleavedPixels(band, image.width, image.height, image.channels, channelIndex, image.pixels);
    }

    return image;
}

} // namespace gis::infrastructure
