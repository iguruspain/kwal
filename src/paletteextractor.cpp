#include "paletteextractor.h"
#include "wallpapersinfo.h"
#include "wallpapermodel.h"

#include <QColor>
#include <QImage>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QRunnable>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kMedianCutBoxes = 128;
constexpr int kProcessTimeoutMs = 30000;

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
    int component(int channel) const
    {
        return channel == 0 ? r : (channel == 1 ? g : b);
    }
};

struct Hsl {
    double h = 0.0;
    double s = 0.0;
    double l = 0.0;
};

// Standard HSL (h, s, l in 0..1) from sRGB.
Hsl rgbToHsl(int r, int g, int b)
{
    const double rd = r / 255.0;
    const double gd = g / 255.0;
    const double bd = b / 255.0;
    const double cMax = std::max({rd, gd, bd});
    const double cMin = std::min({rd, gd, bd});
    const double delta = cMax - cMin;
    const double l = (cMax + cMin) / 2.0;
    if (delta == 0.0) {
        return {0.0, 0.0, l};
    }
    const double s = l > 0.5 ? delta / (2.0 - cMax - cMin) : delta / (cMax + cMin);
    double h;
    if (cMax == rd) {
        h = (gd - bd) / delta + (gd < bd ? 6.0 : 0.0);
    } else if (cMax == gd) {
        h = (bd - rd) / delta + 2.0;
    } else {
        h = (rd - gd) / delta + 4.0;
    }
    return {h / 6.0, s, l};
}

// HSL (h, s in 0..1, l in 0..1) to a color.
QColor hslColor(double h, double s, double l)
{
    const int hue = static_cast<int>(std::lround(h * 360.0)) % 360;
    const int sat = static_cast<int>(std::lround(s * 255.0));
    const int lum = static_cast<int>(std::lround(l * 255.0));
    return QColor::fromHsl(hue, sat, lum);
}

// Median-cut quantization: repeatedly splits the box with the largest
// channel range at its median until `targetBoxes` boxes remain.
QVector<QVector<Rgb>> medianCut(const QVector<Rgb> &pixels, int targetBoxes)
{
    QVector<QVector<Rgb>> boxes;
    boxes.append(pixels);
    while (boxes.size() < targetBoxes) {
        int splitIndex = -1;
        int splitChannel = 0;
        int bestRange = 0;
        for (int i = 0; i < boxes.size(); ++i) {
            if (boxes[i].size() < 2) {
                continue;
            }
            int minC[3] = {255, 255, 255};
            int maxC[3] = {0, 0, 0};
            for (const Rgb &p : boxes[i]) {
                for (int c = 0; c < 3; ++c) {
                    minC[c] = std::min(minC[c], p.component(c));
                    maxC[c] = std::max(maxC[c], p.component(c));
                }
            }
            for (int c = 0; c < 3; ++c) {
                const int range = maxC[c] - minC[c];
                if (range > bestRange) {
                    bestRange = range;
                    splitIndex = i;
                    splitChannel = c;
                }
            }
        }
        if (splitIndex == -1) {
            break; // everything is monochromatic
        }
        std::stable_sort(boxes[splitIndex].begin(),
                         boxes[splitIndex].end(),
                         [splitChannel](const Rgb &a, const Rgb &b) {
                             return a.component(splitChannel) < b.component(splitChannel);
                         });
        const int half = boxes[splitIndex].size() / 2;
        QVector<Rgb> second(boxes[splitIndex].constBegin() + half, boxes[splitIndex].constEnd());
        boxes[splitIndex].erase(boxes[splitIndex].begin() + half, boxes[splitIndex].end());
        boxes.insert(splitIndex + 1, std::move(second));
    }
    return boxes;
}

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


} // namespace
class PaletteExtractor::ExtractJob : public QRunnable
{
public:
    ExtractJob(QPointer<PaletteExtractor> extractor,
               QString path,
               QString image,
               QString backend,
               bool dark)
        : m_extractor(std::move(extractor))
        , m_path(std::move(path))
        , m_image(std::move(image))
        , m_backend(std::move(backend))
        , m_dark(dark)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (!m_extractor) {
            return;
        }
        if (m_backend == QLatin1String("materialyou")) {
            m_extractor->processMaterialYou(m_path, m_image, m_dark);
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
};


PaletteExtractor::PaletteExtractor(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
}

void PaletteExtractor::extract(const QString &path, const QString &backend, bool dark)
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
    m_pool.start(new ExtractJob(QPointer<PaletteExtractor>(this), path, std::move(image), backend, dark));
}

