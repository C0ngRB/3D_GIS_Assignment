#pragma once

// Win32DesktopWindow.h declares the native desktop shell for the 3D GIS app.
// Upstream: main launches this shell when no command-line verification mode is requested.
// Downstream: MainWindow performs data loading, scene updates, camera operations, and screenshots.

#include "ui/MainWindow.h"

namespace gis::ui {

// runWin32DesktopWindow opens the interactive Windows application window.
// Upstream: main calls this for normal double-click usage.
// Downstream: Win32 controls call MainWindow methods and draw the renderer preview.
int runWin32DesktopWindow();

} // namespace gis::ui
