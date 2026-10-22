#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>

// Wallpapers palettes: a simplified Material You scheme (median-cut + HSL
// tone ramps, no OKLCH) and ImageMagick unique colors + best accent.
// Extraction runs on a thread pool; signals are always emitted on the
// thread the object lives on.
class PaletteExtractor : public QObject
{
    Q_OBJECT
public:
    explicit PaletteExtractor(QObject *parent = nullptr);

    // backend: "materialyou" or "imagemagick". For videos the cached first
    // frame is used (WallpapersInfo must be initialized).
    Q_INVOKABLE void extract(const QString &path, const QString &backend, bool dark);

    // Simplified Material You scheme of an image: 12 swatches (bg, fg,
    // primary/secondary/tertiary tone steps, neutral mid), the three seed
    // colors as accents and the primary seed as seed. Pure, thread-safe.
    struct MaterialYouResult {
        QStringList colors;
        QStringList accents;
        QString seed;
    };
    static MaterialYouResult materialYouFromImage(const QImage &image, bool dark);

signals:
    void paletteReady(const QString &path,
                      const QStringList &colors,
                      const QStringList &accents,
                      const QString &seed);
    void extractionFailed(const QString &path, const QString &message);

private:
    // Pool job; nested so it can call the private process* methods.
    class ExtractJob;
    void processMaterialYou(const QString &path, const QString &image, bool dark);
    void processImageMagick(const QString &path, const QString &image);
    void emitReady(const QString &path,
                   const QStringList &colors,
                   const QStringList &accents,
                   const QString &seed);
    void emitFailed(const QString &path, const QString &message);

    QThreadPool m_pool;
};
