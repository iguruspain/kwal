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
    void testUnreadableFormats();
    void testCustomCommands();
    void testCustomCommandExpansion();
    void testCustomCommandQuoting();
    void testSourceColorIndexPersistence();

private:
    static void saveImage(const QString &path, Qt::GlobalColor color)
    {
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(color);
        image.save(path);
    }

    // Custom commands persist in the sandboxed kvalrc across the whole run; start from a known-empty list.
    static void clearCommands(AppController &app)
    {
        while (app.customCommands().size() > 0) {
            app.removeCustomCommand(0);
        }
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

    // Filtering resets the model: rowForPath must follow visible entries and report -1 for
    // hidden files, or the grid's cursor goes stale on a filtered row.
    const QString alphaPath = model->pathAt(0);
    const QString betaPath = model->pathAt(1);
    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);

    model->setFilterText(QStringLiteral("lph"));
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->rowForPath(alphaPath), 0);
    QCOMPARE(model->rowForPath(betaPath), -1);

    model->setFilterText(QString());
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->rowForPath(alphaPath), 0);
    QCOMPARE(model->rowForPath(betaPath), 1);
    QCOMPARE(model->rowForPath(QStringLiteral("/does/not/exist.png")), -1);
    QCOMPARE(model->rowForPath(QString()), -1);

    // Newline names are hidden: video apply writes raw file:// URLs into
    // appletsrc/kscreenlockerrc, where the newline injects an INI line.
    const QString junk = pictures + QStringLiteral("/C-newline");
    QDir().mkpath(junk);
    saveImage(junk + QStringLiteral("/ok.png"), Qt::red);
    saveImage(junk + QStringLiteral("/evil\nnotme.png"), Qt::red);
    model->setSourceDirectory(junk);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(QFileInfo(model->pathAt(0)).fileName(), QStringLiteral("ok.png"));
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

    app.addFolder(a);
    QCOMPARE(app.folders().size(), 2);

    app.addFolder(b);
    QCOMPARE(app.currentFolderIndex(), 2);

    app.setCurrentFolderIndex(0);
    QCOMPARE(app.currentFolderPath(), pictures);

    app.removeFolder(1);
    QStringList expected2 = {pictures, b};
    QCOMPARE(app.folders(), expected2);
    QCOMPARE(app.currentFolderIndex(), 0);

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

    app.applyWallpaper();
    QCOMPARE(resultSpy.count(), 0);
    QCOMPARE(app.selectedWallpaper(), QString());

    // Missing selection file: apply fails and clears the selection.
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

    app.setDefaultWindowWidth(1024);
    QCOMPARE(widthSpy.count(), 1);

    // Valid window range: 700-2160.
    app.setDefaultWindowWidth(1);
    app.setDefaultWindowHeight(4096);
    QCOMPARE(app.defaultWindowWidth(), 700);
    QCOMPARE(app.defaultWindowHeight(), 2160);

    AppController app2;
    QCOMPARE(app2.defaultWindowWidth(), 700);
    QCOMPARE(app2.defaultWindowHeight(), 2160);
}

void tst_AppController::testTransparencyPersistence()
{
    AppController app;
    QCOMPARE(app.enableTransparency(), true);

    app.setEnableTransparency(false);
    QCOMPARE(app.enableTransparency(), false);

    AppController app2;
    QCOMPARE(app2.enableTransparency(), false);

    app2.setEnableTransparency(true);
}

void tst_AppController::testTransparencyLevelPersistence()
{
    AppController app;
    QCOMPARE(app.transparencyLevel(), 80);

    QSignalSpy spy(&app, &AppController::transparencyLevelChanged);
    app.setTransparencyLevel(40);
    QCOMPARE(app.transparencyLevel(), 40);
    QCOMPARE(spy.count(), 1);

    app.setTransparencyLevel(40);
    QCOMPARE(spy.count(), 1);

    // Out-of-range values are clamped to the 0-100 percent range.
    app.setTransparencyLevel(120);
    QCOMPARE(app.transparencyLevel(), 100);
    app.setTransparencyLevel(-5);
    QCOMPARE(app.transparencyLevel(), 0);

    app.setTransparencyLevel(55);
    AppController app2;
    QCOMPARE(app2.transparencyLevel(), 55);
    app2.setTransparencyLevel(80);
}

