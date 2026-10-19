#include "tools/fastfetchconfig.h"
#include "tools/fileutil.h"
#include "tools/tintimage.h"
#include "templateinstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QtTest>

class tst_Tools : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAtomicWrite();
    void testBackupRestore();
    void testTintImage();
    void testTintedImagePath();
    void testFastfetchSetSource();
    void testFastfetchDetectImage();
    void testFastfetchBackup();
    void testFastfetchClearCache();
    void testTemplateInstaller();

    static void writeFile(const QString &path, const QString &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content.toUtf8());
    }

    static QString readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QString::fromUtf8(file.readAll());
    }
};

void tst_Tools::initTestCase()
{
    // Nothing needed: the main() below sandboxes HOME/XDG_* before the app
    // object exists, so all user locations are temp.
}

void tst_Tools::testAtomicWrite()
{
    const QString path = QDir::tempPath() + QStringLiteral("/kwal-test-atomic.txt");
    QFile::remove(path);

    QVERIFY(tools::writeFileAtomic(path, QStringLiteral("hello")));
    QCOMPARE(readFile(path), QStringLiteral("hello"));

    // Overwrite keeps exactly the new content.
    QVERIFY(tools::writeFileAtomic(path, QStringLiteral("world")));
    QCOMPARE(readFile(path), QStringLiteral("world"));

    // No temp files left behind in the directory.
    const QStringList leftovers = QDir(QDir::tempPath()).entryList(
        QStringList{QStringLiteral("kwal-test-atomic.txt.*")}, QDir::Files);
    QCOMPARE(leftovers.isEmpty(), true);

    QFile::remove(path);
}

void tst_Tools::testBackupRestore()
{
    const QString path = QDir::tempPath() + QStringLiteral("/kwal-test-backup.txt");
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));

    writeFile(path, QStringLiteral("original"));

    QCOMPARE(tools::hasBackup(path), false);
    QCOMPARE(tools::backupFile(path), path + QStringLiteral(".bak"));
    QCOMPARE(tools::hasBackup(path), true);

    // A second backup does not clobber the first one.
    writeFile(path, QStringLiteral("modified"));
    QCOMPARE(tools::backupFile(path), QString());
    QCOMPARE(readFile(path + QStringLiteral(".bak")), QStringLiteral("original"));

    QVERIFY(tools::restoreBackup(path));
    QCOMPARE(readFile(path), QStringLiteral("original"));
    QCOMPARE(tools::restoreBackup(path + QStringLiteral(".missing")), false);

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void tst_Tools::testTintImage()
{
    QImage source(10, 10, QImage::Format_RGB32);
    source.fill(Qt::white);

    // Full strength replaces the image with the tint color.
    const QImage full = tools::tintImage(source, QColor(200, 10, 30), 1.0);
    QVERIFY(!full.isNull());
    QCOMPARE(full.pixel(0, 0), qRgb(200, 10, 30));
    QCOMPARE(full.pixel(9, 9), qRgb(200, 10, 30));

    // Zero strength keeps the luminance in grayscale.
    const QImage none = tools::tintImage(source, QColor(200, 10, 30), 0.0);
    const QRgb whiteGray = none.pixel(0, 0);
    QCOMPARE(qRed(whiteGray), qGreen(whiteGray));
    QCOMPARE(qGreen(whiteGray), qBlue(whiteGray));
    QCOMPARE(qRed(whiteGray), 255);

    // Mid strength blends toward the tint: red channel rises, others fall.
    QImage midSource(4, 4, QImage::Format_RGB32);
    midSource.fill(QColor(30, 60, 90));
    const QImage mid = tools::tintImage(midSource, QColor(255, 0, 0), 0.5);
    const QRgb blended = mid.pixel(0, 0);
    QVERIFY(qRed(blended) > 30);
    QVERIFY(qGreen(blended) < 60);

    // Null input stays null; out-of-range strength is clamped, not an error.
    QVERIFY(tools::tintImage(QImage(), QColor(1, 2, 3)).isNull());
    QVERIFY(!tools::tintImage(source, QColor(1, 2, 3), 7).isNull());
}

void tst_Tools::testTintedImagePath()
{
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/wall.png")),
        QStringLiteral("/home/u/pics/wall-tinted.png"));
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/My Wall.jpg")),
        QStringLiteral("/home/u/pics/My Wall-tinted.jpg"));
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/wall.webp")),
        QStringLiteral("/home/u/pics/wall-tinted.webp"));
}

