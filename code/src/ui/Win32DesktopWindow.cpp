// Win32DesktopWindow.cpp implements a small native Windows UI for hands-on operation.
// Upstream: runWin32DesktopWindow is called by main when the app starts without arguments.
// Downstream: users can load data, operate the view, and save screenshots from one window.

#include "ui/Win32DesktopWindow.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ControlId stores stable IDs for Win32 child controls.
// Upstream: createControls assigns these IDs to buttons and edits.
// Downstream: handleCommand dispatches user actions by ID.
enum ControlId {
    IdOpenObj = 1001,
    IdOpenDem = 1002,
    IdOpenImage = 1003,
    IdBuildTerrain = 1004,
    IdBatchLoad = 1005,
    IdOpenOsgb = 1006,
    IdClearScene = 1007,
    IdResetView = 1008,
    IdScreenshot = 1009,
    IdYawLeft = 1010,
    IdYawRight = 1011,
    IdPitchUp = 1012,
    IdPitchDown = 1013,
    IdZoomIn = 1014,
    IdZoomOut = 1015,
    IdPanLeft = 1016,
    IdPanRight = 1017,
    IdPanUp = 1018,
    IdPanDown = 1019,
    IdStepEdit = 2001,
    IdScaleEdit = 2002,
    IdStatusEdit = 2003
};

// isButtonCommandId returns true only for clickable operation controls.
// Upstream: windowProcedure receives WM_COMMAND from buttons and edit notifications.
// Downstream: edit-control EN_CHANGE messages are ignored so status logging cannot recurse.
bool isButtonCommandId(int commandId)
{
    return commandId >= IdOpenObj && commandId <= IdPanDown;
}

// DesktopState owns UI state that lives as long as the window.
// Upstream: createMainWindow allocates it before showing the window.
// Downstream: the window procedure reads it from GWLP_USERDATA for every message.
struct DesktopState {
    gis::ui::MainWindow app;           // app owns scene state and use-case wiring.
    HWND statusEdit = nullptr;         // statusEdit shows operation logs and renderer stats.
    HWND samplingStepEdit = nullptr;   // samplingStepEdit stores the current DEM sampling step.
    HWND verticalScaleEdit = nullptr;  // verticalScaleEdit stores the current vertical scale.
    std::string demPath;               // demPath caches the selected DEM file.
    std::string imagePath;             // imagePath caches the selected image file.
};

// ProjectedPoint stores one mesh vertex after camera projection.
// Upstream: projectSceneVertices creates values from Domain mesh positions.
// Downstream: drawWireframePreview connects projected triangle endpoints with GDI lines.
struct ProjectedPoint {
    int x = 0;       // x is the screen-space horizontal coordinate.
    int y = 0;       // y is the screen-space vertical coordinate.
    bool valid = false; // valid is true when the source vertex could be projected.
};

// ScreenTriangle stores one projected triangle with display color and depth.
// Upstream: buildScreenTriangles converts Domain mesh triangles for terrain preview.
// Downstream: drawTerrainSurface fills triangles back-to-front for a cleaner surface view.
struct ScreenTriangle {
    POINT points[3]{};      // points are the projected screen-space triangle vertices.
    COLORREF color = RGB(0, 0, 0); // color is derived from terrain elevation.
    COLORREF edgeColor = RGB(0, 0, 0); // edgeColor is a softened outline color for sparse terrain lines.
    float depth = 0.0F;     // depth is a coarse painter-sort value.
};

// SceneBounds stores the world-space extents of all visible scene geometry.
// Upstream: computeSceneBounds scans SceneGraph mesh vertices.
// Downstream: projection uses the bounds to center and scale the model into the preview area.
struct SceneBounds {
    gis::domain::Vec3 minimum; // minimum is the lower coordinate on each axis.
    gis::domain::Vec3 maximum; // maximum is the upper coordinate on each axis.
    bool valid = false;        // valid is true when at least one vertex was found.
};

// toWide converts UTF-8-ish narrow paths to a Windows wide string.
// Upstream: file dialogs and labels need wide strings.
// Downstream: Win32 APIs receive UTF-16 text.
std::wstring toWide(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring(value.begin(), value.end());
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
    result.resize(static_cast<std::size_t>(length - 1));
    return result;
}

// toNarrow converts a Windows wide string path to UTF-8.
// Upstream: file dialogs return UTF-16 paths.
// Downstream: MainWindow use cases receive std::string paths.
std::string toNarrow(const std::wstring& value)
{
    if (value.empty()) {
        return std::string();
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string(value.begin(), value.end());
    }

    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(length - 1));
    return result;
}

// fileExists checks whether a filesystem path points to a regular executable or file.
// Upstream: resolveOsgViewerPath tests candidate osgviewer.exe locations.
// Downstream: launcher code only uses paths that Windows can start.
bool fileExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

// environmentPath reads one UTF-8 environment variable.
// Upstream: resolveOsgViewerPath reads THREEDGIS_OSGVIEWER and USERPROFILE.
// Downstream: path resolution can use user-specific OSG installations.
std::filesystem::path environmentPath(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::filesystem::path();
    }

    return std::filesystem::u8path(value);
}

// resolveOsgViewerPath finds the OpenSceneGraph viewer executable.
// Upstream: launchOsgViewerForFolder needs a professional OSGB renderer.
// Downstream: CreateProcess receives the resolved executable path.
std::filesystem::path resolveOsgViewerPath()
{
    const std::filesystem::path configuredPath = environmentPath("THREEDGIS_OSGVIEWER");
    if (fileExists(configuredPath)) {
        return configuredPath;
    }

    const std::filesystem::path userProfile = environmentPath("USERPROFILE");
    const std::vector<std::filesystem::path> candidates = {
        userProfile / "anaconda3" / "envs" / "three_gis_osg" / "Library" / "bin" / "osgviewer.exe",
        userProfile / "anaconda3" / "Library" / "bin" / "osgviewer.exe"};
    for (const std::filesystem::path& candidate : candidates) {
        if (fileExists(candidate)) {
            return candidate;
        }
    }

    wchar_t foundPath[MAX_PATH] = {};
    const DWORD foundLength = SearchPathW(nullptr, L"osgviewer.exe", nullptr, MAX_PATH, foundPath, nullptr);
    if (foundLength > 0 && foundLength < MAX_PATH) {
        return std::filesystem::path(foundPath);
    }

    return std::filesystem::path();
}

