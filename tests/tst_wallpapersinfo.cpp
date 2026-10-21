#include "wallpapersinfo.h"
#include "wallpapermodel.h"

#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
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
    void testColorCategory();
    void testMatugenEnabled();
    void testMatugenColors();
    void testMatugenUpToDate();
    void testCategoryFilter();
    void testSweep();

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

void tst_WallpapersInfo::testColorCategory()
{
    // D4: black/white/gray + 12 hue sectors with the plan's boundaries.
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#000000")), QStringLiteral("black"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#FFFFFF")), QStringLiteral("white"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#808080")), QStringLiteral("gray"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#FF0000")), QStringLiteral("rose"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#FF4D00")), QStringLiteral("red"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#FFD400")), QStringLiteral("orange"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#99FF00")), QStringLiteral("yellow"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#00FF00")), QStringLiteral("yellow-green"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#00FFA3")), QStringLiteral("green"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#00FFFF")), QStringLiteral("cyan-green"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#00B3FF")), QStringLiteral("cyan"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#4D7FFF")), QStringLiteral("blue-cyan"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#0000FF")), QStringLiteral("blue"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#9932CC")), QStringLiteral("violet"));
    QCOMPARE(WallpapersInfo::colorCategory(QStringLiteral("#FF00FF")), QStringLiteral("magenta"));
    QCOMPARE(WallpapersInfo::colorCategory(QString()), QString());
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
    QJsonArray files;
    files.append(obj);
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
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
    WallpapersInfo *info = WallpapersInfo::instance();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString keptPath = dir.path() + QStringLiteral("/kept.png");
    const QString removedPath = dir.path() + QStringLiteral("/removed.png");
    writeFile(keptPath, QByteArray(100, 'k'));
    writeFile(removedPath, QByteArray(100, 'r'));

    info->refreshFile(keptPath, QStringLiteral("image"));
    info->refreshFile(removedPath, QStringLiteral("image"));
    QTRY_VERIFY_WITH_TIMEOUT(info->entryFor(keptPath).lastModified > 0, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(info->entryFor(removedPath).lastModified > 0, 30000);

    // Orphaned frame left in the cache frames dir.
    const QString orphanFrame = cacheRoot() + QStringLiteral("/video_frames/orphan.png");
    writeFile(orphanFrame, QByteArray("orphan"));

    QFile::remove(removedPath);
    info->sweep();

    // The deleted file's entry is gone from the index; the survivor stays.
    QCOMPARE(info->entryFor(removedPath).lastModified, 0.0);
    QVERIFY(info->entryFor(keptPath).lastModified > 0);
    // The orphan frame is deleted and the JSON no longer lists the file.
    QVERIFY(!QFileInfo(orphanFrame).exists());
    const QString json = cacheRoot() + QStringLiteral("/wallpapers_info.json");
    QFile file(json);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : doc.object().value(QStringLiteral("files")).toArray()) {
        QVERIFY(value.toObject().value(QStringLiteral("file")).toString() != removedPath);
    }

    // A sweep with nothing to clean up is a no-op.
    QSignalSpy changed(info, &WallpapersInfo::entryChanged);
    info->sweep();
    QCOMPARE(changed.count(), 0);
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
