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
#include <filesystem>
#include <sstream>
#include <string>

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
    IdClearScene = 1006,
    IdResetView = 1007,
    IdScreenshot = 1008,
    IdYawLeft = 1009,
    IdYawRight = 1010,
    IdPitchUp = 1011,
    IdPitchDown = 1012,
    IdZoomIn = 1013,
    IdZoomOut = 1014,
    IdPanLeft = 1015,
    IdPanRight = 1016,
    IdPanUp = 1017,
    IdPanDown = 1018,
    IdStepEdit = 2001,
    IdScaleEdit = 2002,
    IdStatusEdit = 2003
};

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

    std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
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

    std::string result(static_cast<std::size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

// appendStatus writes one line to the status edit box.
// Upstream: UI operations pass success, warning, and diagnostic messages.
// Downstream: the user sees what each button did without reading a console.
void appendStatus(HWND statusEdit, const std::string& line)
{
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
    createButton(window, IdClearScene, L"Clear", 626, 16, 80, 30);
    createButton(window, IdScreenshot, L"Save BMP", 716, 16, 100, 30);

    createLabel(window, L"Step", 16, 58, 40, 22);
    state.samplingStepEdit = createEdit(window, IdStepEdit, L"30", 58, 54, 60, 26, ES_AUTOHSCROLL);
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

    const auto& drawables = state.app.renderer().drawables();
    int y = previewRect.top + 64;
    for (const auto& drawable : drawables) {
        const int width = std::min(900, 180 + static_cast<int>(drawable.triangleCount / 400));
        const COLORREF color = drawable.textured ? RGB(70, 150, 115) : RGB(105, 135, 190);
        HBRUSH brush = CreateSolidBrush(color);
        RECT bar{previewRect.left + 48, y, previewRect.left + 48 + width, y + 34};
        FillRect(deviceContext, &bar, brush);
        DeleteObject(brush);
        y += 54;
    }

    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, RGB(220, 225, 232));
    const std::wstring summary = toWide(makeRendererSummary(state.app));
    TextOutW(deviceContext, previewRect.left + 24, previewRect.top + 22, summary.c_str(), static_cast<int>(summary.size()));
}

// handleBuildTerrain reads numeric controls and builds the terrain scene node.
// Upstream: handleCommand calls it for the Build Terrain button.
// Downstream: MainWindow receives current parameters and the window preview updates.
void handleBuildTerrain(HWND window, DesktopState& state)
{
    appendResult(state.statusEdit, state.app.setSamplingStep(readEditInt(state.samplingStepEdit, 30)));
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
    DesktopState* state = reinterpret_cast<DesktopState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_NCCREATE: {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return TRUE;
    }
    case WM_CREATE:
        state = reinterpret_cast<DesktopState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        createControls(window, *state);
        appendStatus(state->statusEdit, "Ready. Open OBJ point cloud/mesh, DEM+image, or a batch folder.");
        return 0;
    case WM_COMMAND:
        if (state != nullptr) {
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
