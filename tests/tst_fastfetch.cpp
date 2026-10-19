#include "fastfetchcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
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

}

class tst_Fastfetch : public QObject
{
    Q_OBJECT

private slots:
    void testConstructorAdoptsConfigImage();
    void testNoConfigStaysEmpty();
    void testManualSelectionSurvivesRefresh();
    void testAutoAdoptionTracksConfigChange();
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
