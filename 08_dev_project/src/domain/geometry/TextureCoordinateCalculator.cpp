// TextureCoordinateCalculator.cpp implements planar terrain UV generation.
// Upstream: TerrainMeshBuilder supplies a mesh whose X/Y positions represent DEM grid placement.
// Downstream: rendering adapters use Vertex::texCoord for texture sampling.

#include "domain/geometry/TextureCoordinateCalculator.h"

#include <algorithm>

namespace {

// Bounds stores planar X/Y coordinate limits for a mesh.
// Upstream: findBounds computes it from mesh vertices.
// Downstream: assignTexCoords normalizes vertex positions into this range.
struct Bounds {
    float minX = 0.0F; // minX is the smallest vertex X coordinate.
    float maxX = 0.0F; // maxX is the largest vertex X coordinate.
    float minY = 0.0F; // minY is the smallest vertex Y coordinate.
    float maxY = 0.0F; // maxY is the largest vertex Y coordinate.
};

// findBounds returns planar coordinate limits for a mesh.
// Upstream: computeTerrainTexCoords passes the terrain mesh.
// Downstream: normalized texture coordinate computation uses the returned bounds.
Bounds findBounds(const gis::domain::Mesh& mesh)
{
    Bounds bounds;
    if (mesh.vertices.empty()) {
        return bounds;
    }

    bounds.minX = mesh.vertices.front().position.x;
    bounds.maxX = mesh.vertices.front().position.x;
    bounds.minY = mesh.vertices.front().position.y;
    bounds.maxY = mesh.vertices.front().position.y;

    for (const gis::domain::Vertex& vertex : mesh.vertices) {
        bounds.minX = std::min(bounds.minX, vertex.position.x);
        bounds.maxX = std::max(bounds.maxX, vertex.position.x);
        bounds.minY = std::min(bounds.minY, vertex.position.y);
        bounds.maxY = std::max(bounds.maxY, vertex.position.y);
    }

    return bounds;
}

// normalizeCoordinate maps a scalar coordinate into [0, 1].
// Upstream: assignTexCoords passes X and Y values with their bounds.
// Downstream: Vertex::texCoord receives stable texture coordinates.
float normalizeCoordinate(float value, float minValue, float maxValue)
{
    const float span = maxValue - minValue;
    if (span <= 0.000001F) {
        return 0.0F;
    }

    return (value - minValue) / span;
}

} // namespace

namespace gis::domain {

// computeTerrainTexCoords fills texture coordinates from planar mesh bounds.
// Upstream: terrain construction has already produced vertex positions.
// Downstream: the terrain texture can be sampled consistently across the full mesh.
void TextureCoordinateCalculator::computeTerrainTexCoords(Mesh& mesh) const
{
    const Bounds bounds = findBounds(mesh);
    for (Vertex& vertex : mesh.vertices) {
        vertex.texCoord.x = normalizeCoordinate(vertex.position.x, bounds.minX, bounds.maxX);
        vertex.texCoord.y = normalizeCoordinate(vertex.position.y, bounds.minY, bounds.maxY);
    }
}

} // namespace gis::domain
