#include "paletteextractor.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QPainter>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

class tst_PaletteExtractor : public QObject
{
    Q_OBJECT

private slots:
    void testMaterialYouFromImage();
    void testMaterialYouMonochrome();
    void testMaterialYouSchemes();
    void testSchemeSwitchIsCached();
    void testExtractionFailed();
    void testImageMagick();

private:
    static bool isValidHex(const QString &hex)
    {
        static const QRegularExpression re(QStringLiteral("^#[0-9a-f]{6}$"));
        return re.match(hex).hasMatch();
    }
    static QImage triColorImage()
    {
        // Four distinct quadrants: red, green, blue, orange.
        QImage image(128, 128, QImage::Format_RGB32);
        image.fill(QColor(Qt::black));
        QPainter painter(&image);
        painter.fillRect(0, 0, 64, 64, QColor(0xd4, 0x2a, 0x2a));
        painter.fillRect(64, 64, 64, 64, QColor(0x2a, 0xa0, 0x2a));
        painter.fillRect(64, 0, 64, 64, QColor(0x2a, 0x4a, 0xd4));
        painter.fillRect(0, 64, 64, 64, QColor(0xd4, 0xa0, 0x2a));
        return image;
    }
};

void tst_PaletteExtractor::testMaterialYouFromImage()
{
    const PaletteExtractor::MaterialYouResult first = PaletteExtractor::materialYouFromImage(triColorImage(), false);
    // 16 swatches in the M3 order: bg, primary 80/70, secondary 70/60,
    // tertiary 60/50, on-bg, neutral 50, containers, surface, on-surface.
    QCOMPARE(first.colors.size(), 16);
    for (const QString &hex : std::as_const(first.colors)) {
        QVERIFY(isValidHex(hex));
    }
    QCOMPARE(first.accents.size(), 3);
    for (const QString &hex : std::as_const(first.accents)) {
        QVERIFY(isValidHex(hex));
    }
    QVERIFY(isValidHex(first.seed));
    QCOMPARE(first.seed, first.accents.first());
    // Background (0) and on-background (7) must be distinguishable.
    QVERIFY(first.colors.first() != first.colors.at(7));

    // Deterministic for the same input.
    const PaletteExtractor::MaterialYouResult second = PaletteExtractor::materialYouFromImage(triColorImage(), false);
    QCOMPARE(second.colors, first.colors);
    QCOMPARE(second.accents, first.accents);
    QCOMPARE(second.seed, first.seed);

    // Dark and light schemes differ in background/foreground.
    const PaletteExtractor::MaterialYouResult dark = PaletteExtractor::materialYouFromImage(triColorImage(), true);
    QVERIFY(dark.colors.size() == 16);
    QVERIFY(dark.colors.first() != first.colors.first());
}

void tst_PaletteExtractor::testMaterialYouMonochrome()
{
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(QColor(0x80, 0x80, 0x80));
    const PaletteExtractor::MaterialYouResult result = PaletteExtractor::materialYouFromImage(image, false);
    // A monochromatic image still yields the full ramp.
    QCOMPARE(result.colors.size(), 16);
    for (const QString &hex : std::as_const(result.colors)) {
        QVERIFY(isValidHex(hex));
    }
}

void tst_PaletteExtractor::testMaterialYouSchemes()
{
    static const char *names[] = {"TonalSpot", "Vibrant", "Expressive", "Content",
                                  "FruitSalad", "Rainbow", "Monochrome", "Neutral", "Fidelity"};
    const PaletteExtractor::MaterialYouResult spot = PaletteExtractor::materialYouFromImage(triColorImage(), false);
    bool anyDifferent = false;
    for (const char *name : names) {
        const PaletteExtractor::MaterialYouResult result =
            PaletteExtractor::materialYouFromImage(triColorImage(), false, QLatin1String(name));
        QCOMPARE(result.colors.size(), 16);
        for (const QString &hex : std::as_const(result.colors)) {
            QVERIFY(isValidHex(hex));
        }
        if (result.colors != spot.colors) {
            anyDifferent = true;
        }
    }
    // At least one scheme must differ from Tonal Spot on this fixture.
    QVERIFY(anyDifferent);
    // Unknown scheme names fall back to Tonal Spot.
    const PaletteExtractor::MaterialYouResult unknown =
        PaletteExtractor::materialYouFromImage(triColorImage(), false, QStringLiteral("nosuchscheme"));
    QCOMPARE(unknown.colors, spot.colors);
}

void tst_PaletteExtractor::testSchemeSwitchIsCached()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/wall.png");
    QVERIFY(triColorImage().save(path, "PNG"));

    PaletteExtractor extractor;
    QSignalSpy ready(&extractor, &PaletteExtractor::paletteReady);
    extractor.extract(path, QStringLiteral("materialyou"), false, QStringLiteral("TonalSpot"));
    QTRY_VERIFY_WITH_TIMEOUT(ready.count() == 1, 10000);
    const QStringList spotColors = ready.takeFirst().at(1).toStringList();
    QVERIFY(!spotColors.isEmpty());

    // A scheme switch after the first extraction is answered from the
    // cache: the emission is synchronous, so the signal arrives before
    // extract() returns (no re-run of the extraction).
    extractor.extract(path, QStringLiteral("materialyou"), false, QStringLiteral("Neutral"));
    QCOMPARE(ready.count(), 1);
    const QStringList neutralColors = ready.takeFirst().at(1).toStringList();
    QVERIFY(!neutralColors.isEmpty());
    QVERIFY(spotColors != neutralColors);
}

void tst_PaletteExtractor::testExtractionFailed()
{
    PaletteExtractor extractor;
    QSignalSpy failed(&extractor, &PaletteExtractor::extractionFailed);
    extractor.extract(QStringLiteral("/nonexistent/wallpaper.png"), QStringLiteral("materialyou"), false);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.takeFirst().at(0).toString(), QStringLiteral("/nonexistent/wallpaper.png"));

    extractor.extract(QStringLiteral("/nonexistent/wallpaper.png"), QStringLiteral("nosuchbackend"), false);
    QCOMPARE(failed.count(), 1);
}

void tst_PaletteExtractor::testImageMagick()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/wall.png");
    QVERIFY(triColorImage().save(path, "PNG"));

    PaletteExtractor extractor;
    QSignalSpy ready(&extractor, &PaletteExtractor::paletteReady);
    QSignalSpy failed(&extractor, &PaletteExtractor::extractionFailed);
    extractor.extract(path, QStringLiteral("imagemagick"), false);

    // ImageMagick may or may not be installed; either outcome must be signalled.
    QTRY_VERIFY_WITH_TIMEOUT(ready.count() == 1 || failed.count() == 1, 10000);
    if (ready.count() == 1) {
        const QList<QVariant> args = ready.takeFirst().toList();
        QCOMPARE(args.at(0).toString(), path);
        const QStringList colors = args.at(1).toStringList();
        QVERIFY(!colors.isEmpty());
        QVERIFY(colors.size() <= 16);
        for (const QString &hex : colors) {
            QVERIFY(isValidHex(hex));
        }
        const QStringList accents = args.at(2).toStringList();
        QCOMPARE(accents.size(), 1);
        QVERIFY(isValidHex(accents.first()));
        QVERIFY(colors.contains(accents.first()));
    }
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations: set before the QCoreApplication is constructed so
    // QStandardPaths never touches the real home directory.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-paletteextractor-")
        + QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_PaletteExtractor test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_paletteextractor.moc"
