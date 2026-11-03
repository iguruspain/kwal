#include "wallpapersinfo.h"
#include "colorcategory.h"
#include "wallpapermodel.h"

#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QJsonDocument>
#include <QProcess>
#include <QSize>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

class tst_WallpapersInfo : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testRoundTrip();
    void testCorruptJson();
    void testFirstFrame();
    void testVideoResize();
    void testEnsureResizedCopy();
    void testColorCategory();
    void testMatugenEnabled();
    void testMatugenColors();
    void testMatugenUpToDate();
    void testCategoryFilter();
    void testSweep();
    void testRecalculateCategories();
    void testAutoCategoryCap();
    void testMergeTags();

private:
    static QString cacheRoot()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/kwal");
    }
    static void writeFile(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
        file.close();
    }
    static QString contentMd5(const QString &path)
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            hash.addData(file.readAll());
        }
        return QString::fromLatin1(hash.result().toHex());
    }
    static void writeImage(const QString &path, const QColor &color)
    {
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(color);
        QVERIFY(image.save(path, "PNG"));
    }
};

void tst_WallpapersInfo::initTestCase()
{
    WallpapersInfo::initialize(QCoreApplication::instance());
    QVERIFY(WallpapersInfo::instance() != nullptr);
    // Init creates the cache directories.
    QVERIFY(QDir(cacheRoot() + QStringLiteral("/video_frames")).exists());
}

void tst_WallpapersInfo::testRoundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/wall.png");
    writeFile(path, QByteArray(1234, 'x'));

    WallpapersInfo::instance()->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(WallpapersInfo::instance()->entryFor(path).lastModified > 0, 30000);

    const WallpapersInfo::Entry entry = WallpapersInfo::instance()->entryFor(path);
    QCOMPARE(entry.file, path);
    QCOMPARE(entry.type, QStringLiteral("image"));
    QVERIFY(entry.firstFrame.isEmpty());

    // The index is persisted as JSON next to the cache dirs.
    const QString json = cacheRoot() + QStringLiteral("/wallpapers_info.json");
    QVERIFY(QFileInfo(json).exists());
    QFile file(json);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value(QStringLiteral("files")).toArray().size(), 1);

    // A changed file (new mtime) produces a new entry on the next refresh.
    const double firstModified = entry.lastModified;
    QThread::msleep(50);
    writeFile(path, QByteArray(4321, 'y'));
    WallpapersInfo::instance()->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(WallpapersInfo::instance()->entryFor(path).lastModified != firstModified, 30000);

    // A fresh instance loads the same index from disk.
    WallpapersInfo::initialize(QCoreApplication::instance());
    QCOMPARE(WallpapersInfo::instance()->entryFor(path).type, QStringLiteral("image"));
    QVERIFY(WallpapersInfo::instance()->entryFor(path).lastModified != firstModified);
}

void tst_WallpapersInfo::testCorruptJson()
{
    const QString json = cacheRoot() + QStringLiteral("/wallpapers_info.json");
    writeFile(json, QByteArrayLiteral("{ not json"));

    WallpapersInfo::initialize(QCoreApplication::instance());
    QCOMPARE(WallpapersInfo::instance()->entryFor(QStringLiteral("/nonexistent")).lastModified, 0.0);

    // The corrupt file was rewritten as a valid empty index.
    QFile file(json);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(doc.isObject());
    QVERIFY(doc.object().value(QStringLiteral("files")).toArray().isEmpty());
}

