// ObjModelLoader.cpp implements the OBJ file adapter.
// Upstream: LoadSingleModelUseCase requests model loading through IModelLoader.
// Downstream: Application receives ModelAsset and never sees parsing details.

#include "infrastructure/model/ObjModelLoader.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "domain/geometry/Triangle.h"
#include "domain/geometry/Vertex.h"
#include "domain/math/Vec2.h"
#include "domain/math/Vec3.h"

namespace {

// ObjIndex stores one parsed OBJ face corner.
// Upstream: parseFaceToken converts an OBJ token into this structure.
// Downstream: appendFace applies the referenced data to the Domain mesh.
struct ObjIndex {
    std::size_t vertexIndex = 0;   // vertexIndex references the loaded v array.
    std::size_t texCoordIndex = 0; // texCoordIndex references the loaded vt array when present.
    std::size_t normalIndex = 0;   // normalIndex references the loaded vn array when present.
    bool hasTexCoord = false;      // hasTexCoord indicates whether texCoordIndex is valid.
    bool hasNormal = false;        // hasNormal indicates whether normalIndex is valid.
};

// splitSlashToken separates an OBJ face token around slash characters.
// Upstream: parseFaceToken receives tokens like v, v/vt, v//vn, or v/vt/vn.
// Downstream: individual numeric fields are validated and converted to indices.
std::vector<std::string> splitSlashToken(const std::string& token)
{
    std::vector<std::string> parts;
    std::string current;

    for (const char character : token) {
        if (character == '/') {
            parts.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(character);
    }

    parts.push_back(current);
    return parts;
}

// parseObjIndex converts a one-based or negative OBJ index to a zero-based index.
// Upstream: parseFaceToken passes raw text from a face token.
// Downstream: mesh arrays use the returned zero-based index.
std::size_t parseObjIndex(const std::string& rawValue, std::size_t itemCount, const std::string& fieldName)
{
    if (rawValue.empty()) {
        throw std::runtime_error("Missing OBJ " + fieldName + " index.");
    }

    int parsedValue = 0;
    try {
        parsedValue = std::stoi(rawValue);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid OBJ " + fieldName + " index: " + rawValue);
    }

    if (parsedValue == 0) {
        throw std::runtime_error("OBJ " + fieldName + " index cannot be zero.");
    }

    const int resolvedIndex = parsedValue > 0
        ? parsedValue - 1
        : static_cast<int>(itemCount) + parsedValue;

    if (resolvedIndex < 0 || static_cast<std::size_t>(resolvedIndex) >= itemCount) {
        throw std::runtime_error("OBJ " + fieldName + " index is out of range: " + rawValue);
    }

    return static_cast<std::size_t>(resolvedIndex);
}

// parseFaceToken parses one face corner token from an OBJ f line.
// Upstream: appendFace sends each token after the f prefix.
// Downstream: appendFace uses the resulting indices to update mesh data.
ObjIndex parseFaceToken(
    const std::string& token,
    std::size_t vertexCount,
    std::size_t texCoordCount,
    std::size_t normalCount)
{
    const std::vector<std::string> parts = splitSlashToken(token);
    if (parts.empty() || parts.size() > 3U) {
        throw std::runtime_error("Unsupported OBJ face token: " + token);
    }

    ObjIndex index;
    index.vertexIndex = parseObjIndex(parts[0], vertexCount, "vertex");

    if (parts.size() >= 2U && !parts[1].empty()) {
        index.texCoordIndex = parseObjIndex(parts[1], texCoordCount, "texture");
        index.hasTexCoord = true;
    }

    if (parts.size() == 3U && !parts[2].empty()) {
        index.normalIndex = parseObjIndex(parts[2], normalCount, "normal");
        index.hasNormal = true;
    }

    return index;
}

// appendFace parses an OBJ face line and appends one or more triangles.
// Upstream: ObjModelLoader::load calls it after reading an f prefix.
// Downstream: ModelAsset::mesh receives triangle indices and optional vertex attributes.
void appendFace(
    std::istringstream& lineStream,
    gis::domain::Mesh& mesh,
    const std::vector<gis::domain::Vec2>& texCoords,
    const std::vector<gis::domain::Vec3>& normals)
{
    std::vector<ObjIndex> faceIndices;
    std::string token;

    while (lineStream >> token) {
        faceIndices.push_back(parseFaceToken(token, mesh.vertices.size(), texCoords.size(), normals.size()));
    }

    if (faceIndices.size() < 3U) {
        throw std::runtime_error("OBJ face must contain at least three vertices.");
    }

    for (const ObjIndex& index : faceIndices) {
        gis::domain::Vertex& vertex = mesh.vertices[index.vertexIndex];
        if (index.hasTexCoord) {
            vertex.texCoord = texCoords[index.texCoordIndex];
        }
        if (index.hasNormal) {
            vertex.normal = normals[index.normalIndex];
        }
    }

    for (std::size_t i = 1; i + 1U < faceIndices.size(); ++i) {
        gis::domain::Triangle triangle;
        triangle.v0 = faceIndices[0].vertexIndex;
        triangle.v1 = faceIndices[i].vertexIndex;
        triangle.v2 = faceIndices[i + 1U].vertexIndex;
        mesh.triangles.push_back(triangle);
    }
}

// readVertex reads one OBJ v line into a Domain Vertex.
// Upstream: ObjModelLoader::load calls it after seeing the v prefix.
// Downstream: face parsing later references the appended vertex by index.
gis::domain::Vertex readVertex(std::istringstream& lineStream)
{
    gis::domain::Vertex vertex;
    if (!(lineStream >> vertex.position.x >> vertex.position.y >> vertex.position.z)) {
        throw std::runtime_error("Invalid OBJ vertex line.");
    }
    return vertex;
}

// readTexCoord reads one OBJ vt line into a Domain Vec2.
// Upstream: ObjModelLoader::load calls it after seeing the vt prefix.
// Downstream: face parsing may assign it to referenced vertices.
gis::domain::Vec2 readTexCoord(std::istringstream& lineStream)
{
    gis::domain::Vec2 texCoord;
    if (!(lineStream >> texCoord.x)) {
        throw std::runtime_error("Invalid OBJ texture coordinate line.");
    }

    if (!(lineStream >> texCoord.y)) {
        texCoord.y = 0.0F;
    }

    return texCoord;
}

// readNormal reads one OBJ vn line into a Domain Vec3.
// Upstream: ObjModelLoader::load calls it after seeing the vn prefix.
// Downstream: face parsing may assign it to referenced vertices.
gis::domain::Vec3 readNormal(std::istringstream& lineStream)
{
    gis::domain::Vec3 normal;
    if (!(lineStream >> normal.x >> normal.y >> normal.z)) {
        throw std::runtime_error("Invalid OBJ normal line.");
    }
    return normal;
}

// modelNameFromPath extracts a stable asset name from a file path.
// Upstream: ObjModelLoader::load passes the loaded file path.
// Downstream: SceneNode uses the name for UI display and debugging.
std::string modelNameFromPath(const std::string& filePath)
{
    const std::filesystem::path path(filePath);
    const std::string stem = path.stem().string();
    return stem.empty() ? "ObjModel" : stem;
}

} // namespace

namespace gis::infrastructure {

// load reads a minimal OBJ file into a Domain ModelAsset.
// Upstream: LoadSingleModelUseCase invokes this method through IModelLoader.
// Downstream: AddModelToSceneUseCase inserts the returned model into SceneGraph.
gis::domain::ModelAsset ObjModelLoader::load(const std::string& filePath)
{
    std::ifstream input(filePath);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open OBJ file: " + filePath);
    }

    gis::domain::ModelAsset model;
    model.name = modelNameFromPath(filePath);

    std::vector<gis::domain::Vec2> texCoords;
    std::vector<gis::domain::Vec3> normals;
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream lineStream(line);
        std::string prefix;
        lineStream >> prefix;

        if (prefix == "v") {
            model.mesh.vertices.push_back(readVertex(lineStream));
        } else if (prefix == "vt") {
            texCoords.push_back(readTexCoord(lineStream));
        } else if (prefix == "vn") {
            normals.push_back(readNormal(lineStream));
        } else if (prefix == "f") {
            appendFace(lineStream, model.mesh, texCoords, normals);
        }
    }

    if (model.mesh.vertices.empty()) {
        throw std::runtime_error("OBJ file has no vertices: " + filePath);
    }

    if (model.mesh.triangles.empty()) {
        throw std::runtime_error("OBJ file has no triangle faces: " + filePath);
    }

    return model;
}

} // namespace gis::infrastructure
