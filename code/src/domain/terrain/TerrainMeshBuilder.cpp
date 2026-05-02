// TerrainMeshBuilder.cpp implements DEM grid triangulation.
// Upstream: Application layer passes loaded TerrainRaster and TextureImage.
// Downstream: TerrainMesh is ready for scene insertion and rendering adapters.

#include "domain/terrain/TerrainMeshBuilder.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "domain/geometry/NormalCalculator.h"
#include "domain/geometry/TextureCoordinateCalculator.h"

namespace {

// validateInput checks DEM/image storage and mesh build parameters.
// Upstream: TerrainMeshBuilder::build calls it before allocating mesh data.
// Downstream: later code can assume dimensions and buffers are consistent.
void validateInput(
    const gis::domain::TerrainRaster& dem,
    const gis::domain::TextureImage& image,
    int samplingStep,
    float verticalScale)
{
    if (dem.width < 2 || dem.height < 2) {
        throw std::runtime_error("DEM must contain at least two rows and two columns.");
    }

    const auto expectedElevationCount = static_cast<std::size_t>(dem.width) * static_cast<std::size_t>(dem.height);
    if (dem.elevations.size() != expectedElevationCount) {
        throw std::runtime_error("DEM elevation count does not match DEM dimensions.");
    }

    if (image.width <= 0 || image.height <= 0 || image.channels <= 0 || image.pixels.empty()) {
        throw std::runtime_error("Texture image is empty or invalid.");
    }

    if (samplingStep <= 0) {
        throw std::runtime_error("Terrain sampling step must be greater than zero.");
    }

    if (verticalScale <= 0.0F) {
        throw std::runtime_error("Terrain vertical scale must be greater than zero.");
    }
}

// buildSampleIndices creates a sampled axis and always includes the final source index.
// Upstream: TerrainMeshBuilder::build passes DEM width or height and sampling step.
// Downstream: mesh construction uses the returned source column or row indices.
std::vector<int> buildSampleIndices(int sourceSize, int samplingStep)
{
    std::vector<int> indices;
    for (int index = 0; index < sourceSize; index += samplingStep) {
        indices.push_back(index);
    }

    if (indices.empty() || indices.back() != sourceSize - 1) {
        indices.push_back(sourceSize - 1);
    }

    return indices;
}

// isNoData returns true when an elevation matches the source no-data marker.
// Upstream: elevationAt passes raw DEM elevation values.
// Downstream: invalid DEM cells are flattened to a safe elevation.
bool isNoData(float elevation, const gis::domain::TerrainRaster& dem)
{
    if (!dem.hasNoDataValue) {
        return false;
    }

    return std::abs(static_cast<double>(elevation) - dem.noDataValue) <= 0.001;
}

// elevationAt returns a safe scaled elevation for one DEM row and column.
// Upstream: appendVertices samples DEM cells by row and column.
// Downstream: Vertex::position.z receives the scaled height.
float elevationAt(const gis::domain::TerrainRaster& dem, int row, int column, float verticalScale)
{
    const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(dem.width) + static_cast<std::size_t>(column);
    const float elevation = dem.elevations[index];
    if (isNoData(elevation, dem)) {
        return 0.0F;
    }

    return elevation * verticalScale;
}

// appearsGeographicPixelSize detects degree-based GeoTIFF cell sizes.
// Upstream: terrainCellSizeMeters passes DEM geotransform metadata.
// Downstream: degree rasters are converted to meters before mesh construction.
bool appearsGeographicPixelSize(const gis::domain::TerrainRaster& dem)
{
    return std::abs(dem.pixelSizeX) > 0.0 && std::abs(dem.pixelSizeX) < 1.0 &&
           std::abs(dem.pixelSizeY) > 0.0 && std::abs(dem.pixelSizeY) < 1.0;
}

// terrainCellSizeMeters returns horizontal cell size in display meters.
// Upstream: appendVertices passes DEM metadata.
// Downstream: vertex X/Y use the same meter-like unit as elevation Z.
std::pair<float, float> terrainCellSizeMeters(const gis::domain::TerrainRaster& dem)
{
    const float rawCellSizeX = static_cast<float>(std::abs(dem.pixelSizeX));
    const float rawCellSizeY = static_cast<float>(std::abs(dem.pixelSizeY));
    if (!dem.hasGeoTransform || !appearsGeographicPixelSize(dem)) {
        return std::make_pair(std::max(0.0001F, rawCellSizeX), std::max(0.0001F, rawCellSizeY));
    }

    const double centerLatitude = dem.originY + dem.pixelSizeY * static_cast<double>(dem.height) * 0.5;
    const double latitudeRadians = centerLatitude * 3.14159265358979323846 / 180.0;
    const double metersPerDegreeLatitude = 111320.0;
    const double metersPerDegreeLongitude = metersPerDegreeLatitude * std::max(0.15, std::cos(latitudeRadians));
    return std::make_pair(
        static_cast<float>(std::abs(dem.pixelSizeX) * metersPerDegreeLongitude),
        static_cast<float>(std::abs(dem.pixelSizeY) * metersPerDegreeLatitude));
}

// appendVertices creates terrain vertices from sampled DEM rows and columns.
// Upstream: TerrainMeshBuilder::build provides sample axes and terrain parameters.
// Downstream: triangle construction indexes the generated vertex grid.
void appendVertices(
    const gis::domain::TerrainRaster& dem,
    const std::vector<int>& sampledRows,
    const std::vector<int>& sampledColumns,
    float verticalScale,
    gis::domain::Mesh& mesh)
{
    mesh.vertices.reserve(sampledRows.size() * sampledColumns.size());

    const std::pair<float, float> cellSize = terrainCellSizeMeters(dem);
    const float cellSizeX = cellSize.first;
    const float cellSizeY = cellSize.second;

    for (const int row : sampledRows) {
        for (const int column : sampledColumns) {
            gis::domain::Vertex vertex;
            vertex.position.x = static_cast<float>(column) * cellSizeX;
            vertex.position.y = static_cast<float>(row) * cellSizeY;
            vertex.position.z = elevationAt(dem, row, column, verticalScale);
            mesh.vertices.push_back(vertex);
        }
    }
}

// vertexIndex converts a sampled row/column pair to a linear vertex index.
// Upstream: appendTriangles loops over sampled grid cells.
// Downstream: Triangle stores the returned index values.
std::size_t vertexIndex(std::size_t row, std::size_t column, std::size_t columnCount)
{
    return row * columnCount + column;
}

// appendTriangles creates two triangles for every sampled DEM grid cell.
// Upstream: TerrainMeshBuilder::build calls it after all vertices exist.
// Downstream: NormalCalculator and renderers use the generated triangle indices.
void appendTriangles(std::size_t rowCount, std::size_t columnCount, gis::domain::Mesh& mesh)
{
    if (rowCount < 2U || columnCount < 2U) {
        throw std::runtime_error("Sampled terrain grid is too small to triangulate.");
    }

    mesh.triangles.reserve((rowCount - 1U) * (columnCount - 1U) * 2U);

    for (std::size_t row = 0; row + 1U < rowCount; ++row) {
        for (std::size_t column = 0; column + 1U < columnCount; ++column) {
            const std::size_t v00 = vertexIndex(row, column, columnCount);
            const std::size_t v10 = vertexIndex(row, column + 1U, columnCount);
            const std::size_t v01 = vertexIndex(row + 1U, column, columnCount);
            const std::size_t v11 = vertexIndex(row + 1U, column + 1U, columnCount);

            mesh.triangles.push_back(gis::domain::Triangle{v00, v10, v01});
            mesh.triangles.push_back(gis::domain::Triangle{v10, v11, v01});
        }
    }
}

} // namespace

