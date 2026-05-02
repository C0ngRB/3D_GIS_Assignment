// SceneScreenshotWriter.cpp implements the report screenshot exporter.
// Upstream: MainWindow refreshes the renderer before invoking this writer.
// Downstream: BMP files provide minimal screenshots without requiring a live OpenGL window.

#include "infrastructure/rendering/SceneScreenshotWriter.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

// Pixel stores one RGB image sample for the generated screenshot.
// Upstream: drawing helpers write Pixel values into the image buffer.
// Downstream: writeBmp converts the buffer into bottom-up BGR BMP rows.
struct Pixel {
    std::uint8_t r = 0; // r is the red channel.
    std::uint8_t g = 0; // g is the green channel.
    std::uint8_t b = 0; // b is the blue channel.
};

// clampInt confines a value to an integer range.
// Upstream: drawing helpers pass raw rectangle coordinates.
// Downstream: buffer writes stay inside the image bounds.
int clampInt(int value, int minValue, int maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

// makeBackground chooses a dark neutral background pixel.
// Upstream: makeImage uses it to initialize the whole canvas.
// Downstream: drawable rectangles remain visible for report screenshots.
Pixel makeBackground()
{
    return Pixel{18, 24, 32};
}

// makeGridPixel chooses a subtle guide-line color.
// Upstream: drawGrid writes repeated guide lines.
// Downstream: screenshots show a stable viewport frame even with one object.
Pixel makeGridPixel()
{
    return Pixel{38, 48, 60};
}

// makeDrawableColor maps renderer metadata to a clear visual category.
// Upstream: drawDrawableBands passes each snapshot.
// Downstream: textured terrain, lit meshes, and plain meshes are visually distinct.
Pixel makeDrawableColor(const gis::infrastructure::DrawableSnapshot& drawable)
{
    if (drawable.textured) {
        return Pixel{70, 150, 115};
    }
    if (drawable.lit) {
        return Pixel{105, 135, 190};
    }
    return Pixel{160, 115, 95};
}

// setPixel writes one pixel if the coordinate is valid.
// Upstream: all drawing helpers call this primitive.
// Downstream: image generation avoids out-of-range writes.
void setPixel(std::vector<Pixel>& image, int width, int height, int x, int y, Pixel color)
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    image[static_cast<std::size_t>(y * width + x)] = color;
}

// fillRect fills an axis-aligned rectangle in the image.
// Upstream: drawHeader, drawViewportFrame, and drawDrawableBands call this helper.
// Downstream: the screenshot contains simple stable shapes instead of blank output.
void fillRect(
    std::vector<Pixel>& image,
    int width,
    int height,
    int left,
    int top,
    int right,
    int bottom,
    Pixel color)
{
    const int x0 = clampInt(left, 0, width);
    const int y0 = clampInt(top, 0, height);
    const int x1 = clampInt(right, 0, width);
    const int y1 = clampInt(bottom, 0, height);

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            setPixel(image, width, height, x, y, color);
        }
    }
}

// drawGrid draws subtle viewport guide lines.
// Upstream: makeImage calls it after clearing the canvas.
// Downstream: report screenshots have visible framing independent of data size.
void drawGrid(std::vector<Pixel>& image, int width, int height)
{
    const Pixel gridColor = makeGridPixel();
    for (int x = 0; x < width; x += 80) {
        fillRect(image, width, height, x, 0, x + 1, height, gridColor);
    }
    for (int y = 0; y < height; y += 80) {
        fillRect(image, width, height, 0, y, width, y + 1, gridColor);
    }
}

// drawViewportFrame draws the active viewport boundary.
// Upstream: makeImage passes renderer frame dimensions.
// Downstream: screenshot consumers can distinguish the viewport from the background.
void drawViewportFrame(std::vector<Pixel>& image, int width, int height)
{
    const Pixel frameColor{170, 185, 200};
    fillRect(image, width, height, 24, 24, width - 24, 28, frameColor);
    fillRect(image, width, height, 24, height - 28, width - 24, height - 24, frameColor);
    fillRect(image, width, height, 24, 24, 28, height - 24, frameColor);
    fillRect(image, width, height, width - 28, 24, width - 24, height - 24, frameColor);
}

// drawHeader draws compact status bars for scene totals.
// Upstream: makeImage passes the latest frame statistics.
// Downstream: screenshots encode whether the scene has drawables and textured objects.
void drawHeader(std::vector<Pixel>& image, int width, int height, const gis::infrastructure::RenderFrameStats& stats)
{
    const int maxBarWidth = width - 96;
    const int drawableWidth = std::min(maxBarWidth, 80 + static_cast<int>(stats.drawableCount) * 70);
    const int texturedWidth = std::min(maxBarWidth, 80 + static_cast<int>(stats.texturedDrawableCount) * 90);
    const int litWidth = std::min(maxBarWidth, 80 + static_cast<int>(stats.litDrawableCount) * 60);

    fillRect(image, width, height, 48, 48, 48 + drawableWidth, 64, Pixel{90, 120, 175});
    fillRect(image, width, height, 48, 72, 48 + texturedWidth, 88, Pixel{70, 150, 115});
    fillRect(image, width, height, 48, 96, 48 + litWidth, 112, Pixel{180, 150, 80});
}

