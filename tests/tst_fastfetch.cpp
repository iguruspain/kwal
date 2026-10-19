#include "fastfetchcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUrl>
#include <QStandardPaths>
#include <QtTest>

namespace {

QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/fastfetch/config.jsonc");
}

void writeConfig(const QString &content)
{
    const QString path = configPath();
    QDir(QFileInfo(path).absolutePath()).mkpath(QStringLiteral("."));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content.toUtf8());
}

void cleanConfig()
{
    QFile::remove(configPath());
    QFile::remove(configPath() + QStringLiteral(".bak"));
}

void saveImage(const QString &path, Qt::GlobalColor color)
{
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(color);
    QVERIFY(image.save(path));

}

bool hasImageMagick()
{
    return !QStandardPaths::findExecutable(QStringLiteral("magick")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("convert")).isEmpty();
}

}

class tst_Fastfetch : public QObject
{
    Q_OBJECT

private slots:
    void testConstructorAdoptsConfigImage();
    void testNoConfigStaysEmpty();
    void testManualSelectionSurvivesRefresh();
    void testAutoAdoptionTracksConfigChange();
    void testSelectFileNormalizesFileUrl();
    void testTintedPreviewUrlBumpsCacheBuster();
    void testConfigImageUrlBumpsOnRefresh();
    void testSetDraftColorRegeneratesPreview();
    void testTintedImagePathFor();
    void testManuallySelectedTracksUserPick();
};

void tst_Fastfetch::testConstructorAdoptsConfigImage()
{
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-adopt.png");
    saveImage(img, Qt::red);
    // The real-world fastfetch format: the image lives in a nested logo.source.
    writeConfig(QStringLiteral(
        "{\n"
        "  \"logo\": {\n"
        "    \"source\": \"%1\"\n"
        "  }\n"
        "}\n").arg(img));

    FastfetchController controller;
    QCOMPARE(controller.configImage(), QFileInfo(img).absoluteFilePath());
    QCOMPARE(controller.selectedFile(), QFileInfo(img).absoluteFilePath());

    QFile::remove(img);
    cleanConfig();
}

void tst_Fastfetch::testNoConfigStaysEmpty()
{
    cleanConfig();
    FastfetchController controller;
    QCOMPARE(controller.configImage().isEmpty(), true);
    QCOMPARE(controller.selectedFile().isEmpty(), true);
    cleanConfig();
}

void tst_Fastfetch::testManualSelectionSurvivesRefresh()
{
    cleanConfig();
    const QString manual = QDir::tempPath() + QStringLiteral("/kwal-test-ff-manual.png");
    const QString other = QDir::tempPath() + QStringLiteral("/kwal-test-ff-other.png");
    saveImage(manual, Qt::blue);
    saveImage(other, Qt::green);

    FastfetchController controller;
    controller.selectFile(manual);

    // A different image appears in the config; the user's pick must win.
    writeConfig(QStringLiteral("{ \"source\": \"%1\" }\n").arg(other));
    controller.refreshConfigImage();
    QCOMPARE(controller.selectedFile(), QFileInfo(manual).absoluteFilePath());

    QFile::remove(manual);
    QFile::remove(other);
    cleanConfig();
}

void tst_Fastfetch::testAutoAdoptionTracksConfigChange()
{
    cleanConfig();
    const QString a = QDir::tempPath() + QStringLiteral("/kwal-test-ff-a.png");
    const QString b = QDir::tempPath() + QStringLiteral("/kwal-test-ff-b.png");
    saveImage(a, Qt::red);
    saveImage(b, Qt::blue);

    writeConfig(QStringLiteral("{ \"source\": \"%1\" }\n").arg(a));
    FastfetchController controller;
    QCOMPARE(controller.selectedFile(), QFileInfo(a).absoluteFilePath());

    writeConfig(QStringLiteral("{ \"source\": \"%1\" }\n").arg(b));
    controller.refreshConfigImage();
    QCOMPARE(controller.selectedFile(), QFileInfo(b).absoluteFilePath());

    QFile::remove(a);
    QFile::remove(b);
    cleanConfig();
}

void tst_Fastfetch::testSelectFileNormalizesFileUrl()
{
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-url.png");
    saveImage(img, Qt::red);

    FastfetchController controller;
    // Pass the pick as QML's FileDialog hands it over: a file:// URL.
    controller.selectFile(QUrl::fromLocalFile(img).toString());
    QCOMPARE(controller.selectedFile(), QFileInfo(img).absoluteFilePath());

    QFile::remove(img);
    cleanConfig();
}