// isRootOsgbTile identifies the top-level tile file that owns child LOD files.
// Upstream: findRootOsgbTiles scans the selected OSGB folder.
// Downstream: osgviewer receives only root tiles so OSG can page children itself.
bool isRootOsgbTile(const std::filesystem::path& path)
{
    if (path.extension() != ".osgb") {
        return false;
    }

    const std::string stem = path.stem().u8string();
    if (stem.find("_L") != std::string::npos) {
        return false;
    }

    return path.parent_path().filename().u8string() == stem;
}

// findRootOsgbTiles returns root tiles for professional OSG display.
// Upstream: launchOsgViewerForFolder passes the folder selected by the user.
// Downstream: command-line construction passes these paths to osgviewer.
std::vector<std::filesystem::path> findRootOsgbTiles(const std::string& folderPath, std::size_t maxTiles)
{
    std::vector<std::filesystem::path> rootTiles;
    std::filesystem::path firstAnyTile;
    const std::filesystem::path folder = std::filesystem::u8path(folderPath);

    std::error_code error;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error) || entry.path().extension() != ".osgb") {
            continue;
        }
        if (entry.path().u8string().find("_converted_obj_cache") != std::string::npos) {
            continue;
        }
        if (firstAnyTile.empty()) {
            firstAnyTile = entry.path();
        }
        if (isRootOsgbTile(entry.path())) {
            rootTiles.push_back(entry.path());
            if (rootTiles.size() >= maxTiles) {
                break;
            }
        }
    }

    if (rootTiles.empty() && !firstAnyTile.empty()) {
        rootTiles.push_back(firstAnyTile);
    }
    std::sort(rootTiles.begin(), rootTiles.end());
    return rootTiles;
}

// appendQuotedArgument adds one quoted Windows command-line argument.
// Upstream: buildOsgViewerCommandLine passes executable and OSGB tile paths.
// Downstream: CreateProcess receives paths containing spaces or non-ASCII text safely.
void appendQuotedArgument(std::wstring& commandLine, const std::filesystem::path& argument)
{
    commandLine += L" \"";
    commandLine += argument.wstring();
    commandLine += L"\"";
}

// buildOsgViewerCommandLine creates the process command line.
// Upstream: launchOsgViewerForFolder passes the viewer executable and selected root tiles.
// Downstream: CreateProcess starts OpenSceneGraph with native OSGB loading.
std::wstring buildOsgViewerCommandLine(
    const std::filesystem::path& viewerPath,
    const std::vector<std::filesystem::path>& rootTiles)
{
    std::wstring commandLine;
    appendQuotedArgument(commandLine, viewerPath);
    for (const std::filesystem::path& rootTile : rootTiles) {
        appendQuotedArgument(commandLine, rootTile);
    }
    return commandLine;
}

// launchOsgViewerForFolder starts OpenSceneGraph for high-quality OSGB display.
// Upstream: the Open OSGB button passes the selected OSGB folder.
// Downstream: users get native OSG paged-LOD, texture, and camera rendering quality.
gis::ui::UiOperationResult launchOsgViewerForFolder(const std::string& folderPath)
{
    gis::ui::UiOperationResult result;
    const std::filesystem::path viewerPath = resolveOsgViewerPath();
    if (viewerPath.empty()) {
        result.message = "osgviewer.exe was not found. Set THREEDGIS_OSGVIEWER or install OpenSceneGraph.";
        return result;
    }

    const std::vector<std::filesystem::path> rootTiles = findRootOsgbTiles(folderPath, 16U);
    if (rootTiles.empty()) {
        result.message = "No OSGB tile files were found in the selected folder.";
        return result;
    }

    std::wstring commandLine = buildOsgViewerCommandLine(viewerPath, rootTiles);
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const std::filesystem::path workingDirectory = rootTiles.front().parent_path();
    const BOOL created = CreateProcessW(
        viewerPath.wstring().c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workingDirectory.wstring().c_str(),
        &startupInfo,
        &processInfo);
    if (!created) {
        std::ostringstream message;
        message << "Failed to start osgviewer.exe. Windows error " << GetLastError();
        result.message = message.str();
        return result;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    std::ostringstream message;
    message << "Started osgviewer with " << rootTiles.size() << " root OSGB tile(s).";
    result.success = true;
    result.message = message.str();
    result.warnings.push_back("Professional OSGB display uses OpenSceneGraph directly; the built-in preview remains for OBJ/DEM checks.");
    return result;
}

// appendStatus writes one line to the status edit box.
// Upstream: UI operations pass success, warning, and diagnostic messages.
// Downstream: the user sees what each button did without reading a console.
void appendStatus(HWND statusEdit, const std::string& line)
{
    if (statusEdit == nullptr) {
        return;
    }

    const std::wstring wideLine = toWide(line + "\r\n");
    const int textLength = GetWindowTextLengthW(statusEdit);
    SendMessageW(statusEdit, EM_SETSEL, static_cast<WPARAM>(textLength), static_cast<LPARAM>(textLength));
    SendMessageW(statusEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wideLine.c_str()));
}

// appendResult writes a UI operation result and all warnings to the status box.
// Upstream: handleCommand passes each MainWindow result.
// Downstream: users can diagnose file loading and partial batch failures.
void appendResult(HWND statusEdit, const gis::ui::UiOperationResult& result)
{
    appendStatus(statusEdit, std::string(result.success ? "OK: " : "ERROR: ") + result.message);
    for (const std::string& warning : result.warnings) {
        appendStatus(statusEdit, "Warning: " + warning);
    }
}