void tst_WallpapersInfo::testFirstFrame()
{
    if (!WallpapersInfo::instance()->ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString video = dir.path() + QStringLiteral("/clip.mp4");
    // Synthesize a short 128x72 clip with lavfi.
    QProcess process;
    process.start(QStringLiteral("ffmpeg"),
                  {QStringLiteral("-v"),
                   QStringLiteral("error"),
                   QStringLiteral("-y"),
                   QStringLiteral("-f"),
                   QStringLiteral("lavfi"),
                   QStringLiteral("-i"),
                   QStringLiteral("color=c=blue:s=128x72:d=0.5:r=10"),
                   QStringLiteral("-c:v"),
                   QStringLiteral("libx264"),
                   QStringLiteral("-pix_fmt"),
                   QStringLiteral("yuv420p"),
                   video});
    QVERIFY(process.waitForFinished(60000));
    QCOMPARE(process.exitCode(), 0);
    QVERIFY(QFileInfo(video).size() > 0);

    WallpapersInfo::instance()->refreshFile(video, QStringLiteral("video"));
    QTRY_VERIFY_WITH_TIMEOUT(!WallpapersInfo::instance()->firstFrameFor(video).isEmpty(), 60000);

    const QString frame = WallpapersInfo::instance()->firstFrameFor(video);
    QVERIFY(QFileInfo(frame).size() > 0);
    // The frame is named after the MD5 of the video content.
    QCOMPARE(QFileInfo(frame).baseName(), contentMd5(video));
    // The extracted frame is a decodable PNG with the source resolution.
    QCOMPARE(QImageReader(frame).size(), QSize(128, 72));
}

void tst_WallpapersInfo::testVideoResize()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info->ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString video = dir.path() + QStringLiteral("/resize.mp4");
    // Synthesize a short 640x360@30 clip.
    QProcess process;
    process.start(QStringLiteral("ffmpeg"),
                  {QStringLiteral("-v"),
                   QStringLiteral("error"),
                   QStringLiteral("-y"),
                   QStringLiteral("-f"),
                   QStringLiteral("lavfi"),
                   QStringLiteral("-i"),
                   QStringLiteral("color=c=red:s=640x360:d=0.5:r=30"),
                   QStringLiteral("-c:v"),
                   QStringLiteral("libx264"),
                   QStringLiteral("-pix_fmt"),
                   QStringLiteral("yuv420p"),
                   video});
    QVERIFY(process.waitForFinished(60000));
    QCOMPARE(process.exitCode(), 0);

    // The scan no longer auto-generates resizes: refresh the entry first
    // (first frame), then run an explicit "generate resizes" batch.
    info->refreshFile(video, QStringLiteral("video"));
    QTRY_VERIFY_WITH_TIMEOUT(!info->entryFor(video).firstFrame.isEmpty(), 60000);

    // Generate (360 -> 240, fps kept) stores the cache copy path.
    info->setResizeParameters(240, 0);
    info->beginResizeBatch(1);
    info->planResize(video);
    QTRY_VERIFY_WITH_TIMEOUT(!info->resizing(), 60000);
    QString resized = info->entryFor(video).resized;
    QCOMPARE(QFileInfo(resized).fileName(), contentMd5(video) + QStringLiteral("_240p30.mp4"));
    QVERIFY(QFileInfo(resized).size() > 0);

    // A re-plan with settings that need no filter clears the cache copy
    // (the original is used).
    info->setResizeParameters(1080, 0);
    info->beginResizeBatch(1);
    info->planResize(video);
    QTRY_VERIFY_WITH_TIMEOUT(!info->resizing(), 30000);
    QVERIFY(info->entryFor(video).resized.isEmpty());

    // A lower fps is applied as an fps-only pass (no scaling: 360 < 480).
    info->setResizeParameters(480, 15);
    info->beginResizeBatch(1);
    info->planResize(video);
    QTRY_VERIFY_WITH_TIMEOUT(!info->resizing(), 60000);
    QVERIFY(info->entryFor(video).resized.endsWith(QStringLiteral("_360p15.mp4")));
}

