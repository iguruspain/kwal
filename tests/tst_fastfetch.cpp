#include "fastfetchcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
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

QString magickExe()
{
    const QString magick = QStandardPaths::findExecutable(QStringLiteral("magick"));
    return magick.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("convert")) : magick;
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
    void testAnimatedPreviewCacheLayout();
    void testApplyCopiesPreviewWithoutRetinting();
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
    QSignalSpy previewSpy(&controller, &FastfetchController::tintedPreviewChanged);
    controller.selectFile(img);
    controller.setDraftColor(QStringLiteral("#ff0000"));
    QVERIFY(previewSpy.wait(10000));
    QVERIFY(!controller.tintedPreviewPath().isEmpty());
    const QString first = controller.tintedPreviewUrl();
    QVERIFY(first.startsWith(QUrl::fromLocalFile(controller.tintedPreviewPath()).toString()));
    QVERIFY(first.contains(QStringLiteral("?t=")));

    // Same cache path, new color: the URL must change so the QML Image
    // source changes and reloads.
    controller.setDraftColor(QStringLiteral("#00ff00"));
    QVERIFY(previewSpy.wait(10000));
    QCOMPARE(controller.tintedPreviewUrl() != first, true);

    // Without a preview the URL is empty.
    controller.clearSelection();
    QCOMPARE(controller.tintedPreviewUrl().isEmpty(), true);

    QFile::remove(img);
    cleanConfig();
}

void tst_Fastfetch::testAnimatedPreviewCacheLayout()
{
    if (!hasImageMagick())
        QSKIP("ImageMagick is not installed");
    cleanConfig();

    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/kwal");
    QDir().mkpath(cacheRoot);
    // A stray file from the old flat layout (where an animated source
    // exploded into one PNG per frame): generating a preview must clear it.
    const QString legacy = cacheRoot + QStringLiteral("/fastfetch-preview-0.png");
    saveImage(legacy, Qt::red);

    const QString gif = QDir::tempPath() + QStringLiteral("/kwal-test-ff-anim.gif");
    QFile::remove(gif);
    QProcess gen;
    gen.start(magickExe(), {QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                            QStringLiteral("xc:red"), QStringLiteral(")"),
                            QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                            QStringLiteral("xc:blue"), QStringLiteral(")"), gif});
    QVERIFY(gen.waitForFinished(10000));
    {
        QImageReader check(gif);
        if (!check.canRead() || check.imageCount() != 2)
            QSKIP("could not generate the test GIF fixture");
    }

    FastfetchController controller;
    QSignalSpy previewSpy(&controller, &FastfetchController::tintedPreviewChanged);
    controller.selectFile(gif);
    controller.setDraftColor(QStringLiteral("#00c80a"));
    QVERIFY(previewSpy.wait(30000));
    QVERIFY(!controller.tintedPreviewPath().isEmpty());

    // The preview lives in its own cache folder and keeps the source's
    // animated family: one tinted animated GIF, not a per-frame dump.
    QCOMPARE(controller.tintedPreviewPath(),
        cacheRoot + QStringLiteral("/fastfetch_preview/fastfetch-preview.gif"));
    QImageReader out(controller.tintedPreviewPath());
    QVERIFY(out.canRead());
    QCOMPARE(out.imageCount(), 2);
    QVERIFY(!QFile::exists(controller.tintedPreviewPath() + QStringLiteral("-0.gif")));

    // The old-layout leftovers are gone; only the current preview remains
    // in the preview folder.
    QVERIFY(!QFile::exists(legacy));
    const QStringList kept = QDir(cacheRoot + QStringLiteral("/fastfetch_preview"))
        .entryList(QDir::Files | QDir::Hidden);
    QCOMPARE(kept.size(), 1);

    controller.clearSelection();
    QFile::remove(gif);
    cleanConfig();
}

