// LoadTerrainUseCase.cpp implements DEM and image loading orchestration.
// Upstream: UI layer creates requests and injects raster reader ports.
// Downstream: later terrain mesh construction receives validated raster data.

#include "application/usecases/LoadTerrainUseCase.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <sstream>

namespace {

// RasterExtent stores a normalized bounding box for coarse extent comparison.
// Upstream: makeDemExtent and makeImageExtent derive it from Domain raster metadata.
// Downstream: appendExtentWarningIfNeeded compares DEM and image coverage.
struct RasterExtent {
    double minX = 0.0; // minX is the smaller horizontal coordinate.
    double maxX = 0.0; // maxX is the larger horizontal coordinate.
    double minY = 0.0; // minY is the smaller vertical coordinate.
    double maxY = 0.0; // maxY is the larger vertical coordinate.
};

// makeExtent normalizes an origin, pixel size, and raster dimensions into bounds.
// Upstream: DEM and image metadata supply origin and pixel sizes.
// Downstream: extent comparison uses the normalized min/max coordinates.
RasterExtent makeExtent(double originX, double originY, double pixelSizeX, double pixelSizeY, int width, int height)
{
    const double endX = originX + pixelSizeX * static_cast<double>(width);
    const double endY = originY + pixelSizeY * static_cast<double>(height);

    RasterExtent extent;
    extent.minX = std::min(originX, endX);
    extent.maxX = std::max(originX, endX);
    extent.minY = std::min(originY, endY);
    extent.maxY = std::max(originY, endY);
    return extent;
}

// relativeDifference compares two scalar values with a stable denominator.
// Upstream: appendExtentWarningIfNeeded passes matching extent coordinates.
// Downstream: the returned ratio decides whether to emit a warning.
double relativeDifference(double left, double right)
{
    const double denominator = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) / denominator;
}

// appendExtentWarningIfNeeded warns when DEM and image georeference bounds differ noticeably.
// Upstream: LoadTerrainUseCase calls this after both rasters are loaded.
// Downstream: the UI may display warnings while still allowing ordinary texture use.
void appendExtentWarningIfNeeded(
    const gis::domain::TerrainRaster& dem,
    const gis::domain::TextureImage& image,
    std::vector<std::string>& warnings)
{
    if (!dem.hasGeoTransform || !image.hasGeoTransform) {
        warnings.push_back("DEM or image lacks georeference metadata; image will be used as a plain texture.");
        return;
    }

    const RasterExtent demExtent = makeExtent(dem.originX, dem.originY, dem.pixelSizeX, dem.pixelSizeY, dem.width, dem.height);
    const RasterExtent imageExtent = makeExtent(image.originX, image.originY, image.pixelSizeX, image.pixelSizeY, image.width, image.height);

    const double tolerance = 0.005;
    const bool differs =
        relativeDifference(demExtent.minX, imageExtent.minX) > tolerance ||
        relativeDifference(demExtent.maxX, imageExtent.maxX) > tolerance ||
        relativeDifference(demExtent.minY, imageExtent.minY) > tolerance ||
        relativeDifference(demExtent.maxY, imageExtent.maxY) > tolerance;

    if (differs) {
        std::ostringstream message;
        message << "DEM and image extents are not closely aligned; later texture mapping may need resampling.";
        warnings.push_back(message.str());
    }
}

// isValidDem checks basic DEM dimensions and elevation storage.
// Upstream: LoadTerrainUseCase calls it after IDemReader returns.
// Downstream: success is allowed only when storage matches dimensions.
bool isValidDem(const gis::domain::TerrainRaster& dem)
{
    if (dem.width <= 0 || dem.height <= 0) {
        return false;
    }

    const auto expectedSize = static_cast<std::size_t>(dem.width) * static_cast<std::size_t>(dem.height);
    return dem.elevations.size() == expectedSize;
}

// isValidImage checks basic image dimensions, channels, and pixel storage.
// Upstream: LoadTerrainUseCase calls it after IImageReader returns.
// Downstream: success is allowed only when storage matches dimensions.
bool isValidImage(const gis::domain::TextureImage& image)
{
    if (image.width <= 0 || image.height <= 0 || image.channels <= 0) {
        return false;
    }

    const auto expectedSize =
        static_cast<std::size_t>(image.width) *
        static_cast<std::size_t>(image.height) *
        static_cast<std::size_t>(image.channels);
    return image.pixels.size() == expectedSize;
}

} // namespace

namespace gis::application {

// The constructor stores the DEM and image reader ports.
// Upstream: composition code supplies infrastructure reader implementations.
// Downstream: execute invokes both readers through Domain abstractions.
LoadTerrainUseCase::LoadTerrainUseCase(gis::domain::IDemReader& demReader, gis::domain::IImageReader& imageReader)
    : demReader_(demReader), imageReader_(imageReader)
{
}

// execute loads both rasters and converts expected failures into LoadTerrainResult.
// Upstream: UI passes paths selected by the user.
// Downstream: later terrain mesh use cases consume the returned dem and image.
LoadTerrainResult LoadTerrainUseCase::execute(const LoadTerrainRequest& request) const
{
    LoadTerrainResult result;

    if (request.demPath.empty()) {
        result.errorMessage = "DEM file path is empty.";
        return result;
    }

    if (request.imagePath.empty()) {
        result.errorMessage = "Image file path is empty.";
        return result;
    }

    try {
        result.dem = demReader_.read(request.demPath);
        result.image = imageReader_.read(request.imagePath);

        if (!isValidDem(result.dem)) {
            result.errorMessage = "DEM data is empty or inconsistent with its dimensions.";
            return result;
        }

        if (!isValidImage(result.image)) {
            result.errorMessage = "Image data is empty or inconsistent with its dimensions.";
            return result;
        }

        appendExtentWarningIfNeeded(result.dem, result.image, result.warnings);
        result.success = true;
        return result;
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown error while loading terrain data.";
        return result;
    }
}

} // namespace gis::application
