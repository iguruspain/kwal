#include "tools/fastfetchconfig.h"
#include "tools/fileutil.h"
#include "tools/tintimage.h"
#include "templateinstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QStandardPaths>
#include <QtTest>

class tst_Tools : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAtomicWrite();
    void testBackupRestore();
    void testTintImageTo();
    void testTintAnimatedGif();
    void testTintAnimatedWebp();
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

    // A second backup refreshes the backup to the current state.
    writeFile(path, QStringLiteral("modified"));
    QCOMPARE(tools::backupFile(path), path + QStringLiteral(".bak"));
    QCOMPARE(readFile(path + QStringLiteral(".bak")), QStringLiteral("modified"));

    QVERIFY(tools::restoreBackup(path));
    QCOMPARE(readFile(path), QStringLiteral("modified"));
    QCOMPARE(tools::restoreBackup(path + QStringLiteral(".missing")), false);

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void tst_Tools::testTintImageTo()
{
    // The pipeline shells out to ImageMagick; skip when it is not installed.
    const bool hasMagick = !QStandardPaths::findExecutable(QStringLiteral("magick")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("convert")).isEmpty();
    if (!hasMagick)
        QSKIP("ImageMagick is not installed");

    const QString src = QDir::tempPath() + QStringLiteral("/kwal-test-tint-src.png");
    const QString dst = QDir::tempPath() + QStringLiteral("/kwal-test-tint-dst.png");
    QImage source(16, 16, QImage::Format_RGB32);
    source.fill(QColor(128, 128, 128));
    QVERIFY(source.save(src));

    // Mid-gray tinted red at the default strength: red dominates the output.
    QVERIFY(tools::tintImageTo(src, dst, QColor(255, 0, 0)));
    QVERIFY(QFile::exists(dst));
    const QImage out(dst);
    QVERIFY(!out.isNull());
    const QRgb px = out.pixel(8, 8);
    QVERIFY(qRed(px) > qGreen(px));
    QVERIFY(qRed(px) > qBlue(px));

    // A preview pass with another color: green fill dominates (the pass
    // keeps the source size; there is no scaled variant anymore).
    const QString small = QDir::tempPath() + QStringLiteral("/kwal-test-tint-small.png");
    QVERIFY(tools::tintImageTo(src, small, QColor(0, 200, 10)));
    const QImage preview(small);
    QVERIFY(!preview.isNull());
    const QRgb spx = preview.pixel(0, 0);
    QVERIFY(qGreen(spx) > qRed(spx));
    QVERIFY(qGreen(spx) > qBlue(spx));

    // Missing source is a clean failure with no output written.
    QFile::remove(dst);
    QVERIFY(!tools::tintImageTo(QDir::tempPath() + QStringLiteral("/kwal-test-tint-missing.png"),
        dst, Qt::red));
    QVERIFY(!QFile::exists(dst));

    QFile::remove(src);
    QFile::remove(small);
}

void tst_Tools::testTintAnimatedGif()
{
    // The pipeline shells out to ImageMagick; skip when it is not installed.
    const bool hasMagick = !QStandardPaths::findExecutable(QStringLiteral("magick")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("convert")).isEmpty();
    if (!hasMagick)
        QSKIP("ImageMagick is not installed");
    const QString magickPath = QStandardPaths::findExecutable(QStringLiteral("magick"));
    const QString im = magickPath.isEmpty()
        ? QStandardPaths::findExecutable(QStringLiteral("convert"))
        : magickPath;

    const QString src = QDir::tempPath() + QStringLiteral("/kwal-test-tint-gif-src.gif");
    const QString dst = QDir::tempPath() + QStringLiteral("/kwal-test-tint-gif-dst.gif");
    const QString small = QDir::tempPath() + QStringLiteral("/kwal-test-tint-gif-small.gif");
    QFile::remove(src);
    QFile::remove(dst);
    QFile::remove(small);

    // Six solid frames (red, green, blue, yellow, cyan, magenta): the
    // multi-frame case the pipeline used to fail on. No shell involved, so
    // the parentheses are literal arguments.
    QProcess gen;
    gen.start(im, {QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:red"), QStringLiteral(")"),
                   QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:green"), QStringLiteral(")"),
                   QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:blue"), QStringLiteral(")"),
                   QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:yellow"), QStringLiteral(")"),
                   QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:cyan"), QStringLiteral(")"),
                   QStringLiteral("("), QStringLiteral("-size"), QStringLiteral("16x16"),
                   QStringLiteral("xc:magenta"), QStringLiteral(")"), src});
    QVERIFY(gen.waitForFinished(10000));
    {
        QImageReader check(src);
        if (!check.canRead() || check.imageCount() != 6)
            QSKIP("could not generate the test GIF fixture");
    }

    // The tint pass keeps the whole animated GIF, delays included.
    QVERIFY(tools::tintImageTo(src, dst, QColor(0, 0, 255)));
    QVERIFY(QFile::exists(dst));
    QImageReader out(dst);
    QVERIFY(out.canRead());
    QCOMPARE(out.imageCount(), 6);

    // Frame 0 was solid red: after gray + 80% blue tint, blue dominates.
    // Repeated read() calls return successive frames.
    const QImage frame0 = out.read();
    QVERIFY(!frame0.isNull());
    const QRgb p0 = frame0.pixel(8, 8);
    QVERIFY(qBlue(p0) > qRed(p0));
    QVERIFY(qBlue(p0) > qGreen(p0));

    // The second frame is still present and readable.
    QVERIFY(!out.read().isNull());

    // The preview pass on a GIF keeps the whole sequence in one animated
    // GIF, never a numbered file per frame.
    QVERIFY(tools::tintImageTo(src, small, QColor(0, 200, 10)));
    QImageReader smallOut(small);
    QVERIFY(smallOut.canRead());
    QCOMPARE(smallOut.imageCount(), 6);
    const QImage smallFrame = smallOut.read();
    QVERIFY(!smallFrame.isNull());
    QVERIFY(!QFile::exists(small + QStringLiteral("-0.gif")));

    QFile::remove(src);
    QFile::remove(dst);
    QFile::remove(small);
}