// makeRendererSummary formats current scene and camera state.
// Upstream: redrawPreview and command handlers ask for current diagnostics.
// Downstream: the status box and preview text show operation feedback.
std::string makeRendererSummary(const gis::ui::MainWindow& app)
{
    const auto& stats = app.renderer().lastFrameStats();
    std::ostringstream output;
    output << "Scene nodes=" << app.scene().nodes.size()
           << ", drawables=" << stats.drawableCount
           << ", vertices=" << stats.vertexCount
           << ", triangles=" << stats.triangleCount
           << ", textured=" << stats.texturedDrawableCount
           << ", yaw=" << stats.camera.yawDegrees
           << ", pitch=" << stats.camera.pitchDegrees
           << ", distance=" << stats.camera.distance;
    return output.str();
}

// degreesToRadians converts camera angles for trigonometric projection.
// Upstream: projectPoint passes renderer yaw and pitch degrees.
// Downstream: std::sin and std::cos receive radians.
float degreesToRadians(float degrees)
{
    return degrees * 3.14159265358979323846F / 180.0F;
}

// computeSceneBounds finds a combined bounding box for visible meshes.
// Upstream: drawWireframePreview passes the current scene graph.
// Downstream: projectPoint uses the box center and scale to fit the view.
SceneBounds computeSceneBounds(const gis::domain::SceneGraph& scene)
{
    SceneBounds bounds;
    bounds.minimum = gis::domain::Vec3{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    bounds.maximum = gis::domain::Vec3{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    for (const gis::domain::SceneNode& node : scene.nodes) {
        if (!node.visible) {
            continue;
        }

        for (const gis::domain::Vertex& vertex : node.mesh.vertices) {
            bounds.minimum.x = std::min(bounds.minimum.x, vertex.position.x);
            bounds.minimum.y = std::min(bounds.minimum.y, vertex.position.y);
            bounds.minimum.z = std::min(bounds.minimum.z, vertex.position.z);
            bounds.maximum.x = std::max(bounds.maximum.x, vertex.position.x);
            bounds.maximum.y = std::max(bounds.maximum.y, vertex.position.y);
            bounds.maximum.z = std::max(bounds.maximum.z, vertex.position.z);
            bounds.valid = true;
        }
    }

    return bounds;
}

// boundsCenter returns the midpoint of the scene bounds.
// Upstream: projectPoint uses it to move world coordinates around the view origin.
// Downstream: all nodes share one centered camera projection.
gis::domain::Vec3 boundsCenter(const SceneBounds& bounds)
{
    return gis::domain::Vec3{
        (bounds.minimum.x + bounds.maximum.x) * 0.5F,
        (bounds.minimum.y + bounds.maximum.y) * 0.5F,
        (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

// boundsMaxExtent returns the largest axis length of the scene bounds.
// Upstream: projectPoint uses it to compute a stable fit scale.
// Downstream: small and large models both fit inside the preview rectangle.
float boundsMaxExtent(const SceneBounds& bounds)
{
    const float extentX = bounds.maximum.x - bounds.minimum.x;
    const float extentY = bounds.maximum.y - bounds.minimum.y;
    const float extentZ = bounds.maximum.z - bounds.minimum.z;
    return std::max(0.0001F, std::max(extentX, std::max(extentY, extentZ)));
}

// boundsHorizontalExtent returns the largest planimetric axis length.
// Upstream: projectPoint uses it to fit DEM terrain by x/y footprint instead of raw elevation range.
// Downstream: terrain meshes with large height outliers do not collapse into a single vertical line.
float boundsHorizontalExtent(const SceneBounds& bounds)
{
    const float extentX = bounds.maximum.x - bounds.minimum.x;
    const float extentY = bounds.maximum.y - bounds.minimum.y;
    return std::max(0.0001F, std::max(extentX, extentY));
}

// previewZScale limits vertical relief in the 2D preview.
// Upstream: projectPoint passes scene bounds before camera rotation.
// Downstream: DEM height values remain visible without dominating x/y footprint scaling.
float previewZScale(const SceneBounds& bounds)
{
    const float horizontalExtent = boundsHorizontalExtent(bounds);
    const float verticalExtent = std::max(0.0001F, bounds.maximum.z - bounds.minimum.z);
    if (verticalExtent <= horizontalExtent * 0.35F) {
        return 1.0F;
    }

    return (horizontalExtent * 0.35F) / verticalExtent;
}

// normalizedHeight maps an elevation into [0, 1].
// Upstream: terrainColor passes average triangle elevation.
// Downstream: color bands remain stable across DEM ranges.
float normalizedHeight(float elevation, const SceneBounds& bounds)
{
    const float verticalExtent = std::max(0.0001F, bounds.maximum.z - bounds.minimum.z);
    const float normalized = (elevation - bounds.minimum.z) / verticalExtent;
    return std::max(0.0F, std::min(1.0F, normalized));
}

// clampByte keeps computed color channels inside the GDI byte range.
// Upstream: color blending and lighting helpers pass float channel values.
// Downstream: RGB receives valid 0-255 channel values.
int clampByte(float value)
{
    return std::max(0, std::min(255, static_cast<int>(std::round(value))));
}

// colorChannel extracts one byte from a COLORREF.
// Upstream: blendColor and shadeColor pass packed GDI colors.
// Downstream: color math works on individual red, green, and blue channels.
int colorChannel(COLORREF color, int channel)
{
    if (channel == 0) {
        return GetRValue(color);
    }
    if (channel == 1) {
        return GetGValue(color);
    }
    return GetBValue(color);
}

// blendColor mixes two display colors by a fixed ratio.
// Upstream: terrainTriangleColor combines texture samples with elevation tint.
// Downstream: filled terrain triangles use the blended result.
COLORREF blendColor(COLORREF baseColor, COLORREF overlayColor, float overlayRatio)
{
    const float ratio = std::max(0.0F, std::min(1.0F, overlayRatio));
    const float baseRatio = 1.0F - ratio;
    return RGB(
        clampByte(static_cast<float>(colorChannel(baseColor, 0)) * baseRatio + static_cast<float>(colorChannel(overlayColor, 0)) * ratio),
        clampByte(static_cast<float>(colorChannel(baseColor, 1)) * baseRatio + static_cast<float>(colorChannel(overlayColor, 1)) * ratio),
        clampByte(static_cast<float>(colorChannel(baseColor, 2)) * baseRatio + static_cast<float>(colorChannel(overlayColor, 2)) * ratio));
}

// shadeColor applies simple terrain lighting to a display color.
// Upstream: terrainTriangleColor passes the sampled or hypsometric triangle color.
// Downstream: terrain fill gains slope relief without a full graphics API.
COLORREF shadeColor(COLORREF color, float light)
{
    const float clampedLight = std::max(0.55F, std::min(1.25F, light));
    return RGB(
        clampByte(static_cast<float>(GetRValue(color)) * clampedLight),
        clampByte(static_cast<float>(GetGValue(color)) * clampedLight),
        clampByte(static_cast<float>(GetBValue(color)) * clampedLight));
}

// terrainColor creates a muted hypsometric color for one terrain triangle.
// Upstream: buildScreenTriangles passes the average z value of a triangle.
// Downstream: filled terrain preview shows relief without needing a texture renderer.
COLORREF terrainColor(float elevation, const SceneBounds& bounds)
{
    const float value = normalizedHeight(elevation, bounds);
    if (value < 0.35F) {
        const float t = value / 0.35F;
        return RGB(
            static_cast<int>(48 + 42 * t),
            static_cast<int>(116 + 68 * t),
            static_cast<int>(92 + 40 * t));
    }
    if (value < 0.72F) {
        const float t = (value - 0.35F) / 0.37F;
        return RGB(
            static_cast<int>(90 + 96 * t),
            static_cast<int>(184 + 30 * t),
            static_cast<int>(132 - 56 * t));
    }

    const float t = (value - 0.72F) / 0.28F;
    return RGB(
        static_cast<int>(186 + 44 * t),
        static_cast<int>(214 - 38 * t),
        static_cast<int>(76 + 42 * t));
}

// sampleTextureColor returns one RGB color from a TextureImage using normalized UVs.
// Upstream: terrainTriangleColor averages triangle vertex UVs.
// Downstream: the Win32 terrain preview can show the real draped image content.
COLORREF sampleTextureColor(const gis::domain::TextureImage& texture, float u, float v)
{
    if (texture.width <= 0 || texture.height <= 0 || texture.channels <= 0 || texture.pixels.empty()) {
        return RGB(128, 160, 128);
    }

    const float clampedU = std::max(0.0F, std::min(1.0F, u));
    const float clampedV = std::max(0.0F, std::min(1.0F, v));
    const int x = std::max(0, std::min(texture.width - 1, static_cast<int>(std::round(clampedU * static_cast<float>(texture.width - 1)))));
    const int y = std::max(0, std::min(texture.height - 1, static_cast<int>(std::round((1.0F - clampedV) * static_cast<float>(texture.height - 1)))));
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width) + static_cast<std::size_t>(x)) *
                              static_cast<std::size_t>(texture.channels);
    if (index >= texture.pixels.size()) {
        return RGB(128, 160, 128);
    }

    const std::uint8_t red = texture.pixels[index];
    const std::uint8_t green = texture.channels > 1 && index + 1 < texture.pixels.size() ? texture.pixels[index + 1] : red;
    const std::uint8_t blue = texture.channels > 2 && index + 2 < texture.pixels.size() ? texture.pixels[index + 2] : red;
    return RGB(red, green, blue);
}

// normalLight computes a stable directional light from three averaged vertex normals.
// Upstream: terrainTriangleColor passes the triangle vertices.
// Downstream: shadeColor receives a light factor for terrain relief.
float normalLight(
    const gis::domain::Vertex& a,
    const gis::domain::Vertex& b,
    const gis::domain::Vertex& c)
{
    const float nx = (a.normal.x + b.normal.x + c.normal.x) / 3.0F;
    const float ny = (a.normal.y + b.normal.y + c.normal.y) / 3.0F;
    const float nz = (a.normal.z + b.normal.z + c.normal.z) / 3.0F;
    const float length = std::max(0.0001F, std::sqrt(nx * nx + ny * ny + nz * nz));
    const float unitX = nx / length;
    const float unitY = ny / length;
    const float unitZ = nz / length;
    const float dot = unitX * -0.35F + unitY * -0.45F + unitZ * 0.82F;
    return 0.78F + std::max(0.0F, dot) * 0.38F;
}

// terrainTriangleColor combines texture, elevation tint, and normal lighting.
// Upstream: buildScreenTriangles passes triangle vertices and scene bounds.
// Downstream: ScreenTriangle::color drives the filled GDI polygon.
COLORREF terrainTriangleColor(
    const gis::domain::SceneNode& node,
    const gis::domain::Vertex& a,
    const gis::domain::Vertex& b,
    const gis::domain::Vertex& c,
    float averageElevation,
    const SceneBounds& bounds)
{
    const COLORREF heightColor = terrainColor(averageElevation, bounds);
    COLORREF baseColor = heightColor;
    if (!node.texture.pixels.empty()) {
        const float u = (a.texCoord.x + b.texCoord.x + c.texCoord.x) / 3.0F;
        const float v = (a.texCoord.y + b.texCoord.y + c.texCoord.y) / 3.0F;
        baseColor = blendColor(sampleTextureColor(node.texture, u, v), heightColor, 0.22F);
    }

    return shadeColor(baseColor, normalLight(a, b, c));
}

// darkenColor creates a low-contrast line color from the triangle fill color.
// Upstream: buildScreenTriangles derives sparse terrain outlines.
// Downstream: drawTerrainSurface draws mesh structure without black speckling.
COLORREF darkenColor(COLORREF color, float ratio)
{
    const float keep = std::max(0.0F, std::min(1.0F, ratio));
    return RGB(
        clampByte(static_cast<float>(GetRValue(color)) * keep),
        clampByte(static_cast<float>(GetGValue(color)) * keep),
        clampByte(static_cast<float>(GetBValue(color)) * keep));
}

// projectPoint converts one 3D position into preview screen coordinates.
// Upstream: projectSceneVertices passes mesh vertex positions and current camera stats.
// Downstream: drawWireframePreview draws triangle edges using the returned point.
ProjectedPoint projectPoint(
    const gis::domain::Vec3& position,
    const SceneBounds& bounds,
    const gis::infrastructure::CameraState& camera,
    const RECT& previewRect)
{
    const gis::domain::Vec3 center = boundsCenter(bounds);
    const float yaw = degreesToRadians(camera.yawDegrees);
    const float pitch = degreesToRadians(camera.pitchDegrees);
    const float zScale = previewZScale(bounds);

    const float x = position.x - center.x;
    const float y = position.y - center.y;
    const float z = (position.z - center.z) * zScale;

    const float yawX = x * std::cos(yaw) - y * std::sin(yaw);
    const float yawY = x * std::sin(yaw) + y * std::cos(yaw);
    const float viewVertical = yawY * std::sin(pitch) + z * std::cos(pitch);

    const int width = previewRect.right - previewRect.left;
    const int height = previewRect.bottom - previewRect.top;
    const float baseScale = static_cast<float>(std::min(width, height)) * 0.72F / boundsHorizontalExtent(bounds);
    const float zoomScale = 5.0F / std::max(0.1F, camera.distance);
    const float scale = baseScale * zoomScale;

    ProjectedPoint point;
    point.x = previewRect.left + width / 2 + static_cast<int>((yawX + camera.panX) * scale);
    point.y = previewRect.top + height / 2 - static_cast<int>((viewVertical + camera.panY) * scale);
    point.valid = true;
    return point;
}

// projectSceneVertices projects every vertex in one scene node.
// Upstream: drawWireframePreview passes visible scene nodes.
// Downstream: drawTriangleEdge reads projected endpoints by triangle index.
std::vector<ProjectedPoint> projectSceneVertices(
    const gis::domain::SceneNode& node,
    const SceneBounds& bounds,
    const gis::infrastructure::CameraState& camera,
    const RECT& previewRect)
{
    std::vector<ProjectedPoint> points;
    points.reserve(node.mesh.vertices.size());
    for (const gis::domain::Vertex& vertex : node.mesh.vertices) {
        points.push_back(projectPoint(vertex.position, bounds, camera, previewRect));
    }
    return points;
}

// drawTriangleEdge draws one edge if both projected endpoints are valid.
// Upstream: drawWireframePreview calls it for each sampled triangle edge.
// Downstream: GDI displays a real mesh wireframe instead of a placeholder bar.
void drawTriangleEdge(HDC deviceContext, const ProjectedPoint& a, const ProjectedPoint& b)
{
    if (!a.valid || !b.valid) {
        return;
    }

    MoveToEx(deviceContext, a.x, a.y, nullptr);
    LineTo(deviceContext, b.x, b.y);
}

// isTerrainNode checks whether a scene node represents textured terrain.
// Upstream: drawWireframePreview passes each visible node.
// Downstream: terrain nodes receive filled shaded surfaces while models keep line display.
bool isTerrainNode(const gis::domain::SceneNode& node)
{
    return !node.texture.pixels.empty();
}

// buildScreenTriangles converts terrain mesh triangles into fillable screen triangles.
// Upstream: drawTerrainSurface passes a terrain scene node and projected points.
// Downstream: GDI polygon fill draws a continuous terrain surface.
std::vector<ScreenTriangle> buildScreenTriangles(
    const gis::domain::SceneNode& node,
    const std::vector<ProjectedPoint>& points,
    const SceneBounds& bounds)
{
    std::vector<ScreenTriangle> screenTriangles;
    screenTriangles.reserve(node.mesh.triangles.size());

    for (const gis::domain::Triangle& triangle : node.mesh.triangles) {
        if (triangle.v0 >= points.size() || triangle.v1 >= points.size() || triangle.v2 >= points.size()) {
            continue;
        }

        const ProjectedPoint& p0 = points[triangle.v0];
        const ProjectedPoint& p1 = points[triangle.v1];
        const ProjectedPoint& p2 = points[triangle.v2];
        if (!p0.valid || !p1.valid || !p2.valid) {
            continue;
        }

        const float averageElevation =
            (node.mesh.vertices[triangle.v0].position.z +
             node.mesh.vertices[triangle.v1].position.z +
             node.mesh.vertices[triangle.v2].position.z) / 3.0F;
        const gis::domain::Vertex& vertex0 = node.mesh.vertices[triangle.v0];
        const gis::domain::Vertex& vertex1 = node.mesh.vertices[triangle.v1];
        const gis::domain::Vertex& vertex2 = node.mesh.vertices[triangle.v2];

        ScreenTriangle screenTriangle;
        screenTriangle.points[0] = POINT{p0.x, p0.y};
        screenTriangle.points[1] = POINT{p1.x, p1.y};
        screenTriangle.points[2] = POINT{p2.x, p2.y};
        screenTriangle.color = terrainTriangleColor(node, vertex0, vertex1, vertex2, averageElevation, bounds);
        screenTriangle.edgeColor = darkenColor(screenTriangle.color, 0.62F);
        screenTriangle.depth = static_cast<float>(p0.y + p1.y + p2.y) / 3.0F;
        screenTriangles.push_back(screenTriangle);
    }

    std::sort(screenTriangles.begin(), screenTriangles.end(), [](const ScreenTriangle& left, const ScreenTriangle& right) {
        return left.depth < right.depth;
    });
    return screenTriangles;
}

// drawTerrainSurface fills projected terrain triangles with texture/elevation colors.
// Upstream: drawWireframePreview calls it before drawing sparse terrain grid lines.
// Downstream: DEM preview reads as a continuous shaded surface rather than debug wire noise.
void drawTerrainSurface(
    HDC deviceContext,
    const std::vector<ScreenTriangle>& screenTriangles)
{
    HPEN fillPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(deviceContext, fillPen);

    for (const ScreenTriangle& triangle : screenTriangles) {
        HBRUSH brush = CreateSolidBrush(triangle.color);
        HGDIOBJ oldBrush = SelectObject(deviceContext, brush);
        Polygon(deviceContext, triangle.points, 3);
        SelectObject(deviceContext, oldBrush);
        DeleteObject(brush);
    }

    SelectObject(deviceContext, oldPen);
    DeleteObject(fillPen);
}

// drawSparseTerrainEdges overlays optional softened triangle edges for diagnostics.
// Upstream: drawWireframePreview calls it after filled terrain polygons.
// Downstream: the user can see terrain triangulation without dense black artifacts.
void drawSparseTerrainEdges(
    HDC deviceContext,
    const std::vector<ScreenTriangle>& screenTriangles)
{
    const std::size_t targetLineCount = 900U;
    const std::size_t stride = std::max<std::size_t>(1U, screenTriangles.size() / targetLineCount);
    for (std::size_t index = 0; index < screenTriangles.size(); index += stride) {
        const ScreenTriangle& triangle = screenTriangles[index];
        HPEN edgePen = CreatePen(PS_SOLID, 1, triangle.edgeColor);
        HGDIOBJ oldPen = SelectObject(deviceContext, edgePen);
        MoveToEx(deviceContext, triangle.points[0].x, triangle.points[0].y, nullptr);
        LineTo(deviceContext, triangle.points[1].x, triangle.points[1].y);
        LineTo(deviceContext, triangle.points[2].x, triangle.points[2].y);
        LineTo(deviceContext, triangle.points[0].x, triangle.points[0].y);
        SelectObject(deviceContext, oldPen);
        DeleteObject(edgePen);
    }
}

// drawWireframePreview draws actual mesh geometry as a projected 3D wireframe.
// Upstream: drawPreview passes the current scene graph and renderer camera.
// Downstream: the preview area shows OBJ and terrain shape using real vertices and triangles.
void drawWireframePreview(HDC deviceContext, const RECT& previewRect, const gis::ui::MainWindow& app)
{
    const SceneBounds bounds = computeSceneBounds(app.scene());
    if (!bounds.valid) {
        return;
    }

    const auto& camera = app.renderer().lastFrameStats().camera;
    for (const gis::domain::SceneNode& node : app.scene().nodes) {
        if (!node.visible || node.mesh.vertices.empty()) {
            continue;
        }

        const std::vector<ProjectedPoint> points = projectSceneVertices(node, bounds, camera, previewRect);
        const bool terrainNode = isTerrainNode(node);
        if (terrainNode && !node.mesh.triangles.empty()) {
            const std::vector<ScreenTriangle> screenTriangles = buildScreenTriangles(node, points, bounds);
            drawTerrainSurface(deviceContext, screenTriangles);
        }

        const COLORREF color = terrainNode ? RGB(54, 95, 72) : RGB(128, 166, 230);
        HPEN meshPen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(deviceContext, meshPen);

        if (!terrainNode && !node.mesh.triangles.empty()) {
            const std::size_t targetLineCount = 25000U;
            const std::size_t stride = std::max<std::size_t>(1U, node.mesh.triangles.size() / targetLineCount);
            for (std::size_t index = 0; index < node.mesh.triangles.size(); index += stride) {
                const gis::domain::Triangle& triangle = node.mesh.triangles[index];
                if (triangle.v0 >= points.size() || triangle.v1 >= points.size() || triangle.v2 >= points.size()) {
                    continue;
                }
                drawTriangleEdge(deviceContext, points[triangle.v0], points[triangle.v1]);
                drawTriangleEdge(deviceContext, points[triangle.v1], points[triangle.v2]);
                drawTriangleEdge(deviceContext, points[triangle.v2], points[triangle.v0]);
            }
        } else {
            const std::size_t stride = std::max<std::size_t>(1U, points.size() / 20000U);
            for (std::size_t index = 0; index < points.size(); index += stride) {
                const ProjectedPoint& point = points[index];
                if (point.valid) {
                    SetPixel(deviceContext, point.x, point.y, color);
                }
            }
        }

        SelectObject(deviceContext, oldPen);
        DeleteObject(meshPen);
    }
}

// readEditInt reads an integer from an edit control with fallback.
// Upstream: Build Terrain uses it for sampling step input.
// Downstream: MainWindow receives a validated terrain setting.
int readEditInt(HWND edit, int fallback)
{
    wchar_t buffer[64] = {};
    GetWindowTextW(edit, buffer, 64);
    try {
        return std::stoi(std::wstring(buffer));
    } catch (...) {
        return fallback;
    }
}

// readEditFloat reads a float from an edit control with fallback.
// Upstream: Build Terrain uses it for vertical scale input.
// Downstream: MainWindow receives a validated terrain setting.
float readEditFloat(HWND edit, float fallback)
{
    wchar_t buffer[64] = {};
    GetWindowTextW(edit, buffer, 64);
    try {
        return std::stof(std::wstring(buffer));
    } catch (...) {
        return fallback;
    }
}

// chooseFile opens a standard Windows file selection dialog.
// Upstream: open OBJ, DEM, image, and screenshot buttons call this helper.
// Downstream: selected paths are passed to MainWindow operations.
bool chooseFile(HWND owner, const wchar_t* title, const wchar_t* filter, bool saveDialog, std::string& selectedPath)
{
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = title;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    const BOOL accepted = saveDialog ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (!accepted) {
        return false;
    }

    selectedPath = toNarrow(fileName);
    return true;
}

// chooseFolder opens a standard Windows folder selection dialog.
// Upstream: batch-load button calls this helper.
// Downstream: selected folders are passed to MainWindow::loadBatchModels.
bool chooseFolder(HWND owner, std::string& selectedPath)
{
    BROWSEINFOW browse = {};
    browse.hwndOwner = owner;
    browse.lpszTitle = L"Select OBJ model folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (item == nullptr) {
        return false;
    }

    wchar_t folder[MAX_PATH] = {};
    const BOOL ok = SHGetPathFromIDListW(item, folder);
    CoTaskMemFree(item);
    if (!ok) {
        return false;
    }

    selectedPath = toNarrow(folder);
    return true;
}

// createButton creates one clickable child button.
// Upstream: createControls passes layout and command IDs.
// Downstream: WM_COMMAND messages identify user actions by ID.
HWND createButton(HWND parent, int id, const wchar_t* text, int x, int y, int width, int height)
{
    return CreateWindowExW(
        0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
}

// createLabel creates one static text label.
// Upstream: createControls labels sampling and scale fields.
// Downstream: users can identify numeric controls.
HWND createLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height)
{
    return CreateWindowExW(
        0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

// createEdit creates one text edit child control.
// Upstream: createControls creates numeric inputs and status output.
// Downstream: command handlers read inputs or append status text.
HWND createEdit(HWND parent, int id, const wchar_t* text, int x, int y, int width, int height, DWORD extraStyle)
{
    return CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | extraStyle,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
}

// createControls builds the fixed desktop UI layout.
// Upstream: WM_CREATE calls it after DesktopState is attached.
// Downstream: user actions are available through buttons and edits.
void createControls(HWND window, DesktopState& state)
{
    createButton(window, IdOpenObj, L"Open OBJ", 16, 16, 110, 30);
    createButton(window, IdOpenDem, L"Open DEM", 136, 16, 110, 30);
    createButton(window, IdOpenImage, L"Open Image", 256, 16, 110, 30);
    createButton(window, IdBuildTerrain, L"Build Terrain", 376, 16, 120, 30);
    createButton(window, IdBatchLoad, L"Batch OBJ", 506, 16, 110, 30);
    createButton(window, IdOpenOsgb, L"Open OSGB", 626, 16, 110, 30);
    createButton(window, IdClearScene, L"Clear", 746, 16, 80, 30);
    createButton(window, IdScreenshot, L"Save BMP", 836, 16, 100, 30);

    createLabel(window, L"Step", 16, 58, 40, 22);
    state.samplingStepEdit = createEdit(window, IdStepEdit, L"15", 58, 54, 60, 26, ES_AUTOHSCROLL);
    createLabel(window, L"Scale", 130, 58, 42, 22);
    state.verticalScaleEdit = createEdit(window, IdScaleEdit, L"1", 176, 54, 60, 26, ES_AUTOHSCROLL);

    createButton(window, IdYawLeft, L"Yaw -", 256, 54, 70, 26);
    createButton(window, IdYawRight, L"Yaw +", 332, 54, 70, 26);
    createButton(window, IdPitchUp, L"Pitch +", 408, 54, 76, 26);
    createButton(window, IdPitchDown, L"Pitch -", 490, 54, 76, 26);
    createButton(window, IdZoomIn, L"Zoom +", 572, 54, 76, 26);
    createButton(window, IdZoomOut, L"Zoom -", 654, 54, 76, 26);
    createButton(window, IdResetView, L"Reset", 736, 54, 80, 26);

    createButton(window, IdPanLeft, L"Pan L", 826, 54, 66, 26);
    createButton(window, IdPanRight, L"Pan R", 898, 54, 66, 26);
    createButton(window, IdPanUp, L"Pan U", 970, 54, 66, 26);
    createButton(window, IdPanDown, L"Pan D", 1042, 54, 66, 26);

    state.statusEdit = createEdit(
        window, IdStatusEdit, L"",
        16, 520, 1092, 150,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);
}

// drawPreview fills the preview area with renderer-derived scene feedback.
// Upstream: WM_PAINT calls it whenever the window needs repainting.
// Downstream: users see the loaded scene, drawable counts, and camera operation effects.
void drawPreview(HWND window, HDC deviceContext, DesktopState& state)
{
    RECT previewRect{16, 96, 1108, 506};
    HBRUSH background = CreateSolidBrush(RGB(18, 24, 32));
    FillRect(deviceContext, &previewRect, background);
    DeleteObject(background);

    HPEN framePen = CreatePen(PS_SOLID, 2, RGB(170, 185, 200));
    HGDIOBJ oldPen = SelectObject(deviceContext, framePen);
    HGDIOBJ oldBrush = SelectObject(deviceContext, GetStockObject(HOLLOW_BRUSH));
    Rectangle(deviceContext, previewRect.left, previewRect.top, previewRect.right, previewRect.bottom);
    SelectObject(deviceContext, oldBrush);
    SelectObject(deviceContext, oldPen);
    DeleteObject(framePen);

    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, RGB(220, 225, 232));
    drawWireframePreview(deviceContext, previewRect, state.app);
    const std::wstring summary = toWide(makeRendererSummary(state.app));
    TextOutW(deviceContext, previewRect.left + 24, previewRect.top + 22, summary.c_str(), static_cast<int>(summary.size()));
}

// handleBuildTerrain reads numeric controls and builds the terrain scene node.
// Upstream: handleCommand calls it for the Build Terrain button.
// Downstream: MainWindow receives current parameters and the window preview updates.
void handleBuildTerrain(HWND window, DesktopState& state)
{
    appendResult(state.statusEdit, state.app.setSamplingStep(readEditInt(state.samplingStepEdit, 15)));
    appendResult(state.statusEdit, state.app.setVerticalScale(readEditFloat(state.verticalScaleEdit, 1.0F)));
    if (state.demPath.empty() || state.imagePath.empty()) {
        appendStatus(state.statusEdit, "ERROR: Select DEM and image before building terrain.");
        return;
    }

    appendResult(state.statusEdit, state.app.openTerrainData(state.demPath, state.imagePath));
    appendResult(state.statusEdit, state.app.buildTerrain());
    appendStatus(state.statusEdit, makeRendererSummary(state.app));
    InvalidateRect(window, nullptr, TRUE);
}

// handleCommand dispatches button presses to MainWindow operations.
// Upstream: the window procedure forwards WM_COMMAND messages here.
// Downstream: scene state, camera state, status text, and preview are updated.
void handleCommand(HWND window, DesktopState& state, int commandId)
{
    std::string path;
    switch (commandId) {
    case IdOpenObj:
        if (chooseFile(window, L"Open OBJ point cloud or mesh", L"OBJ files\0*.obj\0All files\0*.*\0", false, path)) {
            appendResult(state.statusEdit, state.app.openSingleModel(path));
        }
        break;
    case IdOpenDem:
        if (chooseFile(window, L"Open DEM GeoTIFF", L"GeoTIFF files\0*.tif;*.tiff\0All files\0*.*\0", false, state.demPath)) {
            appendStatus(state.statusEdit, "DEM selected: " + state.demPath);
        }
        break;
    case IdOpenImage:
        if (chooseFile(window, L"Open image GeoTIFF", L"GeoTIFF files\0*.tif;*.tiff\0All files\0*.*\0", false, state.imagePath)) {
            appendStatus(state.statusEdit, "Image selected: " + state.imagePath);
        }
        break;
    case IdBuildTerrain:
        handleBuildTerrain(window, state);
        break;
    case IdBatchLoad:
        if (chooseFolder(window, path)) {
            appendResult(state.statusEdit, state.app.loadBatchModels(path, true));
        }
        break;
    case IdOpenOsgb:
        if (chooseFolder(window, path)) {
            appendResult(state.statusEdit, launchOsgViewerForFolder(path));
        }
        break;
    case IdClearScene:
        appendResult(state.statusEdit, state.app.clearScene());
        break;
    case IdResetView:
        appendResult(state.statusEdit, state.app.resetView());
        break;
    case IdScreenshot:
        if (chooseFile(window, L"Save BMP screenshot", L"BMP files\0*.bmp\0All files\0*.*\0", true, path)) {
            if (std::filesystem::path(path).extension().empty()) {
                path += ".bmp";
            }
            appendResult(state.statusEdit, state.app.saveScreenshot(path));
        }
        break;
    case IdYawLeft:
        appendResult(state.statusEdit, state.app.orbitView(-10.0F, 0.0F));
        break;
    case IdYawRight:
        appendResult(state.statusEdit, state.app.orbitView(10.0F, 0.0F));
        break;
    case IdPitchUp:
        appendResult(state.statusEdit, state.app.orbitView(0.0F, 5.0F));
        break;
    case IdPitchDown:
        appendResult(state.statusEdit, state.app.orbitView(0.0F, -5.0F));
        break;
    case IdZoomIn:
        appendResult(state.statusEdit, state.app.zoomView(0.8F));
        break;
    case IdZoomOut:
        appendResult(state.statusEdit, state.app.zoomView(1.25F));
        break;
    case IdPanLeft:
        appendResult(state.statusEdit, state.app.panView(-1.0F, 0.0F));
        break;
    case IdPanRight:
        appendResult(state.statusEdit, state.app.panView(1.0F, 0.0F));
        break;
    case IdPanUp:
        appendResult(state.statusEdit, state.app.panView(0.0F, 1.0F));
        break;
    case IdPanDown:
        appendResult(state.statusEdit, state.app.panView(0.0F, -1.0F));
        break;
    default:
        break;
    }

    appendStatus(state.statusEdit, makeRendererSummary(state.app));
    InvalidateRect(window, nullptr, TRUE);
}

// windowProcedure handles native window lifecycle, painting, and commands.
// Upstream: Win32 dispatches messages to this callback.
// Downstream: DesktopState and MainWindow provide persistent application behavior.
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    try {
        DesktopState* state = reinterpret_cast<DesktopState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        switch (message) {
        case WM_NCCREATE: {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE:
            state = reinterpret_cast<DesktopState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (state == nullptr) {
                return -1;
            }
            createControls(window, *state);
            appendStatus(state->statusEdit, "Ready. Open OBJ point cloud/mesh, DEM+image, or a batch folder.");
            return 0;
        case WM_COMMAND:
            if (state != nullptr && isButtonCommandId(LOWORD(wParam))) {
                handleCommand(window, *state, LOWORD(wParam));
            }
            return 0;
        case WM_PAINT:
            if (state != nullptr) {
                PAINTSTRUCT paint = {};
                HDC deviceContext = BeginPaint(window, &paint);
                drawPreview(window, deviceContext, *state);
                EndPaint(window, &paint);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    } catch (const std::exception& error) {
        MessageBoxW(window, toWide(error.what()).c_str(), L"ThreeDGISApp error", MB_ICONERROR | MB_OK);
        PostQuitMessage(1);
        return 0;
    } catch (...) {
        MessageBoxW(window, L"Unknown UI callback error.", L"ThreeDGISApp error", MB_ICONERROR | MB_OK);
        PostQuitMessage(1);
        return 0;
    }
}

// registerWindowClass registers the native window procedure and class name.
// Upstream: runWin32DesktopWindow calls it once before creating the window.
// Downstream: CreateWindowExW can instantiate the desktop shell.
bool registerWindowClass(HINSTANCE instance)
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"ThreeDGISDesktopWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    return RegisterClassW(&windowClass) != 0;
}

} // namespace

namespace gis::ui {

// runWin32DesktopWindow creates the app window and runs the Win32 message loop.
// Upstream: main calls this for normal user-facing startup.
// Downstream: users interact with MainWindow through native controls.
int runWin32DesktopWindow()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registerWindowClass(instance)) {
        return 1;
    }

    DesktopState state;
    HWND window = CreateWindowExW(
        0,
        L"ThreeDGISDesktopWindow",
        L"ThreeDGISApp - 3D GIS Assignment",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1140,
        740,
        nullptr,
        nullptr,
        instance,
        &state);

    if (window == nullptr) {
        return 1;
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

} // namespace gis::ui
