#include "appcontroller.h"
#include "wallpapermodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QtTest>

class tst_AppController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testWallpaperModel();
    void testAutoRefresh();
    void testFoldersAndConfig();
    void testSelectionAndApply();
    void testWindowSettings();
    void testTransparencyPersistence();
    void testToolDetection();

private:
    static void saveImage(const QString &path, Qt::GlobalColor color)
    {
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(color);
        image.save(path);
    }
};

void tst_AppController::initTestCase()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString a = pictures + QStringLiteral("/A");
    const QString b = pictures + QStringLiteral("/B");
    QDir().mkpath(a);
    QDir().mkpath(b);
    saveImage(a + QStringLiteral("/alpha.png"), Qt::red);
    saveImage(a + QStringLiteral("/beta.png"), Qt::blue);
    QFile notes(a + QStringLiteral("/notes.txt"));
    QVERIFY(notes.open(QIODevice::WriteOnly));
    notes.write("not an image");
    saveImage(b + QStringLiteral("/gamma.png"), Qt::green);
}

void tst_AppController::testWallpaperModel()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    AppController app;
    // Fresh config falls back to the pictures folder, which has no direct images.
    QCOMPARE(app.folders().size(), 1);
    QCOMPARE(app.currentFolderIndex(), 0);

    WallpaperModel *model = app.wallpaperModel();
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 0);

    model->setSourceDirectory(pictures + QStringLiteral("/A"));
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(QFileInfo(model->pathAt(0)).fileName(), QStringLiteral("alpha.png"));
    QCOMPARE(QFileInfo(model->pathAt(1)).fileName(), QStringLiteral("beta.png"));

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, WallpaperModel::NameRole).toString(), QStringLiteral("alpha.png"));
    QVERIFY(!model->data(index, WallpaperModel::ThumbnailRole).toUrl().isEmpty());
    QCOMPARE(model->data(index, WallpaperModel::ExtensionRole).toString(), QStringLiteral("png"));
}

void tst_AppController::testAutoRefresh()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString a = pictures + QStringLiteral("/A");

    WallpaperModel model;
    model.setSourceDirectory(a);
    QCOMPARE(model.rowCount(), 2);

    QSignalSpy countSpy(&model, &WallpaperModel::countChanged);
    saveImage(a + QStringLiteral("/delta.png"), Qt::green);
    QVERIFY(countSpy.wait(5000));
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(QFileInfo(model.pathAt(2)).fileName(), QStringLiteral("delta.png"));
}

void tst_AppController::testFoldersAndConfig()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString a = pictures + QStringLiteral("/A");
    const QString b = pictures + QStringLiteral("/B");

    AppController app;
    QCOMPARE(app.folders(), QStringList{pictures});

    app.addFolder(a);
    QStringList expected = {pictures, a};
    QCOMPARE(app.folders(), expected);
    QCOMPARE(app.currentFolderIndex(), 1);

    app.addFolder(a); // duplicate is ignored
    QCOMPARE(app.folders().size(), 2);

    app.addFolder(b);
    QCOMPARE(app.currentFolderIndex(), 2);

    app.setCurrentFolderIndex(0);
    QCOMPARE(app.currentFolderPath(), pictures);

    app.removeFolder(1);
    QStringList expected2 = {pictures, b};
    QCOMPARE(app.folders(), expected2);
    QCOMPARE(app.currentFolderIndex(), 0);

    // A fresh controller must see the persisted state from the same config file.
    AppController app2;
    QCOMPARE(app2.folders(), app.folders());
    QCOMPARE(app2.currentFolderIndex(), 0);
    QVERIFY(QFile::exists(app2.configFilePath()));
    QVERIFY(app2.configFilePath().endsWith(QStringLiteral("kwalrc")));
}

void tst_AppController::testSelectionAndApply()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    AppController app;
    QSignalSpy resultSpy(&app, &AppController::wallpaperSetResult);

    // No selection: applying is a no-op.
    app.applyWallpaper();
    QCOMPARE(resultSpy.count(), 0);
    QCOMPARE(app.selectedWallpaper(), QString());

    // Selection pointing at a missing file: cleared and reported.
    app.selectWallpaper(pictures + QStringLiteral("/A/missing.png"));
    QCOMPARE(app.selectedWallpaper(), pictures + QStringLiteral("/A/missing.png"));
    app.applyWallpaper();
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.at(0).at(0).toBool(), false);
    QCOMPARE(app.selectedWallpaper(), QString());
}

void tst_AppController::testWindowSettings()
{
    AppController app;
    QCOMPARE(app.defaultWindowWidth(), 960);
    QCOMPARE(app.defaultWindowHeight(), 650);

    QSignalSpy widthSpy(&app, &AppController::defaultWindowWidthChanged);
    app.setDefaultWindowWidth(1024);
    app.setDefaultWindowHeight(768);
    QCOMPARE(app.defaultWindowWidth(), 1024);
    QCOMPARE(app.defaultWindowHeight(), 768);
    QCOMPARE(widthSpy.count(), 1);

    app.setDefaultWindowWidth(1024); // unchanged value does not re-emit
    QCOMPARE(widthSpy.count(), 1);

    // Out-of-range values are clamped to the configurable window range.
    app.setDefaultWindowWidth(1);
    app.setDefaultWindowHeight(4096);
    QCOMPARE(app.defaultWindowWidth(), 700);
    QCOMPARE(app.defaultWindowHeight(), 2160);

    // Persisted to the same kwalrc file and visible to a fresh controller.
    AppController app2;
    QCOMPARE(app2.defaultWindowWidth(), 700);
    QCOMPARE(app2.defaultWindowHeight(), 2160);
}

void tst_AppController::testTransparencyPersistence()
{
    AppController app;
    QCOMPARE(app.enableTransparency(), true); // default on

    app.setEnableTransparency(false);
    QCOMPARE(app.enableTransparency(), false);

    AppController app2;
    QCOMPARE(app2.enableTransparency(), false);

    app2.setEnableTransparency(true); // restore default
}

void tst_AppController::testToolDetection()
{
    // Detection is a $PATH lookup for installed binaries (not running
    // processes): point PATH at a sandbox dir holding only a fake
    // fastfetch and assert the other tools are not reported.
    const QByteArray previousPath = qgetenv("PATH");
    const QString fakeBin = QDir::tempPath() + QStringLiteral("/kwal-test-tools-") +
                            QString::number(QCoreApplication::applicationPid());
    QVERIFY(QDir().mkpath(fakeBin));
    QFile fake(fakeBin + QStringLiteral("/fastfetch"));
    QVERIFY(fake.open(QIODevice::WriteOnly));
    fake.write("#!/bin/sh");
    fake.close();
    const QFile::Permissions exec = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                    QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther;
    QVERIFY(QFile::setPermissions(fake.fileName(), exec));
    qputenv("PATH", fakeBin.toUtf8());

    AppController app;
    QCOMPARE(app.fastfetchAvailable(), true);
    QCOMPARE(app.starshipAvailable(), false);
    QCOMPARE(app.ulauncherAvailable(), false);

    qputenv("PATH", previousPath);
    QDir().remove(fakeBin);
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations: set before the QCoreApplication is constructed.
    // HOME is sandboxed too so QStandardPaths user dirs (Pictures, ...) never
    // touch the real home directory.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-") +
                         QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_AppController test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_appcontroller.moc"