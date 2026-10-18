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
    void testFoldersAndConfig();

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

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations: set before the QCoreApplication is constructed.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-") +
                         QString::number(QCoreApplication::applicationPid());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    qputenv("XDG_USER_PICTURES", (base + QStringLiteral("/Pictures")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_AppController test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_appcontroller.moc"