void tst_WallpapersInfo::testEnsureResizedCopy()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info->ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString video = dir.path() + QStringLiteral("/ensure.mp4");
    // Synthesize a short 640x360@30 clip.
    QProcess process;
    process.start(QStringLiteral("ffmpeg"),
                  {QStringLiteral("-v"),
                   QStringLiteral("error"),
                   QStringLiteral("-y"),
                   QStringLiteral("-f"),
                   QStringLiteral("lavfi"),
                   QStringLiteral("-i"),
                   QStringLiteral("color=c=green:s=640x360:d=0.5:r=30"),
                   QStringLiteral("-c:v"),
                   QStringLiteral("libx264"),
                   QStringLiteral("-pix_fmt"),
                   QStringLiteral("yuv420p"),
                   video});
    QVERIFY(process.waitForFinished(60000));
    QCOMPARE(process.exitCode(), 0);

    QSignalSpy started(info, &WallpapersInfo::resizeCopyStarted);
    QSignalSpy finished(info, &WallpapersInfo::resizeCopyFinished);

    // No cached copy yet: the copy is generated, returned, and stored in
    // the entry (blocking call, no batch involved).
    info->setResizeParameters(240, 0);
    const QString copy = info->ensureResizedCopy(video);
    QCOMPARE(QFileInfo(copy).fileName(), contentMd5(video) + QStringLiteral("_240p30.mp4"));
    QVERIFY(QFileInfo(copy).size() > 0);
    QCOMPARE(info->entryFor(video).resized, copy);
    QCOMPARE(started.count(), 1);
    QCOMPARE(started.at(0).at(0).toString(), video);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toString(), video);
    QCOMPARE(finished.at(0).at(1).toBool(), true);

    // A second call reuses the existing copy without re-encoding.
    const QString copy2 = info->ensureResizedCopy(video);
    QCOMPARE(copy2, copy);
    QCOMPARE(info->entryFor(video).resized, copy);
    QCOMPARE(started.count(), 2);
    QCOMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(1).toBool(), true);

    // Settings that call for no filter return "" and clear the stored
    // copy (the original is applied as-is), like the batch job.
    info->setResizeParameters(1080, 0);
    const QString none = info->ensureResizedCopy(video);
    QVERIFY(none.isEmpty());
    QVERIFY(info->entryFor(video).resized.isEmpty());
    QCOMPARE(finished.count(), 3);
    QCOMPARE(finished.at(2).at(1).toBool(), true);
}

void tst_WallpapersInfo::testColorCategory()
{
    // D4: black/white/gray + 12 hue sectors with the plan's boundaries.
    const auto argb = [](const QString &hex) {
        return static_cast<quint32>(0xFF000000) | static_cast<quint32>(hex.mid(1).toUInt(nullptr, 16));
    };
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#000000"))), QStringLiteral("black"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FFFFFF"))), QStringLiteral("white"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#808080"))), QStringLiteral("gray"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FF0000"))), QStringLiteral("red"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FF8000"))), QStringLiteral("orange"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FFD400"))), QStringLiteral("yellow"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#99FF00"))), QStringLiteral("yellow-green"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#00FF00"))), QStringLiteral("green"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#00FFA3"))), QStringLiteral("cyan-green"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#00FFFF"))), QStringLiteral("cyan"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#00B3FF"))), QStringLiteral("blue-cyan"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#0000FF"))), QStringLiteral("blue"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#9932CC"))), QStringLiteral("violet"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FF00FF"))), QStringLiteral("magenta"));
    QCOMPARE(colorcategory::colorCategory(argb(QStringLiteral("#FF6699"))), QStringLiteral("rose"));
}

void tst_WallpapersInfo::testMatugenEnabled()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    QSignalSpy spy(info, &WallpapersInfo::matugenEnabledChanged);
    QVERIFY(!info->matugenEnabled());
    info->setMatugenEnabled(true);
    QVERIFY(info->matugenEnabled());
    QCOMPARE(spy.count(), 1);
    info->setMatugenEnabled(true);
    QCOMPARE(spy.count(), 1);
    info->setMatugenEnabled(false);
    QVERIFY(!info->matugenEnabled());
    QCOMPARE(spy.count(), 2);
}

void tst_WallpapersInfo::testMatugenColors()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info->matugenAvailable()) {
        QSKIP("matugen not available");
    }
    info->setMatugenEnabled(true);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/solid.png");
    writeImage(path, QColor(0, 0, 255));

    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(!info->sourceColorsFor(path).isEmpty(), 30000);

    const QStringList colors = info->sourceColorsFor(path);
    QVERIFY(!colors.isEmpty());
    QVERIFY(colors.size() <= 8);
    const QStringList categories = info->colorCategoriesFor(path);
    QCOMPARE(categories, QStringList{QStringLiteral("blue")});

    info->setMatugenEnabled(false);
}

