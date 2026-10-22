#include "paletteextractor.h"
#include "wallpapersinfo.h"
#include "wallpapermodel.h"

#include <cpp/cam/hct.h>
#include <cpp/palettes/tones.h>
#include <cpp/quantize/celebi.h>
#include <cpp/score/score.h>
#include <cpp/scheme/scheme_content.h>
#include <cpp/scheme/scheme_expressive.h>
#include <cpp/scheme/scheme_fidelity.h>
#include <cpp/scheme/scheme_fruit_salad.h>
#include <cpp/scheme/scheme_monochrome.h>
#include <cpp/scheme/scheme_neutral.h>
#include <cpp/scheme/scheme_rainbow.h>
#include <cpp/scheme/scheme_tonal_spot.h>
#include <cpp/scheme/scheme_vibrant.h>
#include <cpp/utils/utils.h>

#include <QImage>
#include <QFileInfo>
#include <QMutex>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QRunnable>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>

#include <vector>

namespace {

constexpr int kProcessTimeoutMs = 30000;

using material_color_utilities::Argb;

QString firstAvailableBinary(const QStringList &candidates)
{
    for (const QString &name : candidates) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}

// The nine Material You (M3) schemes share the (source, is_dark)
// constructor; an unknown name falls back to Tonal Spot (D6). The result
// is sliced to the DynamicScheme base: all 16 swatches (D6) are read from
// base fields only.
material_color_utilities::DynamicScheme buildScheme(const material_color_utilities::Hct &source,
                                                    bool dark,
                                                    const QString &schemeName)
{
    if (schemeName == QLatin1String("Vibrant")) {
        return material_color_utilities::SchemeVibrant(source, dark);
    }
    if (schemeName == QLatin1String("Expressive")) {
        return material_color_utilities::SchemeExpressive(source, dark);
    }
    if (schemeName == QLatin1String("Content")) {
        return material_color_utilities::SchemeContent(source, dark);
    }
    if (schemeName == QLatin1String("FruitSalad")) {
        return material_color_utilities::SchemeFruitSalad(source, dark);
    }
    if (schemeName == QLatin1String("Rainbow")) {
        return material_color_utilities::SchemeRainbow(source, dark);
    }
    if (schemeName == QLatin1String("Monochrome")) {
        return material_color_utilities::SchemeMonochrome(source, dark);
    }
    if (schemeName == QLatin1String("Neutral")) {
        return material_color_utilities::SchemeNeutral(source, dark);
    }
    if (schemeName == QLatin1String("Fidelity")) {
        return material_color_utilities::SchemeFidelity(source, dark);
    }
    return material_color_utilities::SchemeTonalSpot(source, dark);
}

// The nine M3 scheme names, shared by the single-pass computation, the
// fallback lookup and the QML combo.
const QStringList kSchemeNames = {
    QStringLiteral("TonalSpot"),
    QStringLiteral("Vibrant"),
    QStringLiteral("Expressive"),
    QStringLiteral("Content"),
    QStringLiteral("FruitSalad"),
    QStringLiteral("Rainbow"),
    QStringLiteral("Monochrome"),
    QStringLiteral("Neutral"),
    QStringLiteral("Fidelity")};

QString hexOf(const Argb argb)
{
    return QString::asprintf("#%06x", argb & 0xFFFFFF);
}

// The 16 swatches of a scheme in the Material You (M3) order (D6), read
// from DynamicScheme base fields only.
QStringList schemeSwatches(const material_color_utilities::DynamicScheme &scheme)
{
    const auto tone = [](const material_color_utilities::TonalPalette &palette, double t) {
        return hexOf(palette.get(t));
    };
    QStringList colors;
    colors.append(hexOf(scheme.GetBackground()));
    colors.append(tone(scheme.primary_palette, 80.0));
    colors.append(tone(scheme.primary_palette, 70.0));
    colors.append(tone(scheme.secondary_palette, 70.0));
    colors.append(tone(scheme.secondary_palette, 60.0));
    colors.append(tone(scheme.tertiary_palette, 60.0));
    colors.append(tone(scheme.tertiary_palette, 50.0));
    colors.append(hexOf(scheme.GetOnBackground()));
    colors.append(tone(scheme.neutral_palette, 50.0));
    colors.append(hexOf(scheme.GetPrimaryContainer()));
    colors.append(hexOf(scheme.GetSecondaryContainer()));
    colors.append(hexOf(scheme.GetTertiaryContainer()));
    colors.append(hexOf(scheme.GetSurfaceContainer()));
    colors.append(hexOf(scheme.GetSurfaceContainerHigh()));
    colors.append(hexOf(scheme.GetOnSurface()));
    colors.append(hexOf(scheme.GetOnSurfaceVariant()));
    return colors;
}

// Cache key: the wallpaper path plus the dark mode of one extraction pass.
QString schemeCacheKey(const QString &path, bool dark)
{
    return path + QLatin1Char('|') + (dark ? QStringLiteral("dark") : QStringLiteral("light"));
}
} // namespace

