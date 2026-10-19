#include "tools/tintimage.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace tools
{

namespace {

// ImageMagick fill color, in the same form the old app passed: #rrggbb, or
// rgba(r,g,b,a) when the tint has partial alpha.
QString fillColor(const QColor &tint)
{
    if (tint.alpha() < 255) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(tint.red())
            .arg(tint.green())
            .arg(tint.blue())
            .arg(tint.alpha() / 255.0, 0, 'f', 3);
    }
    return tint.name(QColor::HexRgb);
}

// Prefer the IM7 `magick` binary, fall back to the IM6 `convert` shim.
QString magickExecutable()
{
    const QString magick = QStandardPaths::findExecutable(QStringLiteral("magick"));
    if (!magick.isEmpty())
        return magick;
    return QStandardPaths::findExecutable(QStringLiteral("convert"));
}

bool runMagick(const QString &exe, const QStringList &args)
{
    QProcess process;
    process.start(exe, args);
    if (!process.waitForFinished(60000))
        return false;
    if (process.exitCode() != 0)
        return false;
    return true;
}

}

bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength, int previewMaxDim)
{
    const QString im = magickExecutable();
    if (im.isEmpty())
        return false;
    if (!QFileInfo::exists(srcPath))
        return false;

    const int pct = qBound(0, int(qRound(strength * 100.0)), 100);
    const QFileInfo dstInfo(dstPath);
    const QString tmp = dstInfo.absolutePath()
        + QStringLiteral("/.") + dstInfo.fileName() + QStringLiteral(".gray-tmp.png");

    // 1. Grayscale (optionally resized for a preview).
    QStringList grayArgs;
    if (previewMaxDim > 0)
        grayArgs << srcPath
                 << QStringLiteral("-resize")
                 << QStringLiteral("%1x%2>").arg(previewMaxDim).arg(previewMaxDim);
    else
        grayArgs << srcPath;
    grayArgs << QStringLiteral("-colorspace") << QStringLiteral("gray") << tmp;
    if (!runMagick(im, grayArgs)) {
        QFile::remove(tmp);
        return false;
    }

    // 2. Tint.
    const bool ok = runMagick(im, {tmp, QStringLiteral("-fill"), fillColor(tint),
                                   QStringLiteral("-tint"), QString::number(pct), dstPath});
    QFile::remove(tmp);
    return ok;
}

QString tintedImagePath(const QString &srcPath)
{
    QFileInfo info(srcPath);
    return info.absolutePath() + QStringLiteral("/") + info.completeBaseName()
        + QStringLiteral("-tinted.") + info.suffix();
}

}
