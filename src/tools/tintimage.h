#pragma once

#include <QColor>
#include <QString>

// ImageMagick -tint is non-linear and preserves tonal structure: shell out rather than re-implement.
namespace tools
{
// Animated src -> animated dst keeps the sequence (-coalesce for GIF; WebP writer re-encodes);
// animated src -> still dst reads [0] only, else ImageMagick splits into one numbered file per frame.
bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength = 0.8);

QString tintedImagePath(const QString &srcPath);

}