PaletteExtractor::MaterialYouResult PaletteExtractor::materialYouFromImage(const QImage &image, bool dark)
{
    MaterialYouResult result;
    const QImage scaled = image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QVector<Rgb> pixels;
    pixels.reserve(scaled.width() * scaled.height());
    for (int y = 0; y < scaled.height(); ++y) {
        for (int x = 0; x < scaled.width(); ++x) {
            const QRgb pixel = scaled.pixel(x, y);
            if (qAlpha(pixel) > 128) {
                pixels.append({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            }
        }
    }
    if (pixels.isEmpty()) {
        return result;
    }

    struct Candidate {
        Rgb mean;
        Hsl hsl;
        double score = 0.0;
    };
    QVector<Candidate> candidates;
    for (const auto &box : medianCut(pixels, kMedianCutBoxes)) {
        long rSum = 0;
        long gSum = 0;
        long bSum = 0;
        for (const Rgb &p : box) {
            rSum += p.r;
            gSum += p.g;
            bSum += p.b;
        }
        const Rgb mean{static_cast<int>(rSum / box.size()),
                       static_cast<int>(gSum / box.size()),
                       static_cast<int>(bSum / box.size())};
        const Hsl hsl = rgbToHsl(mean.r, mean.g, mean.b);
        const double score = hsl.s * (0.35 + 0.6 * std::abs(hsl.l - 0.5))
            * std::log2(1.0 + box.size() / 512.0);
        candidates.append({mean, hsl, score});
    }
    std::stable_sort(candidates.begin(),
                     candidates.end(),
                     [](const Candidate &a, const Candidate &b) { return a.score > b.score; });

    const Candidate primary = candidates.first();
    const Candidate secondary = candidates.size() > 1 ? candidates.at(1) : primary;
    const Candidate tertiary = candidates.size() > 2 ? candidates.at(2) : primary;

    // Tone ramp per role: role(t) = HSL(h_role, sat_role, t/100).
    const auto tone = [](double hue, double sat, double tone) {
        return hslColor(hue, sat, tone / 100.0);
    };
    const auto hex = [](const QColor &color) { return color.name(); };

    const double neutralHue = primary.hsl.h;
    const QStringList primaryTones = dark ? QStringList({QStringLiteral("90"), QStringLiteral("80"), QStringLiteral("70")})
                                          : QStringList({QStringLiteral("30"), QStringLiteral("40"), QStringLiteral("50")});
    const QStringList secondaryTones = dark ? QStringList({QStringLiteral("85"), QStringLiteral("70"), QStringLiteral("60")})
                                            : QStringList({QStringLiteral("35"), QStringLiteral("50"), QStringLiteral("60")});
    const QStringList tertiaryTones = dark ? QStringList({QStringLiteral("80"), QStringLiteral("70"), QStringLiteral("60")})
                                           : QStringList({QStringLiteral("40"), QStringLiteral("50"), QStringLiteral("60")});
    const double backgroundTone = dark ? 10.0 : 95.0;
    const double foregroundTone = dark ? 95.0 : 10.0;

    result.colors.append(hex(tone(neutralHue, 0.05, backgroundTone))); // bg
    result.colors.append(hex(tone(neutralHue, 0.05, foregroundTone))); // fg
    for (const QString &toneValue : primaryTones) {
        result.colors.append(hex(tone(primary.hsl.h, 1.0, toneValue.toDouble())));
    }
    for (const QString &toneValue : secondaryTones) {
        result.colors.append(hex(tone(secondary.hsl.h, 0.75, toneValue.toDouble())));
    }
    for (const QString &toneValue : tertiaryTones) {
        result.colors.append(hex(tone(tertiary.hsl.h, 0.75, toneValue.toDouble())));
    }
    result.colors.append(hex(tone(neutralHue, 0.05, 50.0))); // neutral mid

    result.accents.append(hex(QColor(primary.mean.r, primary.mean.g, primary.mean.b)));
    result.accents.append(hex(QColor(secondary.mean.r, secondary.mean.g, secondary.mean.b)));
    result.accents.append(hex(QColor(tertiary.mean.r, tertiary.mean.g, tertiary.mean.b)));
    result.seed = result.accents.first();
    return result;
}

void PaletteExtractor::processMaterialYou(const QString &path, const QString &image, bool dark)
{
    const QImage loaded = QImage(image);
    MaterialYouResult result;
    if (loaded.isNull()) {
        emitReady(path, QStringList(), QStringList(), QString());
        return;
    }
    result = materialYouFromImage(loaded, dark);
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