void tst_AppController::testToolDetection()
{
    // Detection is a $PATH lookup for binaries (not running processes): fake fastfetch on a sandboxed PATH.
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

void tst_AppController::testUnreadableFormats()
{
    const QStringList accepted = WallpaperModel::imageExtensions();
    QVERIFY(!accepted.isEmpty());

    QVERIFY(wallpapermodel::unreadableFormats({QByteArrayLiteral("png"), QByteArrayLiteral("jpg"),
                                               QByteArrayLiteral("jpeg"), QByteArrayLiteral("webp"),
                                               QByteArrayLiteral("bmp"), QByteArrayLiteral("gif"),
                                               QByteArrayLiteral("svg")},
                                              accepted)
                .isEmpty());

    // Only formats whose handler lives outside qt6-base are reported, in accepted order.
    QCOMPARE(wallpapermodel::unreadableFormats({QByteArrayLiteral("png"), QByteArrayLiteral("jpeg"),
                                                QByteArrayLiteral("bmp"), QByteArrayLiteral("gif")},
                                               accepted),
             QStringList({QStringLiteral("webp"), QStringLiteral("svg")}));

    // Handler aliases: a plugin advertising "jpeg" decodes .jpg too.
    QVERIFY(wallpapermodel::unreadableFormats({QByteArrayLiteral("jpeg")},
                                              {QStringLiteral("jpg"), QStringLiteral("jpeg")})
                .isEmpty());

    // Case-insensitive, reported lowercased, repeated extensions collapsed.
    QCOMPARE(wallpapermodel::unreadableFormats({QByteArrayLiteral("PNG")},
                                               {QStringLiteral("PNG"), QStringLiteral("WebP"),
                                                QStringLiteral("webp")}),
             QStringList({QStringLiteral("webp")}));

    // Handlers shipping inside qt6-base must never be reported: that warning
    // would tell the user to reinstall Qt itself.
    AppController app;
    const QStringList reported = app.unreadableWallpaperFormats();
    for (const QString &extension : {QStringLiteral("png"), QStringLiteral("jpg"),
                                     QStringLiteral("gif"), QStringLiteral("bmp")}) {
        QVERIFY2(!reported.contains(extension),
                 qPrintable(extension + QStringLiteral(" reported unreadable: ") + reported.join(QLatin1Char(','))));
    }
}

void tst_AppController::testCustomCommands()
{
    AppController app;
    QCOMPARE(app.selectedSourceColorIndex(), 0);
    QCOMPARE(app.customCommands().size(), 0);

    QSignalSpy spy(&app, &AppController::customCommandsChanged);

    app.addCustomCommand();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(app.customCommands().size(), 1);
    app.updateCustomCommand(0, QStringLiteral("notify-send 'x %path%'"));
    app.updateCustomCommand(0, QStringLiteral("notify-send 'x %path%'"));
    QCOMPARE(spy.count(), 2);

    app.addCustomCommand();
    app.updateCustomCommand(1, QStringLiteral("echo %scolor% %sc%"));
    app.setCustomCommandEnabled(0, false);
    app.setCustomCommandEnabled(0, false);
    QCOMPARE(spy.count(), 5);

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
    clearCommands(app);
    QTemporaryDir dir;
    const QString wallpaper = dir.path() + QStringLiteral("/wall.png");
    const QString out = dir.path() + QStringLiteral("/expanded.txt");
    const QString second = dir.path() + QStringLiteral("/second.txt");

    app.selectWallpaper(wallpaper);
    app.setSelectedSourceColorIndex(2);

    QCOMPARE(app.themeMode(), QStringLiteral("dark"));
    QCOMPARE(app.customColor(), QString());
    app.setThemeMode(QStringLiteral("light"));
    app.setCustomColor(QStringLiteral("#123456"));
    // Shell-bound placeholders are quoted; %sc% (always an int) is the only unquoted one.
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%mode%")), QStringLiteral("'light'"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%ccolor%")), QStringLiteral("'#123456'"));
    AppController modeApp;
    QCOMPARE(modeApp.themeMode(), QStringLiteral("light"));
    QCOMPARE(modeApp.customColor(), QStringLiteral("#123456"));

    QCOMPARE(app.expandPlaceholders(QStringLiteral("%path%")),
             QStringLiteral("'%1'").arg(wallpaper));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%image%")),
             QStringLiteral("'%1'").arg(wallpaper));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%sc%")), QStringLiteral("2"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%scolor%")), QStringLiteral("''"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("a %path% %sc% b")),
             QStringLiteral("a '%1' 2 b").arg(wallpaper));
    // Embedded single quotes are closed and re-escaped ('\'' ), never left open.
    const QString tricky = dir.path() + QStringLiteral("/it's a $(touch bad) `bad` \"bad\".png");
    app.selectWallpaper(tricky);
    QString escapedTricky = tricky;
    escapedTricky.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    QCOMPARE(app.expandPlaceholders(QStringLiteral("%path%")),
             QStringLiteral("'%1'").arg(escapedTricky));

    // Enabled commands run as one detached shell chain; disabled/empty are skipped.
    app.selectWallpaper(wallpaper);
    app.addCustomCommand();
    app.updateCustomCommand(0, QStringLiteral("echo %path% > %1").arg(out));
    app.addCustomCommand();
    app.updateCustomCommand(1, QStringLiteral("echo x > %1").arg(second));
    app.setCustomCommandEnabled(1, false);
    app.addCustomCommand();
    app.runCustomCommands();

    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(out), 5000);
    QFile file(out);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readLine()).trimmed(),
             wallpaper);
    QVERIFY(!QFile::exists(second));
}