// drawDrawableBands draws one representative band for each prepared drawable.
// Upstream: makeImage passes renderer drawable snapshots.
// Downstream: single-model, terrain, and batch screenshots look different.
void drawDrawableBands(
    std::vector<Pixel>& image,
    int width,
    int height,
    const std::vector<gis::infrastructure::DrawableSnapshot>& drawables)
{
    const int drawableCount = static_cast<int>(std::max<std::size_t>(1U, drawables.size()));
    const int bandAreaTop = 160;
    const int bandAreaBottom = height - 64;
    const int bandAreaHeight = std::max(1, bandAreaBottom - bandAreaTop);
    const int bandHeight = std::max(24, bandAreaHeight / (drawableCount * 2));

    for (std::size_t index = 0; index < drawables.size(); ++index) {
        const auto& drawable = drawables[index];
        const int y = bandAreaTop + static_cast<int>(index) * (bandHeight + 18);
        const int span = std::min(
            width - 120,
            160 + static_cast<int>(std::min<std::size_t>(drawable.triangleCount / 1200U, 560U)));
        const Pixel color = makeDrawableColor(drawable);
        fillRect(image, width, height, 64, y, 64 + span, y + bandHeight, color);
        fillRect(image, width, height, 64, y + bandHeight, 64 + std::max(40, span / 4), y + bandHeight + 6, Pixel{210, 215, 220});
    }
}

// makeImage creates the complete screenshot image buffer.
// Upstream: SceneScreenshotWriter::write passes renderer stats and drawables.
// Downstream: writeBmp serializes the generated pixels.
std::vector<Pixel> makeImage(
    int width,
    int height,
    const gis::infrastructure::RenderFrameStats& stats,
    const std::vector<gis::infrastructure::DrawableSnapshot>& drawables)
{
    std::vector<Pixel> image(static_cast<std::size_t>(width * height), makeBackground());
    drawGrid(image, width, height);
    drawViewportFrame(image, width, height);
    drawHeader(image, width, height, stats);
    drawDrawableBands(image, width, height, drawables);
    return image;
}

// writeUint16 writes a little-endian 16-bit value.
// Upstream: writeBmp uses it for BMP headers.
// Downstream: image viewers can parse the generated file.
void writeUint16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

// writeUint32 writes a little-endian 32-bit value.
// Upstream: writeBmp uses it for BMP headers.
// Downstream: image viewers can parse dimensions and offsets.
void writeUint32(std::ofstream& output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
    output.put(static_cast<char>((value >> 16U) & 0xFFU));
    output.put(static_cast<char>((value >> 24U) & 0xFFU));
}

// writeBmp writes a 24-bit bottom-up BMP file.
// Upstream: SceneScreenshotWriter::write provides the output path and image buffer.
// Downstream: Windows Photos, Word, and the course report can consume the file.
bool writeBmp(const std::string& filePath, int width, int height, const std::vector<Pixel>& image, std::string& errorMessage)
{
    const int rowStride = ((width * 3 + 3) / 4) * 4;
    const std::uint32_t pixelDataSize = static_cast<std::uint32_t>(rowStride * height);
    const std::uint32_t fileSize = 14U + 40U + pixelDataSize;

    std::ofstream output(filePath, std::ios::binary);
    if (!output) {
        errorMessage = "Cannot create screenshot file: " + filePath;
        return false;
    }

    output.put('B');
    output.put('M');
    writeUint32(output, fileSize);
    writeUint16(output, 0U);
    writeUint16(output, 0U);
    writeUint32(output, 54U);

    writeUint32(output, 40U);
    writeUint32(output, static_cast<std::uint32_t>(width));
    writeUint32(output, static_cast<std::uint32_t>(height));
    writeUint16(output, 1U);
    writeUint16(output, 24U);
    writeUint32(output, 0U);
    writeUint32(output, pixelDataSize);
    writeUint32(output, 2835U);
    writeUint32(output, 2835U);
    writeUint32(output, 0U);
    writeUint32(output, 0U);

    std::vector<char> padding(static_cast<std::size_t>(rowStride - width * 3), 0);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const Pixel& pixel = image[static_cast<std::size_t>(y * width + x)];
            output.put(static_cast<char>(pixel.b));
            output.put(static_cast<char>(pixel.g));
            output.put(static_cast<char>(pixel.r));
        }
        output.write(padding.data(), static_cast<std::streamsize>(padding.size()));
    }

    if (!output) {
        errorMessage = "Failed while writing screenshot file: " + filePath;
        return false;
    }

    return true;
}

} // namespace

namespace gis::infrastructure {

// write creates parent folders, draws a deterministic preview, and writes it as BMP.
// Upstream: MainWindow screenshot commands pass the target file path.
// Downstream: the report screenshot folder receives a non-empty image file.
bool SceneScreenshotWriter::write(
    const std::string& filePath,
    const RenderFrameStats& stats,
    const std::vector<DrawableSnapshot>& drawables,
    std::string& errorMessage) const
{
    const std::filesystem::path screenshotPath(filePath);
    if (screenshotPath.has_parent_path()) {
        std::error_code errorCode;
        std::filesystem::create_directories(screenshotPath.parent_path(), errorCode);
        if (errorCode) {
            errorMessage = "Cannot create screenshot folder: " + screenshotPath.parent_path().string();
            return false;
        }
    }

    const int width = std::max(320, stats.viewportWidth);
    const int height = std::max(240, stats.viewportHeight);
    const std::vector<Pixel> image = makeImage(width, height, stats, drawables);
    return writeBmp(filePath, width, height, image, errorMessage);
}

} // namespace gis::infrastructure
