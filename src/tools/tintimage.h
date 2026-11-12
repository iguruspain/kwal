#pragma once

#include <QColor>
#include <QString>

// Tints a source image with ImageMagick's -tint, a non-linear operation that
// preserves the source's tonal structure, so it is executed directly rather
// than re-implemented.
namespace tools
{
// One ImageMagick pass: grayscale, then -fill/-tint (pct = strength*100),
// full resolution, every frame. An animated source (GIF or WebP) with an
// animated destination keeps the whole sequence — frames, delays, loop
// count (-coalesce for GIFs; the WebP writer re-encodes the animation
// itself); an animated source with a still destination contributes its
// first frame (`src[0]`) only, because ImageMagick would otherwise split
// the sequence into one numbered file per frame.
// Returns false when ImageMagick is missing, the source is unreadable, or
// the pass fails.
bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength = 0.8);

// Path of the tinted copy written in a "tinted" subfolder next to the
// original image (<dir>/tinted/<stem>-tinted.<ext>).
QString tintedImagePath(const QString &srcPath);

}
