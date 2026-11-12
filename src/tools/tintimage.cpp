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

// The animated families the app treats as frame sequences (GIF and WebP);
// every other source is a still image.
bool isAnimatedSuffix(const QFileInfo &info)
{
    const QString suffix = info.suffix().toLower();
    return suffix == QLatin1String("gif") || suffix == QLatin1String("webp");
}

}

bool tintImageTo(const QString &srcPath, const QString &dstPath, const QColor &tint,
                 qreal strength)
{
    const QString im = magickExecutable();
    if (im.isEmpty())
        return false;
    if (!QFileInfo::exists(srcPath))
        return false;

    const int pct = qBound(0, int(qRound(strength * 100.0)), 100);
    const QFileInfo dstInfo(dstPath);
    const bool animatedSource = isAnimatedSuffix(QFileInfo(srcPath));
    const bool animatedDst = isAnimatedSuffix(dstInfo);

    // A single ImageMagick pass: grayscale and tint applied in order to the
    // same image sequence (measured more than twice as fast as writing a
    // grayscale intermediate and re-reading it). An animated destination
    // keeps every frame, its delay and the loop count (-coalesce for GIFs;
    // the WebP writer re-encodes the whole animation itself).
    // ImageMagick silently splits a multi-frame source written to a
    // single-frame destination into one numbered file per frame, and would
    // flatten the animation anyway, so a still destination always reads the
    // first frame only.
    QStringList args;
    if (animatedSource && animatedDst)
        args << srcPath << QStringLiteral("-coalesce");
    else
        args << (animatedSource ? srcPath + QStringLiteral("[0]") : srcPath);
    args << QStringLiteral("-colorspace") << QStringLiteral("gray")
         << QStringLiteral("-fill") << fillColor(tint)
         << QStringLiteral("-tint") << QString::number(pct)
         << dstPath;
    return runMagick(im, args);
}

QString tintedImagePath(const QString &srcPath)
{
    QFileInfo info(srcPath);
    return info.absolutePath() + QStringLiteral("/tinted/") + info.completeBaseName()
        + QStringLiteral("-tinted.") + info.suffix();
}

}
