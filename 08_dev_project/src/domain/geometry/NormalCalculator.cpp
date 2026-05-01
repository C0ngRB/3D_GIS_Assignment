// NormalCalculator.cpp implements triangle-mesh vertex normal generation.
// Upstream: TerrainMeshBuilder supplies a fully indexed mesh.
// Downstream: renderers consume Vertex::normal for lighting.

#include "domain/geometry/NormalCalculator.h"

#include <cmath>
#include <stdexcept>

namespace {

// subtract returns the vector difference between two positions.
// Upstream: computeTriangleNormal passes triangle corner positions.
// Downstream: cross computes the triangle normal from these edge vectors.
gis::domain::Vec3 subtract(const gis::domain::Vec3& left, const gis::domain::Vec3& right)
{
    return gis::domain::Vec3{left.x - right.x, left.y - right.y, left.z - right.z};
}

// cross returns the right-handed cross product of two vectors.
// Upstream: computeTriangleNormal passes two triangle edge vectors.
// Downstream: accumulated vertex normals use the returned area-weighted normal.
gis::domain::Vec3 cross(const gis::domain::Vec3& left, const gis::domain::Vec3& right)
{
    return gis::domain::Vec3{
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

// addInPlace adds a vector into an accumulated normal.
// Upstream: NormalCalculator accumulates one triangle normal into each corner.
// Downstream: normalizeInPlace converts the final sum into a unit vector.
void addInPlace(gis::domain::Vec3& target, const gis::domain::Vec3& value)
{
    target.x += value.x;
    target.y += value.y;
    target.z += value.z;
}

// length returns the Euclidean length of a vector.
// Upstream: normalizeInPlace checks whether the normal is degenerate.
// Downstream: normalized vectors divide by the returned length.
float length(const gis::domain::Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

// normalizeInPlace converts a vector to unit length or a safe default.
// Upstream: NormalCalculator passes accumulated vertex normals.
// Downstream: Vertex::normal is stable even for isolated or degenerate vertices.
void normalizeInPlace(gis::domain::Vec3& value)
{
    const float normalLength = length(value);
    if (normalLength <= 0.000001F) {
        value = gis::domain::Vec3{0.0F, 0.0F, 1.0F};
        return;
    }

    value.x /= normalLength;
    value.y /= normalLength;
    value.z /= normalLength;
}

// assertTriangleIndices validates that a triangle references existing vertices.
// Upstream: NormalCalculator checks each triangle before reading vertices.
// Downstream: invalid mesh topology fails clearly instead of reading out of bounds.
void assertTriangleIndices(const gis::domain::Triangle& triangle, const gis::domain::Mesh& mesh)
{
    const std::size_t vertexCount = mesh.vertices.size();
    if (triangle.v0 >= vertexCount || triangle.v1 >= vertexCount || triangle.v2 >= vertexCount) {
        throw std::runtime_error("Triangle index is out of range while computing normals.");
    }
}

// computeTriangleNormal returns an area-weighted normal for one triangle.
// Upstream: NormalCalculator passes a validated triangle and mesh.
// Downstream: the returned vector is accumulated into each triangle corner.
gis::domain::Vec3 computeTriangleNormal(const gis::domain::Triangle& triangle, const gis::domain::Mesh& mesh)
{
    const gis::domain::Vec3& p0 = mesh.vertices[triangle.v0].position;
    const gis::domain::Vec3& p1 = mesh.vertices[triangle.v1].position;
    const gis::domain::Vec3& p2 = mesh.vertices[triangle.v2].position;

    const gis::domain::Vec3 edge01 = subtract(p1, p0);
    const gis::domain::Vec3 edge02 = subtract(p2, p0);
    return cross(edge01, edge02);
}

} // namespace

namespace gis::domain {

// computeVertexNormals fills Vertex::normal by accumulating triangle cross products.
// Upstream: terrain or model mesh construction produces positions and triangle indices.
// Downstream: lighting and visual checks use normalized per-vertex normals.
void NormalCalculator::computeVertexNormals(Mesh& mesh) const
{
    for (Vertex& vertex : mesh.vertices) {
        vertex.normal = Vec3{};
    }

    for (const Triangle& triangle : mesh.triangles) {
        assertTriangleIndices(triangle, mesh);
        const Vec3 triangleNormal = computeTriangleNormal(triangle, mesh);
        addInPlace(mesh.vertices[triangle.v0].normal, triangleNormal);
        addInPlace(mesh.vertices[triangle.v1].normal, triangleNormal);
        addInPlace(mesh.vertices[triangle.v2].normal, triangleNormal);
    }

    for (Vertex& vertex : mesh.vertices) {
        normalizeInPlace(vertex.normal);
    }
}

} // namespace gis::domain