void tst_Fastfetch::testTintedPreviewUrlBumpsCacheBuster()
{
    if (!hasImageMagick())
        QSKIP("ImageMagick is not installed");
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-cb.png");
    saveImage(img, Qt::red);

    FastfetchController controller;
    controller.selectFile(img);
    controller.setDraftColor(QStringLiteral("#ff0000"));
    QVERIFY(!controller.tintedPreviewPath().isEmpty());
    const QString first = controller.tintedPreviewUrl();
    QVERIFY(first.startsWith(QUrl::fromLocalFile(controller.tintedPreviewPath()).toString()));
    QVERIFY(first.contains(QStringLiteral("?t=")));

    // Same cache path, new color: the URL must change so the QML Image
    // source changes and reloads.
    controller.setDraftColor(QStringLiteral("#00ff00"));
    QCOMPARE(controller.tintedPreviewUrl() != first, true);

    // Without a preview the URL is empty.
    controller.clearSelection();
    QCOMPARE(controller.tintedPreviewUrl().isEmpty(), true);

    QFile::remove(img);
    cleanConfig();
}

void tst_Fastfetch::testConfigImageUrlBumpsOnRefresh()
{
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-cfgurl.png");
    saveImage(img, Qt::red);
    writeConfig(QStringLiteral("{ \"source\": \"%1\" }\n").arg(img));

    FastfetchController controller;
    QVERIFY(!controller.configImage().isEmpty());
    const QString first = controller.configImageUrl();
    QVERIFY(first.contains(QStringLiteral("?t=")));

    // Re-detecting the same path must still produce a new URL.
    controller.refreshConfigImage();
    QCOMPARE(controller.configImage(), QFileInfo(img).absoluteFilePath());
    QCOMPARE(controller.configImageUrl() != first, true);

    // No config: both properties are empty.
    cleanConfig();
    controller.refreshConfigImage();
    QCOMPARE(controller.configImage().isEmpty(), true);
    QCOMPARE(controller.configImageUrl().isEmpty(), true);

    QFile::remove(img);
    cleanConfig();
}

void tst_Fastfetch::testSetDraftColorRegeneratesPreview()
{
    if (!hasImageMagick())
        QSKIP("ImageMagick is not installed");
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-dc.png");
    const QString img2 = QDir::tempPath() + QStringLiteral("/kwal-test-ff-dc2.png");
    saveImage(img, Qt::white);
    saveImage(img2, Qt::black);

    FastfetchController controller;
    controller.selectFile(img);
    QCOMPARE(controller.tintedPreviewPath().isEmpty(), true); // no tint yet

    controller.setDraftColor(QStringLiteral("#336699"));
    QVERIFY(!controller.tintedPreviewPath().isEmpty());

    // Selecting another image re-generates the preview automatically:
    // same cache file, so the cache-buster must change.
    const QString url1 = controller.tintedPreviewUrl();
    controller.selectFile(img2);
    QVERIFY(!controller.tintedPreviewPath().isEmpty());
    QCOMPARE(controller.tintedPreviewUrl() != url1, true);

    QFile::remove(img);
    QFile::remove(img2);
    cleanConfig();
}

void tst_Fastfetch::testTintedImagePathFor()
{
    FastfetchController controller;
    QCOMPARE(controller.tintedImagePathFor(QStringLiteral("/home/u/pics/wall.png")),
        QStringLiteral("/home/u/pics/wall-tinted.png"));
    QCOMPARE(controller.tintedImagePathFor(QStringLiteral("/home/u/pics/wall.jpg")),
        QStringLiteral("/home/u/pics/wall-tinted.jpg"));
}

void tst_Fastfetch::testManuallySelectedTracksUserPick()
{
    cleanConfig();
    const QString img = QDir::tempPath() + QStringLiteral("/kwal-test-ff-manual.png");
    saveImage(img, Qt::red);
    writeConfig(QStringLiteral("{ \"source\": \"%1\" }\n").arg(img));

    FastfetchController controller;
    // The config image is auto-adopted: not a manual pick.
    QVERIFY(!controller.selectedFile().isEmpty());
    QCOMPARE(controller.manuallySelected(), false);

    // A user pick flips it on.
    controller.selectFile(img);
    QCOMPARE(controller.manuallySelected(), true);

    // Clearing it flips it back off.
    controller.clearSelection();
    QCOMPARE(controller.manuallySelected(), false);

    QFile::remove(img);
    cleanConfig();
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations, set before the app object is constructed.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-fastfetch-")
        + QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_Fastfetch test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_fastfetch.moc"
