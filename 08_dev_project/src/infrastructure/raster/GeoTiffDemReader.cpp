// GeoTiffDemReader.cpp implements DEM GeoTIFF reading in Infrastructure.
// Upstream: LoadTerrainUseCase requests TerrainRaster through IDemReader.
// Downstream: Domain and Application layers receive plain TerrainRaster data.

#include "infrastructure/raster/GeoTiffDemReader.h"

#include <cstddef>
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
// Upstream: GeoTiffDemReader::read passes the requested DEM path.
// Downstream: GdalDatasetHandle owns the opened dataset.
GdalDatasetHandle openDataset(const std::string& path)
{
    GDALAllRegister();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDatasetH dataset = GDALOpen(path.c_str(), GA_ReadOnly);
    CPLPopErrorHandler();
    if (dataset == nullptr) {
        throw std::runtime_error("Cannot open DEM GeoTIFF: " + path);
    }

    return GdalDatasetHandle(dataset);
}

// applyGeoTransform copies georeference metadata into TerrainRaster when available.
// Upstream: GeoTiffDemReader::read calls it after opening the dataset.
// Downstream: LoadTerrainUseCase can compare DEM and image extents.
void applyGeoTransform(GDALDatasetH dataset, gis::domain::TerrainRaster& raster)
{
    double transform[6] = {};
    if (GDALGetGeoTransform(dataset, transform) == CE_None) {
        raster.originX = transform[0];
        raster.pixelSizeX = transform[1];
        raster.originY = transform[3];
        raster.pixelSizeY = transform[5];
        raster.hasGeoTransform = true;
    }
}

// applyNoData copies the source no-data value into TerrainRaster when present.
// Upstream: GeoTiffDemReader::read calls it for band 1.
// Downstream: later terrain mesh construction can skip invalid elevations.
void applyNoData(GDALRasterBandH band, gis::domain::TerrainRaster& raster)
{
    int hasNoData = 0;
    const double noDataValue = GDALGetRasterNoDataValue(band, &hasNoData);
    if (hasNoData != 0) {
        raster.noDataValue = noDataValue;
        raster.hasNoDataValue = true;
    }
}

} // namespace

namespace gis::infrastructure {

// read opens a DEM GeoTIFF and reads band 1 into TerrainRaster::elevations.
// Upstream: LoadTerrainUseCase invokes this method through IDemReader.
// Downstream: Terrain mesh construction will use the returned elevation array.
gis::domain::TerrainRaster GeoTiffDemReader::read(const std::string& demPath)
{
    GdalDatasetHandle dataset = openDataset(demPath);

    gis::domain::TerrainRaster raster;
    raster.width = GDALGetRasterXSize(dataset.get());
    raster.height = GDALGetRasterYSize(dataset.get());

    if (raster.width <= 0 || raster.height <= 0) {
        throw std::runtime_error("DEM GeoTIFF has invalid dimensions: " + demPath);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset.get(), 1);
    if (band == nullptr) {
        throw std::runtime_error("DEM GeoTIFF has no band 1: " + demPath);
    }

    applyGeoTransform(dataset.get(), raster);
    applyNoData(band, raster);

    const auto pixelCount = static_cast<std::size_t>(raster.width) * static_cast<std::size_t>(raster.height);
    raster.elevations.resize(pixelCount);

    const CPLErr readResult = GDALRasterIO(
        band,
        GF_Read,
        0,
        0,
        raster.width,
        raster.height,
        raster.elevations.data(),
        raster.width,
        raster.height,
        GDT_Float32,
        0,
        0);

    if (readResult != CE_None) {
        throw std::runtime_error("Failed to read DEM elevation band: " + demPath);
    }

    return raster;
}

} // namespace gis::infrastructure
