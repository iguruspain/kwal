#include "appcontroller.h"
#include "wallpapermodel.h"

#include <KConfig>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariantMap>

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
    void testTransparencyLevelPersistence();
    void testToolDetection();
    void testCustomCommands();
    void testCustomCommandExpansion();
    void testSourceColorIndexPersistence();

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

void tst_AppController::testTransparencyLevelPersistence()
{
    AppController app;
    QCOMPARE(app.transparencyLevel(), 80); // default 80%

    QSignalSpy spy(&app, &AppController::transparencyLevelChanged);
    app.setTransparencyLevel(40);
    QCOMPARE(app.transparencyLevel(), 40);
    QCOMPARE(spy.count(), 1);

    app.setTransparencyLevel(40); // unchanged value does not re-emit
    QCOMPARE(spy.count(), 1);

    // Out-of-range values are clamped to the 0-100 percent range.
    app.setTransparencyLevel(120);
    QCOMPARE(app.transparencyLevel(), 100);
    app.setTransparencyLevel(-5);
    QCOMPARE(app.transparencyLevel(), 0);

    // Persisted to the same kwalrc file and visible to a fresh controller.
    app.setTransparencyLevel(55);
    AppController app2;
    QCOMPARE(app2.transparencyLevel(), 55);
    app2.setTransparencyLevel(80); // restore default
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

void tst_AppController::testCustomCommands()
{
    AppController app;
    QCOMPARE(app.selectedSourceColorIndex(), 0); // first dot selected by default
    QCOMPARE(app.customCommands().size(), 0);

    QSignalSpy spy(&app, &AppController::customCommandsChanged);

    app.addCustomCommand();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(app.customCommands().size(), 1);
    app.updateCustomCommand(0, QStringLiteral("notify-send 'x %path%'"));
    app.updateCustomCommand(0, QStringLiteral("notify-send 'x %path%'")); // no-op
    QCOMPARE(spy.count(), 2);

    app.addCustomCommand();
    app.updateCustomCommand(1, QStringLiteral("echo %scolor% %sc%"));
    app.setCustomCommandEnabled(0, false);
    app.setCustomCommandEnabled(0, false); // no-op
    QCOMPARE(spy.count(), 5);

    // Round-trip through a fresh controller over the same [Commands] group.
    AppController app2;
    QCOMPARE(app2.customCommands().size(), 2);
    const QVariantMap first = app2.customCommands().at(0).toMap();
    QCOMPARE(first.value("command").toString(), QStringLiteral("notify-send 'x %path%'"));
    QCOMPARE(first.value("enabled").toBool(), false);
    const QVariantMap second = app2.customCommands().at(1).toMap();
    QCOMPARE(second.value("command").toString(), QStringLiteral("echo %scolor% %sc%"));
    QCOMPARE(second.value("enabled").toBool(), true);

    // Out-of-range indices and self-moves are no-ops.
    app2.updateCustomCommand(9, QStringLiteral("nope"));
    app2.setCustomCommandEnabled(-1, false);
    app2.removeCustomCommand(9);
    app2.moveCustomCommand(-1, 1);
    app2.moveCustomCommand(0, 2);
    app2.moveCustomCommand(0, 0);
    QCOMPARE(app2.customCommands().size(), 2);

    // A real move reorders and persists.
    app2.moveCustomCommand(0, 1);
    QCOMPARE(app2.customCommands().at(0).toMap().value("command").toString(),
             QStringLiteral("echo %scolor% %sc%"));
    AppController app3;
    QCOMPARE(app3.customCommands().size(), 2);
    QCOMPARE(app3.customCommands().at(0).toMap().value("command").toString(),
             QStringLiteral("echo %scolor% %sc%"));

    app2.removeCustomCommand(1);
    QCOMPARE(app2.customCommands().size(), 1);
    app2.removeCustomCommand(0);
    QCOMPARE(app2.customCommands().size(), 0);
    AppController app4;
    QCOMPARE(app4.customCommands().size(), 0);
}

void tst_AppController::testCustomCommandExpansion()
{
    AppController app;
    QTemporaryDir dir;
    const QString wallpaper = dir.path() + QStringLiteral("/wall.png");
    const QString out = dir.path() + QStringLiteral("/expanded.txt");
    const QString second = dir.path() + QStringLiteral("/second.txt");

    app.selectWallpaper(wallpaper);
    app.setSelectedSourceColorIndex(2);

    // %mode% defaults to "dark"; %ccolor% is empty until picked; both
    // persist to [Commands].
    QCOMPARE(app.themeMode(), QStringLiteral("dark"));
    QCOMPARE(app.customColor(), QString());
    app.setThemeMode(QStringLiteral("light"));
    app.setCustomColor(QStringLiteral("#123456"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%mode%")), QStringLiteral("light"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%ccolor%")), QStringLiteral("#123456"));
    AppController modeApp;
    QCOMPARE(modeApp.themeMode(), QStringLiteral("light"));
    QCOMPARE(modeApp.customColor(), QStringLiteral("#123456"));

    // No source colors for the selection: %path% and %image% expand quoted,
    // %sc% to the index, %scolor% to an empty string.
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%path%")),
             QStringLiteral("\"%1\"").arg(wallpaper));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%image%")),
             QStringLiteral("\"%1\"").arg(wallpaper));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%sc%")), QStringLiteral("2"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%scolor%")), QString());
    QCOMPARE(app.expandPlaceholders(QStringLiteral("a %path% %sc% b")),
             QStringLiteral("a \"%1\" 2 b").arg(wallpaper));

    // The enabled commands run as one detached shell chain; disabled and
    // empty commands are skipped.
    app.addCustomCommand();
    app.updateCustomCommand(0, QStringLiteral("echo %path% > %1").arg(out));
    app.addCustomCommand();
    app.updateCustomCommand(1, QStringLiteral("echo x > %1").arg(second));
    app.setCustomCommandEnabled(1, false);
    app.addCustomCommand(); // empty command, skipped
    app.runCustomCommands();

    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(out), 5000);
    QFile file(out);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readLine()).trimmed(),
             wallpaper);
    QVERIFY(!QFile::exists(second));
}

void tst_AppController::testSourceColorIndexPersistence()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString a = pictures + QStringLiteral("/A");
    const QString alpha = a + QStringLiteral("/alpha.png");
    const QString beta = a + QStringLiteral("/beta.png");

    AppController app;
    app.selectWallpaper(alpha);
    app.setSelectedSourceColorIndex(3);

    // A different wallpaper starts at the first dot, but reselecting a
    // wallpaper restores its own stored dot.
    app.selectWallpaper(beta);
    QCOMPARE(app.selectedSourceColorIndex(), 0);
    app.selectWallpaper(alpha);
    QCOMPARE(app.selectedSourceColorIndex(), 3);

    // Persists to the same [Wallpapers] group as the grid position and is
    // visible to a fresh controller.
    AppController app2;
    app2.selectWallpaper(alpha);
    QCOMPARE(app2.selectedSourceColorIndex(), 3);
    app2.selectWallpaper(beta);
    QCOMPARE(app2.selectedSourceColorIndex(), 0);
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