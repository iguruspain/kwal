#pragma once

#include <QColor>
#include <QString>

// Tints a source image by running the same two ImageMagick commands the old
// app used (grayscale, then -fill/-tint). ImageMagick's -tint is a non-linear
// operation that preserves the source's tonal structure, so it is executed
// directly rather than re-implemented.
namespace tools
{

// Runs `magick src -colorspace gray <tmp>` then
// `magick <tmp> -fill <tint> -tint <pct> dst`, where pct is strength*100.
// When previewMaxDim > 0 the image is first resized to fit within that square
// (shrink only), for a small, fast preview. Returns false when ImageMagick is
// missing, the source is unreadable, or either step fails.
bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength = 0.8, int previewMaxDim = 0);

// Path of the tinted copy written next to the original image
// (<stem>-tinted.<ext>, same directory).
QString tintedImagePath(const QString &srcPath);

}
