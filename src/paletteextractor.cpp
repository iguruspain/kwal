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
    m_pool.start(new ExtractJob(QPointer<PaletteExtractor>(this), path, std::move(image), backend, dark, scheme));
}

PaletteExtractor::MaterialYouResult PaletteExtractor::materialYouFromImage(const QImage &image, bool dark, const QString &schemeName)
{
    MaterialYouResult result;
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
        return result;
    }

    // Google's material-color-utilities (Apache-2.0): Celebi quantization,
    // ranked suggestions, and the selected M3 scheme from the primary seed.
    const auto quantized = material_color_utilities::QuantizeCelebi(pixels, 128);
    material_color_utilities::ScoreOptions options;
    options.desired = 3;
    const std::vector<Argb> ranked =
        material_color_utilities::RankedSuggestions(quantized.color_to_count, options);
    if (ranked.empty()) {
        return result;
    }
    const auto source = material_color_utilities::Hct(ranked.front());
    const auto scheme = buildScheme(source, dark, schemeName);

    // The 16 swatches in the Material You (M3) order (D6).
    const auto hex = [](const Argb argb) {
        return QString::asprintf("#%06x", argb & 0xFFFFFF);
    };
    const auto tone = [&hex](const material_color_utilities::TonalPalette &palette, double t) {
        return hex(palette.get(t));
    };
    result.colors.append(hex(scheme.GetBackground()));
    result.colors.append(tone(scheme.primary_palette, 80.0));
    result.colors.append(tone(scheme.primary_palette, 70.0));
    result.colors.append(tone(scheme.secondary_palette, 70.0));
    result.colors.append(tone(scheme.secondary_palette, 60.0));
    result.colors.append(tone(scheme.tertiary_palette, 60.0));
    result.colors.append(tone(scheme.tertiary_palette, 50.0));
    result.colors.append(hex(scheme.GetOnBackground()));
    result.colors.append(tone(scheme.neutral_palette, 50.0));
    result.colors.append(hex(scheme.GetPrimaryContainer()));
    result.colors.append(hex(scheme.GetSecondaryContainer()));
    result.colors.append(hex(scheme.GetTertiaryContainer()));
    result.colors.append(hex(scheme.GetSurfaceContainer()));
    result.colors.append(hex(scheme.GetSurfaceContainerHigh()));
    result.colors.append(hex(scheme.GetOnSurface()));
    result.colors.append(hex(scheme.GetOnSurfaceVariant()));

    for (const Argb seed : ranked) {
        result.accents.append(hex(seed));
    }
    result.seed = result.accents.first();
    return result;
}

void PaletteExtractor::processMaterialYou(const QString &path, const QString &image, bool dark, const QString &schemeName)
{
    const QImage loaded = QImage(image);
    MaterialYouResult result;
    if (loaded.isNull()) {
        emitReady(path, QStringList(), QStringList(), QString());
        return;
    }
    result = materialYouFromImage(loaded, dark, schemeName);
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