void tst_AppController::testCustomCommandQuoting()
{
    // Regression: a wallpaper named as a shell payload. Double-quoted expansion would run
    // $( ) and backtick substitutions through sh -c; single-quoted expansion keeps them literal.
    QTemporaryDir dir;
    const QString subMarker = dir.path() + QStringLiteral("/pwn_sub");
    const QString btMarker = dir.path() + QStringLiteral("/pwn_bt");
    const QString ccMarker = dir.path() + QStringLiteral("/pwn_cc");
    const QString name = QStringLiteral("a $(touch '%1') b `touch '%2'` it's.png").arg(subMarker, btMarker);
    const QString wallpaper = dir.path() + QLatin1Char('/') + name;
    const QString out = dir.path() + QStringLiteral("/out.txt");
    const QString ccOut = dir.path() + QStringLiteral("/cc.txt");

    AppController app;
    clearCommands(app);
    app.selectWallpaper(wallpaper);
    app.setCustomColor(QStringLiteral("#fff; touch '%1'").arg(ccMarker));

    app.addCustomCommand();
    app.updateCustomCommand(0, QStringLiteral("echo %path% > '%1'").arg(out));
    app.addCustomCommand();
    app.updateCustomCommand(1, QStringLiteral("echo %ccolor% > '%1'").arg(ccOut));
    app.runCustomCommands();

    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(out), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(ccOut), 5000);
    QFile file(out);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()).trimmed(), wallpaper);
    QFile ccFile(ccOut);
    QVERIFY(ccFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(ccFile.readAll()).trimmed(),
             QStringLiteral("#fff; touch '%1'").arg(ccMarker));
    QVERIFY2(!QFile::exists(subMarker), "substitution in path executed");
    QVERIFY2(!QFile::exists(btMarker), "backtick in path executed");
    QVERIFY2(!QFile::exists(ccMarker), "command break-out via ccolor executed");
}

void tst_AppController::testSourceColorIndexPersistence()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString a = pictures + QStringLiteral("/A");
    const QString alpha = a + QStringLiteral("/alpha.png");
    const QString beta = a + QStringLiteral("/beta.png");

    AppController app;
    QCOMPARE(app.currentWallpaper().isEmpty(), true);

    // Dot selection is per wallpaper: it follows navigation and survives going back.
    app.selectWallpaper(alpha);
    app.setSelectedSourceColorIndex(3);
    app.selectWallpaper(beta);
    QCOMPARE(app.selectedSourceColorIndex(), 0);
    app.selectWallpaper(alpha);
    QCOMPARE(app.selectedSourceColorIndex(), 3);

    // Nothing is persisted while no wallpaper has been applied.
    AppController app2;
    app2.selectWallpaper(alpha);
    QCOMPARE(app2.selectedSourceColorIndex(), 0);

    // Applying persists the active dot selection as the single stored entry.
    app.setCurrentWallpaper(alpha);
    AppController app3;
    app3.selectWallpaper(alpha);
    QCOMPARE(app3.selectedSourceColorIndex(), 3);
    app3.selectWallpaper(beta);
    QCOMPARE(app3.selectedSourceColorIndex(), 0);

    // Dot choices on non-applied wallpapers are session-local; applying stores the
    // selection active at apply time (config stays single-entry).
    app3.setSelectedSourceColorIndex(5); // on beta, not applied yet
    app3.setCurrentWallpaper(beta);
    AppController app4;
    app4.selectWallpaper(beta);
    QCOMPARE(app4.selectedSourceColorIndex(), 5);
    app4.selectWallpaper(alpha);
    QCOMPARE(app4.selectedSourceColorIndex(), 0);
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations and HOME, set before QCoreApplication construction:
    // QStandardPaths user dirs (Pictures, ...) never touch the real home.
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