void tst_Tools::testTintAnimatedWebp()
{
    // The pipeline shells out to ImageMagick; skip when it is not installed.
    const bool hasMagick = !QStandardPaths::findExecutable(QStringLiteral("magick")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("convert")).isEmpty();
    if (!hasMagick)
        QSKIP("ImageMagick is not installed");
    const QString magickPath = QStandardPaths::findExecutable(QStringLiteral("magick"));
    const QString im = magickPath.isEmpty()
        ? QStandardPaths::findExecutable(QStringLiteral("convert"))
        : magickPath;

    const QString src = QDir::tempPath() + QStringLiteral("/kwal-test-tint-webp-src.webp");
    const QString dst = QDir::tempPath() + QStringLiteral("/kwal-test-tint-webp-dst.webp");
    const QString small = QDir::tempPath() + QStringLiteral("/kwal-test-tint-webp-small.webp");
    QFile::remove(src);
    QFile::remove(dst);
    QFile::remove(small);

    // Six frames (red, green, blue, yellow, cyan, magenta), same
    // parenthesized-operand form as the GIF fixture. WebP needs explicit
    // frame delays for a real sequence.
    QProcess gen;
    const QStringList colors = {QStringLiteral("red"), QStringLiteral("green"),
                                QStringLiteral("blue"), QStringLiteral("yellow"),
                                QStringLiteral("cyan"), QStringLiteral("magenta")};
    QStringList genArgs;
    for (const QString &color : colors) {
        genArgs << QStringLiteral("(") << QStringLiteral("-size") << QStringLiteral("16x16")
                << QStringLiteral("xc:") + color << QStringLiteral("-delay")
                << QStringLiteral("10") << QStringLiteral(")");
    }
    genArgs << QStringLiteral("-coalesce") << src;
    gen.start(im, genArgs);
    QVERIFY(gen.waitForFinished(10000));
    {
        QImageReader check(src);
        if (!check.canRead() || check.imageCount() != 6)
            QSKIP("could not generate the test WebP fixture");
    }

    // The tint pass keeps the whole animated WebP, delays included.
    QVERIFY(tools::tintImageTo(src, dst, QColor(0, 0, 255)));
    QImageReader out(dst);
    QVERIFY(out.canRead());
    QCOMPARE(out.imageCount(), 6);
    const QImage frame0 = out.read();
    QVERIFY(!frame0.isNull());
    const QRgb p0 = frame0.pixel(8, 8);
    QVERIFY(qBlue(p0) > qRed(p0));
    QVERIFY(qBlue(p0) > qGreen(p0));
    QVERIFY(!out.read().isNull());

    // The preview pass keeps the whole sequence in one animated WebP, not
    // the per-frame PNG split ImageMagick produces when a sequence is
    // written to a still format.
    QVERIFY(tools::tintImageTo(src, small, QColor(0, 200, 10)));
    QImageReader smallOut(small);
    QVERIFY(smallOut.canRead());
    QCOMPARE(smallOut.imageCount(), 6);
    const QImage smallFrame = smallOut.read();
    QVERIFY(!smallFrame.isNull());
    QVERIFY(!QFile::exists(small + QStringLiteral("-0.webp")));

    QFile::remove(src);
    QFile::remove(dst);
    QFile::remove(small);
}

void tst_Tools::testTintedImagePath()
{
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/wall.png")),
        QStringLiteral("/home/u/pics/tinted/wall-tinted.png"));
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/My Wall.jpg")),
        QStringLiteral("/home/u/pics/tinted/My Wall-tinted.jpg"));
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/wall.webp")),
        QStringLiteral("/home/u/pics/tinted/wall-tinted.webp"));
    QCOMPARE(tools::tintedImagePath(QStringLiteral("/home/u/pics/wall.gif")),
        QStringLiteral("/home/u/pics/tinted/wall-tinted.gif"));
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
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
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