void tst_WallpapersInfo::testMatugenUpToDate()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info->matugenAvailable()) {
        QSKIP("matugen not available");
    }
    info->setMatugenEnabled(true);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/done.png");
    writeImage(path, QColor(0, 0, 255));

    const double mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch() / 1000.0;

    // Index where matugen already ran for this file and the palette came out
    // empty: with a matching mtime the entry is up to date and must not be
    // re-processed on the next refresh.
    QJsonObject obj;
    obj[QStringLiteral("file")] = path;
    obj[QStringLiteral("type")] = QStringLiteral("image");
    obj[QStringLiteral("last_modified")] = mtime;
    obj[QStringLiteral("matugen_done")] = true;
    obj[QStringLiteral("color_categories")] = QJsonArray{QStringLiteral("blue")};
    QJsonArray files;
    files.append(obj);
    QJsonObject root;
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("files")] = files;
    writeFile(cacheRoot() + QStringLiteral("/wallpapers_info.json"), QJsonDocument(root).toJson());

    WallpapersInfo::initialize(QCoreApplication::instance());
    info = WallpapersInfo::instance();
    // A fresh instance starts with matugen disabled; the toggle is per-instance.
    info->setMatugenEnabled(true);

    QSignalSpy changed(info, &WallpapersInfo::entryChanged);
    QSignalSpy finished(info, &WallpapersInfo::scanFinished);
    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, 30000);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(info->entryFor(path).matugenDone, true);

    // Without the flag the same entry is stale and gets re-processed.
    obj.remove(QStringLiteral("matugen_done"));
    files.replace(0, obj);
    root[QStringLiteral("files")] = files;
    writeFile(cacheRoot() + QStringLiteral("/wallpapers_info.json"), QJsonDocument(root).toJson());
    WallpapersInfo::initialize(QCoreApplication::instance());
    info = WallpapersInfo::instance();
    info->setMatugenEnabled(true);
    QSignalSpy changed2(info, &WallpapersInfo::entryChanged);
    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(changed2.count() >= 1, 30000);
    QVERIFY(info->entryFor(path).matugenDone);

    info->setMatugenEnabled(false);
}

void tst_WallpapersInfo::testCategoryFilter()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info->matugenAvailable()) {
        QSKIP("matugen not available");
    }
    info->setMatugenEnabled(true);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bluePath = dir.path() + QStringLiteral("/blue.png");
    const QString greenPath = dir.path() + QStringLiteral("/green.png");
    writeImage(bluePath, QColor(0, 0, 255));
    writeImage(greenPath, QColor(0, 255, 163));

    WallpaperModel model;
    model.setSourceDirectory(dir.path());
    QCOMPARE(model.rowCount(), 2);

    info->refreshFile(bluePath, QStringLiteral("image"));
    info->refreshFile(greenPath, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(!info->sourceColorsFor(bluePath).isEmpty(), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!info->sourceColorsFor(greenPath).isEmpty(), 30000);

    // Category chips filter with AND semantics.
    model.setCategoryFilter(QStringList{QStringLiteral("blue")});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.pathAt(0), bluePath);
    model.setCategoryFilter(QStringList{QStringLiteral("blue"), QStringLiteral("green")});
    QCOMPARE(model.rowCount(), 0);
    model.setCategoryFilter(QStringList());
    QCOMPARE(model.rowCount(), 2);

    // The text filter still applies on top of the category filter.
    model.setFilterText(QStringLiteral("green"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.pathAt(0), greenPath);

    model.setFilterText(QString());
    info->setMatugenEnabled(false);
}

