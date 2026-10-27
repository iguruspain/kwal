#include "tools/tintimage.h"

#include <QColor>
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
    // Large animated GIFs (hundreds of frames) can take minutes.
    if (!process.waitForFinished(300000))
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
    const bool gifSource = QFileInfo(srcPath)
        .suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0;
    const bool gifDst = dstInfo.suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0;

    // A single ImageMagick pass: grayscale and tint applied in order to the
    // same image sequence (measured more than twice as fast as writing a
    // grayscale intermediate and re-reading it). An animated GIF destination
    // needs -coalesce and keeps every frame, its delay and the loop count;
    // this ImageMagick silently drops a multi-frame image written to a
    // single-frame format (and would flatten the animation anyway), so a
    // still destination always reads the first frame only.
    QStringList args;
    if (gifSource && gifDst)
        args << srcPath << QStringLiteral("-coalesce");
    else
        args << (gifSource ? srcPath + QStringLiteral("[0]") : srcPath);
    if (previewMaxDim > 0 && !(gifSource && gifDst))
        args << QStringLiteral("-resize")
             << QStringLiteral("%1x%2>").arg(previewMaxDim).arg(previewMaxDim);
    args << QStringLiteral("-colorspace") << QStringLiteral("gray")
         << QStringLiteral("-fill") << fillColor(tint)
         << QStringLiteral("-tint") << QString::number(pct)
         << dstPath;
    return runMagick(im, args);
}

QString tintedImagePath(const QString &srcPath)
{
    QFileInfo info(srcPath);
    return info.absolutePath() + QStringLiteral("/") + info.completeBaseName()
        + QStringLiteral("-tinted.") + info.suffix();
}

}
