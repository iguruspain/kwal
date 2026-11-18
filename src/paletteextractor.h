#pragma once

#include <QHash>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>

// M3 palettes via material-color-utilities (Celebi + 9-scheme selector) or
// ImageMagick. Signals always emit on the object's thread.
class PaletteExtractor : public QObject
{
    Q_OBJECT
public:
    explicit PaletteExtractor(QObject *parent = nullptr);

    // backend: "materialyou" (scheme = any of the nine M3 names, else TonalSpot)
    // or "imagemagick"; videos use the cached first frame (WallpapersInfo must be
    // initialized). Schemes are cached per dark mode so repeat calls skip Celebi.
    Q_INVOKABLE void extract(const QString &path,
                             const QString &backend,
                             bool dark,
                             const QString &scheme = QStringLiteral("TonalSpot"));

    // M3 scheme of an image: 16 swatches, top-3 ranked colors, primary seed.
    struct MaterialYouResult {
        QStringList colors;
        QStringList accents;
        QString seed;
    };
    static MaterialYouResult materialYouFromImage(const QImage &image,
                                                  bool dark,
                                                  const QString &scheme = QStringLiteral("TonalSpot"));
    // All nine M3 schemes in one pass: scaling, Celebi and ranking run once;
    // each scheme is a cheap recombination. Keyed by scheme name; empty on failure.
    static QMap<QString, MaterialYouResult>
        materialYouSchemesFromImage(const QImage &image, bool dark);

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

    // "path|dark"/"path|light" -> nine schemes of one pass. Pool writes,
    // owning thread reads: guarded by m_cacheMutex.
    QHash<QString, QMap<QString, MaterialYouResult>> m_schemeCache;
    QMutex m_cacheMutex;
    QThreadPool m_pool;
};