void tst_Tools::testFastfetchSetSource()
{
    const QString cfg = tools::fastfetchConfigPath();
    QDir(QFileInfo(cfg).absolutePath()).mkpath(QStringLiteral("."));

    // Quoted source key.
    writeFile(cfg, QStringLiteral(
        "{\n"
        "  // my fastfetch config\n"
        "  \"source\": \"~/old-image.png\",\n"
        "  \"scroll\": { \"pad\": 4 }\n"
        "}\n"));
    QVERIFY(tools::setImageSource(QStringLiteral("/tmp/new-image.png")));
    const QString after = readFile(cfg);
    QVERIFY(after.contains(QStringLiteral("\"source\": \"/tmp/new-image.png\"")));
    QVERIFY(after.contains(QStringLiteral("// my fastfetch config")));
    QVERIFY(after.contains(QStringLiteral("\"pad\": 4")));

    // Home-relative storage.
    const QString homeImage = QDir::homePath() + QStringLiteral("/pics/wall.png");
    QVERIFY(tools::setImageSource(homeImage));
    QVERIFY(readFile(cfg).contains(QStringLiteral("\"source\": \"~/pics/wall.png\"")));

    // Unquoted JSONC source key.
    writeFile(cfg, QStringLiteral(
        "{\n"
        "  source: 'x.png',\n"
        "  logo: 'none'\n"
        "}\n"));
    QVERIFY(tools::setImageSource(QStringLiteral("/abs/img.png")));
    const QString afterUnquoted = readFile(cfg);
    QVERIFY(afterUnquoted.contains(QStringLiteral("source: \"/abs/img.png\"")));
    QVERIFY(afterUnquoted.contains(QStringLiteral("logo: 'none'")));

    // Missing source key is a clean failure.
    writeFile(cfg, QStringLiteral("{\n  logo: 'none'\n}\n"));
    QCOMPARE(tools::setImageSource(QStringLiteral("/abs/img.png")), false);

    QFile::remove(cfg);
    QFile::remove(cfg + QStringLiteral(".bak"));
}

void tst_Tools::testFastfetchDetectImage()
{
    const QString cfg = tools::fastfetchConfigPath();
    const QString existing = QDir::tempPath() + QStringLiteral("/kwal-test-image.png");
    writeFile(existing, QByteArray(8, 0)); // Content irrelevant; existence matters.

    writeFile(cfg, QStringLiteral("{ \"source\": \"%1\" }\n").arg(existing));
    QCOMPARE(tools::detectConfigImage(), QFileInfo(existing).absoluteFilePath());

    // A key pointing at a missing file is skipped; the fallback is used only
    // when no key matches an existing file.
    writeFile(cfg, QStringLiteral("{ \"config_image\": \"/nonexistent/x.png\" }\n"));
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/fastfetch/chica-tinted.png");
    QDir(QFileInfo(fallback).absolutePath()).mkpath(QStringLiteral("."));
    writeFile(fallback, QByteArray(8, 0));
    QCOMPARE(tools::detectConfigImage(), QFileInfo(fallback).absoluteFilePath());

    QFile::remove(cfg);
    QFile::remove(existing);
    QFile::remove(fallback);
}

void tst_Tools::testFastfetchBackup()
{
    const QString cfg = tools::fastfetchConfigPath();
    QDir(QFileInfo(cfg).absolutePath()).mkpath(QStringLiteral("."));
    QFile::remove(cfg);
    QFile::remove(cfg + QStringLiteral(".bak"));

    writeFile(cfg, QStringLiteral("{ \"source\": \"~/one.png\" }\n"));
    QCOMPARE(tools::hasConfigBackup(), false);

    tools::backupFile(cfg);
    QCOMPARE(tools::hasConfigBackup(), true);

    writeFile(cfg, QStringLiteral("{ \"source\": \"~/two.png\" }\n"));
    QVERIFY(tools::restoreConfigBackup());
    QCOMPARE(readFile(cfg), QStringLiteral("{ \"source\": \"~/one.png\" }\n"));
    // Restore is a copy: the backup survives so a second restore works.
    QCOMPARE(tools::restoreConfigBackup(), true);

    QFile::remove(cfg);
    QFile::remove(cfg + QStringLiteral(".bak"));
}

void tst_Tools::testFastfetchClearCache()
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/fastfetch");
    QVERIFY(QDir().mkpath(cacheDir));
    writeFile(cacheDir + QStringLiteral("/fetch_cache.json"), QByteArray("{}"));

    tools::clearCache();
    QCOMPARE(QDir(cacheDir).exists(), false);
}

void tst_Tools::testTemplateInstaller()
{
    const QString dir = tools::templateDir();
    const QString starshipToml = dir + QStringLiteral("/starship/catpuccin.toml");
    const QString ulauncherManifest = dir + QStringLiteral("/ulauncher/theme_template/manifest.json");

    tools::installTemplates();
    QCOMPARE(QFileInfo(starshipToml).isFile(), true);
    QCOMPARE(QFileInfo(ulauncherManifest).isFile(), true);

    // Existing files are never overwritten.
    writeFile(ulauncherManifest, QByteArray("{\"name\": \"user-edited\"}"));
    tools::installTemplates();
    QCOMPARE(readFile(ulauncherManifest), QString::fromUtf8("{\"name\": \"user-edited\"}"));
}

int main(int argc, char *argv[])
{
    // Sandboxed XDG locations, set before the app object is constructed.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-test-tools-")
        + QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_Tools test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_tools.moc"