void tst_Fastfetch::testApplyCopiesPreviewWithoutRetinting()
{
    if (!hasImageMagick())
        QSKIP("ImageMagick is not installed");
    cleanConfig();

    const QString gif = QDir::tempPath() + QStringLiteral("/kwal-test-ff-apply.gif");
    QFile::remove(gif);
    QProcess gen;
    gen.start(magickExe(), {QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                            QStringLiteral("xc:red"), QStringLiteral(")"),
                            QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                            QStringLiteral("xc:blue"), QStringLiteral(")"), gif});
    QVERIFY(gen.waitForFinished(10000));
    {
        QImageReader check(gif);
        if (!check.canRead() || check.imageCount() != 2)
            QSKIP("could not generate the test GIF fixture");
    }

    // Config points at the gif so the controller adopts it and the apply
    // can rewrite the source key.
    writeConfig(QStringLiteral("{\n  source: \"%1\"\n}\n").arg(gif));
    FastfetchController controller;
    QSignalSpy previewSpy(&controller, &FastfetchController::tintedPreviewChanged);
    controller.selectFile(gif);
    controller.setDraftColor(QStringLiteral("#00c80a"));
    QVERIFY(previewSpy.wait(30000));

    // Apply copies the finished preview next to the source: the applied
    // file is byte-identical to the preview (no second tint pass), keeps
    // every frame, and the config starts pointing at it.
    const QString previewFile = controller.tintedPreviewPath();
    QSignalSpy resultSpy(&controller, &FastfetchController::operationResult);
    controller.applyTintedImage();
    // The copy is synchronous when the preview is up to date; QTRY covers
    // both that and the queued path without missing an early emission.
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 1, 30000);
    QCOMPARE(resultSpy.at(0).at(0).toBool(), true);

    const QString applied = controller.tintedImagePathFor(gif);
    QVERIFY(QFile::exists(applied));
    QFile p1(previewFile);
    QFile p2(applied);
    QVERIFY(p1.open(QIODevice::ReadOnly));
    QVERIFY(p2.open(QIODevice::ReadOnly));
    QCOMPARE(p2.readAll(), p1.readAll());
    QImageReader out(applied);
    QVERIFY(out.canRead());
    QCOMPARE(out.imageCount(), 2);
    const QRgb first = out.read().pixel(8, 8);
    QVERIFY(qGreen(first) > qRed(first));
    QString configText;
    QFile cfgFile(configPath());
    if (cfgFile.open(QIODevice::ReadOnly))
        configText = QString::fromUtf8(cfgFile.readAll());
    QVERIFY(configText.contains(applied));

    // Clicking apply while a fresh pass is still running must not re-tint
    // nor copy a stale file: the queued apply lands on the new result.
    // The pass exposes its own previewBusy; the apply busy stays free.
    controller.setDraftColor(QStringLiteral("#0000ff"));
    QVERIFY(controller.previewBusy());
    QCOMPARE(controller.busy(), false);
    controller.applyTintedImage();
    QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 2, 30000);
    QCOMPARE(resultSpy.last().at(0).toBool(), true);
    QVERIFY(!controller.previewBusy());
    QVERIFY(!controller.busy());
    QImageReader queuedOut(applied);
    QVERIFY(queuedOut.canRead());
    QCOMPARE(queuedOut.imageCount(), 2);
    const QRgb queued = queuedOut.read().pixel(8, 8);
    QVERIFY(qBlue(queued) > qRed(queued));
    QVERIFY(qBlue(queued) > qGreen(queued));

    controller.clearSelection();
    QFile::remove(gif);
    QFile::remove(applied);
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
    QSignalSpy previewSpy(&controller, &FastfetchController::tintedPreviewChanged);
    controller.selectFile(img);
    QCOMPARE(controller.tintedPreviewPath().isEmpty(), true); // no tint yet

    controller.setDraftColor(QStringLiteral("#336699"));
    QVERIFY(previewSpy.wait(10000));
    QVERIFY(!controller.tintedPreviewPath().isEmpty());

    // Selecting another image re-generates the preview automatically:
    // same cache file, so the cache-buster must change.
    const QString url1 = controller.tintedPreviewUrl();
    controller.selectFile(img2);
    QVERIFY(previewSpy.wait(10000));
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
        QStringLiteral("/home/u/pics/tinted/wall-tinted.png"));
    QCOMPARE(controller.tintedImagePathFor(QStringLiteral("/home/u/pics/wall.jpg")),
        QStringLiteral("/home/u/pics/tinted/wall-tinted.jpg"));
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