class PaletteExtractor::ExtractJob : public QRunnable
{
public:
    ExtractJob(QPointer<PaletteExtractor> extractor,
               QString path,
               QString image,
               QString backend,
               bool dark,
               QString schemeName)
        : m_extractor(std::move(extractor))
        , m_path(std::move(path))
        , m_image(std::move(image))
        , m_backend(std::move(backend))
        , m_dark(dark)
        , m_schemeName(std::move(schemeName))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (!m_extractor) {
            return;
        }
        if (m_backend == QLatin1String("materialyou")) {
            m_extractor->processMaterialYou(m_path, m_image, m_dark, m_schemeName);
        } else {
            m_extractor->processImageMagick(m_path, m_image);
        }
    }

private:
    QPointer<PaletteExtractor> m_extractor;
    QString m_path;
    QString m_image;
    QString m_backend;
    bool m_dark;
    QString m_schemeName;
};


PaletteExtractor::PaletteExtractor(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
}

void PaletteExtractor::extract(const QString &path, const QString &backend, bool dark, const QString &scheme)
{
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        emit extractionFailed(path, tr("File does not exist."));
        return;
    }
    if (backend != QLatin1String("materialyou") && backend != QLatin1String("imagemagick")) {
        emit extractionFailed(path, tr("Unknown palette backend."));
        return;
    }
    QString image = path;
    if (WallpaperModel::isVideo(path)) {
        const WallpapersInfo *info = WallpapersInfo::instance();
        image = info ? info->firstFrameFor(path) : QString();
        if (image.isEmpty() || !QFileInfo(image).isFile()) {
            emit extractionFailed(path, tr("The first frame of the video is not available yet."));
            return;
        }
    }
    if (backend == QLatin1String("materialyou")) {
        // The nine schemes of a previously extracted wallpaper (in this
        // dark mode) are cached: answer immediately instead of re-running
        // Celebi.
        QMutexLocker locker(&m_cacheMutex);
        const QMap<QString, MaterialYouResult> cached = m_schemeCache.value(schemeCacheKey(path, dark));
        if (!cached.isEmpty()) {
            const MaterialYouResult result = cached.value(scheme, cached.value(QStringLiteral("TonalSpot")));
            locker.unlock();
            emitReady(path, result.colors, result.accents, result.seed);
            return;
        }
    }
    m_pool.start(new ExtractJob(QPointer<PaletteExtractor>(this), path, std::move(image), backend, dark, scheme));
}

PaletteExtractor::MaterialYouResult PaletteExtractor::materialYouFromImage(const QImage &image, bool dark, const QString &schemeName)
{
    const QMap<QString, MaterialYouResult> all = materialYouSchemesFromImage(image, dark);
    return all.value(schemeName, all.value(QStringLiteral("TonalSpot")));
}

QMap<QString, PaletteExtractor::MaterialYouResult> PaletteExtractor::materialYouSchemesFromImage(const QImage &image, bool dark)
{
    QMap<QString, MaterialYouResult> out;
    const QImage scaled = image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    std::vector<Argb> pixels;
    pixels.reserve(static_cast<std::size_t>(scaled.width()) * scaled.height());
    for (int y = 0; y < scaled.height(); ++y) {
        for (int x = 0; x < scaled.width(); ++x) {
            const QRgb pixel = scaled.pixel(x, y);
            if (qAlpha(pixel) > 128) {
                pixels.push_back(material_color_utilities::ArgbFromRgb(qRed(pixel), qGreen(pixel), qBlue(pixel)));
            }
        }
    }
    if (pixels.empty()) {
        return out;
    }

    // Google's material-color-utilities (Apache-2.0): Celebi quantization
    // and ranked suggestions dominate the cost; the nine M3 schemes are a
    // cheap recombination of the same ranked colors.
    const auto quantized = material_color_utilities::QuantizeCelebi(pixels, 128);
    material_color_utilities::ScoreOptions options;
    options.desired = 3;
    const std::vector<Argb> ranked =
        material_color_utilities::RankedSuggestions(quantized.color_to_count, options);
    if (ranked.empty()) {
        return out;
    }
    const auto source = material_color_utilities::Hct(ranked.front());

    QStringList accents;
    for (const Argb seed : ranked) {
        accents.append(hexOf(seed));
    }

    for (const QString &name : kSchemeNames) {
        MaterialYouResult result;
        result.colors = schemeSwatches(buildScheme(source, dark, name));
        result.accents = accents;
        result.seed = accents.first();
        out.insert(name, std::move(result));
    }
    return out;
}

