#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>

// Wallpapers palettes: Material You (M3) schemes via Google's
// material-color-utilities (Celebi quantization + 9-scheme selector) and
// ImageMagick unique colors + best accent.
// Extraction runs on a thread pool; signals are always emitted on the
// thread the object lives on.
class PaletteExtractor : public QObject
{
    Q_OBJECT
public:
    explicit PaletteExtractor(QObject *parent = nullptr);

    // backend: "materialyou" or "imagemagick". For videos the cached first
    // frame is used (WallpapersInfo must be initialized). scheme: one of the
    // nine M3 scheme names ("TonalSpot", "Vibrant", "Expressive", "Content",
    // "FruitSalad", "Rainbow", "Monochrome", "Neutral", "Fidelity"); any
    // other value falls back to TonalSpot. Only used by "materialyou".
    Q_INVOKABLE void extract(const QString &path,
                             const QString &backend,
                             bool dark,
                             const QString &scheme = QStringLiteral("TonalSpot"));

    // Material You (M3) scheme of an image: 16 swatches (D6),
    // the top-3 ranked colors as accents and the primary seed as seed.
    // scheme as in extract().
    struct MaterialYouResult {
        QStringList colors;
        QStringList accents;
        QString seed;
    };
    static MaterialYouResult materialYouFromImage(const QImage &image,
                                                  bool dark,
                                                  const QString &scheme = QStringLiteral("TonalSpot"));

signals:
    void paletteReady(const QString &path,
                      const QStringList &colors,
                      const QStringList &accents,
                      const QString &seed);
    void extractionFailed(const QString &path, const QString &message);

private:
    // Pool job; nested so it can call the private process* methods.
    class ExtractJob;
    void processMaterialYou(const QString &path, const QString &image, bool dark, const QString &scheme);
    void processImageMagick(const QString &path, const QString &image);
    void emitReady(const QString &path,
                   const QStringList &colors,
                   const QStringList &accents,
                   const QString &seed);
    void emitFailed(const QString &path, const QString &message);

    QThreadPool m_pool;
};
