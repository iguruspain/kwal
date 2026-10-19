#include "tools/tintimage.h"

#include <QFileInfo>

namespace tools
{

QImage tintImage(const QImage &source, const QColor tint, qreal strength)
{
    if (source.isNull())
        return QImage();

    const qreal s = qBound(0.0, strength, 1.0);
    const float inv = static_cast<float>(1.0 - s);
    const float tr = static_cast<float>(tint.red());
    const float tg = static_cast<float>(tint.green());
    const float tb = static_cast<float>(tint.blue());
    const float ta = static_cast<float>(tint.alpha()) / 255.0f;

    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            // Perceptual luminance, same weights Qt uses for grayscale.
            const float lum = 0.2126f * static_cast<float>(qRed(pixel))
                + 0.7152f * static_cast<float>(qGreen(pixel)) + 0.0722f * static_cast<float>(qBlue(pixel));
            const int r = qBound(0, static_cast<int>(tr * s + lum * inv + 0.5f), 255);
            const int g = qBound(0, static_cast<int>(tg * s + lum * inv + 0.5f), 255);
            const int b = qBound(0, static_cast<int>(tb * s + lum * inv + 0.5f), 255);
            const int a = qBound(0, static_cast<int>(qAlpha(pixel) * ta + 0.5f), 255);
            line[x] = qRgba(r, g, b, a);
        }
    }
    return image;
}

QString tintedImagePath(const QString &srcPath)
{
    QFileInfo info(srcPath);
    return info.absolutePath() + QStringLiteral("/") + info.completeBaseName()
        + QStringLiteral("-tinted.") + info.suffix();
}

}
