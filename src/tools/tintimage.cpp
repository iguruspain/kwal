#include "tools/tintimage.h"

#include <QColor>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace tools
{

namespace {

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

    // One pass, measured >2x faster than a grayscale intermediate; still dsts read [0] to avoid IM's per-frame file split.
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
