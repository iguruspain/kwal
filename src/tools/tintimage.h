#pragma once

#include <QColor>
#include <QImage>
#include <QString>

// Tints a source image with a solid color, mirroring the old app's
// ImageMagick pipeline (grayscale, then tint): each pixel becomes a blend of
// the tint color (strength) and the pixel's luminance (1 - strength).
namespace tools
{

// Returns a null image when source is null.
QImage tintImage(const QImage &source, const QColor tint, qreal strength = 0.8);

// Path of the tinted copy written next to the original image
// (<stem>-tinted.png, same directory and extension family).
QString tintedImagePath(const QString &srcPath);

}