namespace gis::domain {

// build converts DEM samples into a textured terrain mesh.
// Upstream: BuildTerrainMeshUseCase supplies DEM, image, sampling step, and vertical scale.
// Downstream: AddTerrainToSceneUseCase can append the returned mesh to SceneGraph.
TerrainMesh TerrainMeshBuilder::build(
    const TerrainRaster& dem,
    const TextureImage& image,
    int samplingStep,
    float verticalScale) const
{
    validateInput(dem, image, samplingStep, verticalScale);

    TerrainMesh terrainMesh;
    terrainMesh.texture = image;

    const std::vector<int> sampledRows = buildSampleIndices(dem.height, samplingStep);
    const std::vector<int> sampledColumns = buildSampleIndices(dem.width, samplingStep);

    appendVertices(dem, sampledRows, sampledColumns, verticalScale, terrainMesh.mesh);
    appendTriangles(sampledRows.size(), sampledColumns.size(), terrainMesh.mesh);

    NormalCalculator normalCalculator;
    normalCalculator.computeVertexNormals(terrainMesh.mesh);

    TextureCoordinateCalculator textureCoordinateCalculator;
    textureCoordinateCalculator.computeTerrainTexCoords(terrainMesh.mesh);

    return terrainMesh;
}

} // namespace gis::domain