void tst_WallpapersInfo::testSweep()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString keptPath = dir.path() + QStringLiteral("/kept.png");
    const QString videoPath = dir.path() + QStringLiteral("/live.mp4");
    const QString deadPath = dir.path() + QStringLiteral("/gone.mp4");
    writeFile(keptPath, QByteArray(100, 'k'));
    writeFile(videoPath, QByteArray(100, 'v'));

    // Fixture index: a live image, a live video referencing its cached
    // frame and resize copy, and a dead video. Stray frame and resize
    // files sit in the cache dirs alongside the referenced ones.
    const QString liveFrame = cacheRoot() + QStringLiteral("/video_frames/live.png");
    const QString orphanFrame = cacheRoot() + QStringLiteral("/video_frames/orphan.png");
    const QString deadFrame = cacheRoot() + QStringLiteral("/video_frames/dead.png");
    const QString resizesDir = cacheRoot() + QStringLiteral("/video_resizes");
    QVERIFY(QDir().mkpath(resizesDir));
    const QString liveResize = resizesDir + QStringLiteral("/live.mp4");
    const QString orphanResize = resizesDir + QStringLiteral("/orphan.mp4");
    writeFile(liveFrame, QByteArray("frame"));
    writeFile(orphanFrame, QByteArray("orphan"));
    writeFile(deadFrame, QByteArray("deadframe"));
    writeFile(liveResize, QByteArray("resize"));
    writeFile(orphanResize, QByteArray("orphanresize"));

    const QJsonObject kept = {
        {QStringLiteral("file"), keptPath},
        {QStringLiteral("type"), QStringLiteral("image")},
        {QStringLiteral("last_modified"), 1.0},
    };
    const QJsonObject video = {
        {QStringLiteral("file"), videoPath},
        {QStringLiteral("type"), QStringLiteral("video")},
        {QStringLiteral("first_frame"), liveFrame},
        {QStringLiteral("resized"), liveResize},
        {QStringLiteral("last_modified"), 1.0},
    };
    const QJsonObject dead = {
        {QStringLiteral("file"), deadPath},
        {QStringLiteral("type"), QStringLiteral("video")},
        {QStringLiteral("first_frame"), deadFrame},
        {QStringLiteral("last_modified"), 1.0},
    };
    QJsonObject root;
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("files")] = QJsonArray{kept, video, dead};
    writeFile(cacheRoot() + QStringLiteral("/wallpapers_info.json"), QJsonDocument(root).toJson());

    WallpapersInfo::initialize(QCoreApplication::instance());
    WallpapersInfo *info = WallpapersInfo::instance();

    QSignalSpy purged(info, &WallpapersInfo::purged);
    info->sweep();
    QCOMPARE(purged.count(), 1);

    // The dead entry is gone from the index; the live entries survive with
    // their cache references intact.
    QCOMPARE(info->entryFor(deadPath).lastModified, 0.0);
    QVERIFY(info->entryFor(keptPath).lastModified > 0);
    const WallpapersInfo::Entry live = info->entryFor(videoPath);
    QCOMPARE(live.firstFrame, liveFrame);
    QCOMPARE(live.resized, liveResize);
    // The orphan frame and resize copy are deleted, as is the frame of the
    // dead entry; the referenced frame and resize stay.
    QVERIFY(!QFileInfo(orphanFrame).exists());
    QVERIFY(!QFileInfo(deadFrame).exists());
    QVERIFY(QFileInfo(liveFrame).exists());
    QVERIFY(!QFileInfo(orphanResize).exists());
    QVERIFY(QFileInfo(liveResize).exists());
    // The JSON no longer lists the dead file.
    const QString json = cacheRoot() + QStringLiteral("/wallpapers_info.json");
    QFile file(json);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : doc.object().value(QStringLiteral("files")).toArray()) {
        QVERIFY(value.toObject().value(QStringLiteral("file")).toString() != deadPath);
    }

    // A sweep with nothing to clean up is a no-op.
    QSignalSpy changed(info, &WallpapersInfo::entryChanged);
    QSignalSpy purged2(info, &WallpapersInfo::purged);
    info->sweep();
    QCOMPARE(changed.count(), 0);
    QCOMPARE(purged2.count(), 0);
}