void PaletteExtractor::processMaterialYou(const QString &path, const QString &image, bool dark, const QString &schemeName)
{
    const QImage loaded = QImage(image);
    if (loaded.isNull()) {
        emitReady(path, QStringList(), QStringList(), QString());
        return;
    }
    const QMap<QString, MaterialYouResult> all = materialYouSchemesFromImage(loaded, dark);
    {
        QMutexLocker locker(&m_cacheMutex);
        if (!all.isEmpty()) {
            m_schemeCache.insert(schemeCacheKey(path, dark), all);
        }
    }
    const MaterialYouResult result = all.value(schemeName, all.value(QStringLiteral("TonalSpot")));
    emitReady(path, result.colors, result.accents, result.seed);
}

void PaletteExtractor::processImageMagick(const QString &path, const QString &image)
{
    const QString binary = firstAvailableBinary({QStringLiteral("magick"), QStringLiteral("convert")});
    if (binary.isEmpty()) {
        emit extractionFailed(path, tr("ImageMagick is not installed."));
        return;
    }

    // Up to 16 unique colors of the 128x128 downscale.
    QProcess paletteProcess;
    paletteProcess.start(binary,
                         {image,
                          QStringLiteral("-resize"),
                          QStringLiteral("128x128"),
                          QStringLiteral("-colors"),
                          QStringLiteral("16"),
                          QStringLiteral("-unique-colors"),
                          QStringLiteral("txt:-")});
    if (!paletteProcess.waitForStarted(5000) || !paletteProcess.waitForFinished(kProcessTimeoutMs)
        || paletteProcess.exitStatus() != QProcess::NormalExit
        || paletteProcess.exitCode() != 0) {
        paletteProcess.kill();
        emitFailed(path, tr("ImageMagick failed to extract the colors."));
        return;
    }
    QStringList colors;
    for (const QByteArray &line : paletteProcess.readAllStandardOutput().split('\n')) {
        const QString tail = QString::fromLatin1(line);
        const int hexPos = tail.indexOf('#');
        if (hexPos < 0 || tail.size() < hexPos + 7) {
            continue;
        }
        const QString hexPart = tail.mid(hexPos + 1, 6);
        bool ok = false;
        hexPart.toULong(&ok, 16);
        if (ok) {
            colors.append(tail.mid(hexPos, 7).toLower());
        }
    }
    if (colors.isEmpty()) {
        emitFailed(path, tr("ImageMagick returned no colors."));
        return;
    }

    // Best accent: 8-color histogram of the 64x64 downscale, scored by
    // HSV s*v with s > 0.15 and 0.15 < v < 0.95 (same as the original app).
    QString bestHex = colors.first();
    double bestScore = -1.0;
    QProcess histogramProcess;
    histogramProcess.start(binary,
                           {image,
                            QStringLiteral("-resize"),
                            QStringLiteral("64x64"),
                            QStringLiteral("+dither"),
                            QStringLiteral("-colors"),
                            QStringLiteral("8"),
                            QStringLiteral("-format"),
                            QStringLiteral("%c"),
                            QStringLiteral("histogram:info:")});
    if (histogramProcess.waitForStarted(5000) && histogramProcess.waitForFinished(kProcessTimeoutMs)
        && histogramProcess.exitStatus() == QProcess::NormalExit
        && histogramProcess.exitCode() == 0) {
        static const QRegularExpression hexRe(QStringLiteral("#([0-9a-fA-F]{6})"));
        for (const QByteArray &line : histogramProcess.readAllStandardOutput().split('\n')) {
            const QRegularExpressionMatch match = hexRe.match(QString::fromLatin1(line));
            if (!match.hasMatch()) {
                continue;
            }
            const QString hexPart = match.captured(1);
            const double r = hexPart.mid(0, 2).toUInt(nullptr, 16) / 255.0;
            const double g = hexPart.mid(2, 2).toUInt(nullptr, 16) / 255.0;
            const double b = hexPart.mid(4, 2).toUInt(nullptr, 16) / 255.0;
            const double v = std::max({r, g, b});
            const double s = v == 0.0 ? 0.0 : (v - std::min({r, g, b})) / v;
            if (s > 0.15 && v > 0.15 && v < 0.95) {
                const double score = s * v;
                if (score > bestScore) {
                    bestScore = score;
                    bestHex = QStringLiteral("#") + hexPart.toLower();
                }
            }
        }
    }
    emitReady(path, colors, QStringList{bestHex}, QString());
}

void PaletteExtractor::emitReady(const QString &path,
                                 const QStringList &colors,
                                 const QStringList &accents,
                                 const QString &seed)
{
    if (QThread::currentThread() == thread()) {
        emit paletteReady(path, colors, accents, seed);
    } else {
        QMetaObject::invokeMethod(this,
                                  [this, path, colors, accents, seed]() {
                                      emit paletteReady(path, colors, accents, seed);
                                  },
                                  Qt::QueuedConnection);
    }
}

void PaletteExtractor::emitFailed(const QString &path, const QString &message)
{
    if (QThread::currentThread() == thread()) {
        emit extractionFailed(path, message);
    } else {
        QMetaObject::invokeMethod(this,
                                  [this, path, message]() {
                                      emit extractionFailed(path, message);
                                  },
                                  Qt::QueuedConnection);
    }
}
