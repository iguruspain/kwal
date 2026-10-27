#pragma once

#include <QColor>
#include <QString>

// Tints a source image with ImageMagick's -tint, a non-linear operation that
// preserves the source's tonal structure, so it is executed directly rather
// than re-implemented.
namespace tools
{
// One ImageMagick pass: grayscale, then -fill/-tint (pct = strength*100).
// A GIF source with a GIF destination goes through -coalesce, keeping every
// frame, its delay and the loop count; any other combination outputs a still
// image (a GIF source contributes its first frame, `src[0]`).
// When previewMaxDim > 0 the still image is resized to fit within that square
// (shrink only), for a small, fast preview. Returns false when ImageMagick is
// missing, the source is unreadable, or the pass fails.
bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength = 0.8, int previewMaxDim = 0);

// Path of the tinted copy written next to the original image
// (<stem>-tinted.<ext>, same directory).
QString tintedImagePath(const QString &srcPath);

}