void tst_WallpapersInfo::testRecalculateCategories()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/solid.png");
    writeImage(path, QColor(0, 0, 255)); // -> "blue"

    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(info->colorCategoriesFor(path) == QStringList{QStringLiteral("blue")}, 30000);

    // A manual override replaces the automatic categories...
    info->setCategories(path, QStringList{QStringLiteral("rose")});
    QCOMPARE(info->entryFor(path).categoriesManual, true);
    QCOMPARE(info->colorCategoriesFor(path), QStringList{QStringLiteral("rose")});

    // ...and recalculateCategories restores the computed ones, clearing the flag.
    QSignalSpy changed(info, &WallpapersInfo::entryChanged);
    info->recalculateCategories(path);
    QTRY_VERIFY_WITH_TIMEOUT(info->colorCategoriesFor(path) == QStringList{QStringLiteral("blue")}, 30000);
    QCOMPARE(info->entryFor(path).categoriesManual, false);
    QCOMPARE(changed.count(), 1);

    // Unknown path is a no-op.
    info->recalculateCategories(QStringLiteral("/nonexistent/missing.png"));
    QCOMPARE(changed.count(), 1);

    // The recalculated entry is up to date again: a rescan keeps the
    // automatic categories without reprocessing.
    QSignalSpy rescan(info, &WallpapersInfo::entryChanged);
    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(info->colorCategoriesFor(path) == QStringList{QStringLiteral("blue")}, 30000);
    QCOMPARE(rescan.count(), 0);
}

void tst_WallpapersInfo::testAutoCategoryCap()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/stripes.png");
    // Eight solid stripes in eight different categories: the top-8 colors
    // are all distinct, so the automatic categories hit the cap (5).
    QImage image(80, 20, QImage::Format_RGB32);
    const QList<QColor> stripes = {
        QColor(200, 0, 0),
        QColor(230, 110, 0),
        QColor(220, 220, 0),
        QColor(0, 150, 0),
        QColor(0, 0, 220),
        QColor(150, 0, 150),
        QColor(245, 245, 245),
        QColor(15, 15, 15)
    };
    QPainter painter(&image);
    for (int i = 0; i < stripes.size(); ++i) {
        painter.fillRect(i * 10, 0, 10, image.height(), stripes[i]);
    }
    painter.end();
    QVERIFY(image.save(path, "PNG"));

    info->refreshFile(path, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(info->colorCategoriesFor(path).size() == 5, 30000);

    // Manual edits are not capped: all 15 categories can be assigned.
    info->setCategories(path, colorcategory::categoryList());
    QCOMPARE(info->colorCategoriesFor(path).size(), 15);
    QCOMPARE(info->entryFor(path).categoriesManual, true);
}

void tst_WallpapersInfo::testMergeTags()
{
    WallpapersInfo *info = WallpapersInfo::instance();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/sunflower.jpg");

    // Merge into an entry with no tags creates them; the manual flag keeps
    // them across rescans.
    info->mergeTags(path, {QStringLiteral("landscape")});
    QCOMPARE(info->tagsFor(path), QStringList{QStringLiteral("landscape")});
    QCOMPARE(info->entryFor(path).tagsManual, true);

    // Existing tags survive; duplicates are dropped case-insensitively and
    // the existing order is kept.
    info->mergeTags(path, {QStringLiteral("LANDSCAPE"), QStringLiteral("sunflower"), QStringLiteral("photo")});
    QCOMPARE(info->tagsFor(path),
             (QStringList{QStringLiteral("landscape"), QStringLiteral("sunflower"), QStringLiteral("photo")}));

    // A second merge adds only what is new.
    info->mergeTags(path, {QStringLiteral("sunflower"), QStringLiteral("clouds")});
    QCOMPARE(info->tagsFor(path),
             (QStringList{QStringLiteral("landscape"), QStringLiteral("sunflower"),
                          QStringLiteral("photo"), QStringLiteral("clouds")}));
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations: set before the QCoreApplication is constructed so
    // QStandardPaths never touches the real home directory.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-wallpapersinfo-")
        + QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_WallpapersInfo test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_wallpapersinfo.moc"
