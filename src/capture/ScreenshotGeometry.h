#ifndef SCREENSHOT_GEOMETRY_H
#define SCREENSHOT_GEOMETRY_H

#include <QRect>
#include <QSize>

// Pure geometry helpers shared by the screenshot service and its tests.
//
// The in-app region-selection overlay works on a downscaled preview image
// (a 4K grab is encoded at most 1920px wide), so the rectangle the user drags
// out is expressed in PREVIEW pixels and must be mapped back onto the
// original grab before cropping.
namespace ScreenshotGeometry {

// Maps `previewRect` (preview-image pixels) onto `original`'s coordinate
// space, then clamps the result to `original`'s bounds. Returns an empty
// rectangle for degenerate inputs (null sizes, empty/non-positive rect).
inline QRect mapPreviewRectToOriginal(const QSize& original, const QSize& preview, const QRect& previewRect) {
    // Note: QSize::isValid() accepts a zero dimension, so the bounds are
    // checked explicitly (a zero preview width would divide by zero below).
    if (original.width() <= 0 || original.height() <= 0 || preview.width() <= 0 || preview.height() <= 0 ||
        previewRect.isEmpty()) {
        return QRect();
    }

    const double scaleX = static_cast<double>(original.width()) / preview.width();
    const double scaleY = static_cast<double>(original.height()) / preview.height();
    const QRect mapped(qRound(previewRect.x() * scaleX), qRound(previewRect.y() * scaleY),
                       qRound(previewRect.width() * scaleX), qRound(previewRect.height() * scaleY));
    return mapped.intersected(QRect(QPoint(0, 0), original));
}

}  // namespace ScreenshotGeometry

#endif  // SCREENSHOT_GEOMETRY_H
