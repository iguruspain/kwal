#include "plasmaapply.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

class tst_PlasmaApply : public QObject
{
    Q_OBJECT

private slots:
    void testFileUrl();
    void testComputeVideoUrls();
    void testIniEditor();
    void testIniPreservation();
    void testDesktopIds();
    void testPruneVideoUrls();

private:
    static void writeFile(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
        file.close();
    }
    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const QByteArray data = file.readAll();
        file.close();
        return data;
    }
};

void tst_PlasmaApply::testFileUrl()
{
    // Unreserved chars pass through, the rest is percent-encoded in uppercase hex: Plasma's config-URL form.
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/wall.png")),
             QStringLiteral("file:///home/user/wall.png"));
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/my wall.png")),
             QStringLiteral("file:///home/user/my%20wall.png"));
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/á.mp4")),
             QStringLiteral("file:///home/user/%C3%A1.mp4"));
    QCOMPARE(plasmaapply::rawVideoUrl(QStringLiteral("/home/user/clip.mp4")),
             QStringLiteral("file:///home/user/clip.mp4"));
    // Accents/spaces stay raw: the exact form the video plugin stores (QUrl::toString, PreferredEncoding).
    QCOMPARE(plasmaapply::rawVideoUrl(QStringLiteral("/home/user/Vídeos/Ayanami Rei.mp4")),
             QStringLiteral("file:///home/user/Vídeos/Ayanami Rei.mp4"));
}

void tst_PlasmaApply::testComputeVideoUrls()
{
    const QString target = QStringLiteral("file:///v/b.mp4");

    // No existing playlist: target appended with the plugin UI's default fields.
    QJsonArray entries = QJsonDocument::fromJson(plasmaapply::computeVideoUrls(QString(), target).toUtf8()).array();
    QCOMPARE(entries.size(), 1);
    QJsonObject first = entries.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("filename")).toString(), target);
    QCOMPARE(first.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(first.value(QStringLiteral("loop")).toBool(), false);
    QCOMPARE(first.value(QStringLiteral("dayNightPhase")).toInt(), 4);

    // Existing playlist: target enabled, others disabled; entry order and canonical fields preserved.
    const QString existing = QStringLiteral("[{\"filename\":\"file:///v/a.mp4\",\"enabled\":true,\"loop\":true}]");
    entries = QJsonDocument::fromJson(plasmaapply::computeVideoUrls(existing, target).toUtf8()).array();
    QCOMPARE(entries.size(), 2);
    QJsonObject a = entries.at(0).toObject();
    QCOMPARE(a.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(a.value(QStringLiteral("loop")).toBool(), true);
    QJsonObject b = entries.at(1).toObject();
    QCOMPARE(b.value(QStringLiteral("filename")).toString(), target);
    QCOMPARE(b.value(QStringLiteral("enabled")).toBool(), true);

    // A target that already exists is toggled in place, not duplicated.
    const QString existingTarget = QStringLiteral("[{\"filename\":%1,\"enabled\":false}]").arg(target);
    entries = QJsonDocument::fromJson(plasmaapply::computeVideoUrls(existingTarget, target).toUtf8()).array();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).toObject().value(QStringLiteral("enabled")).toBool(), true);

    // The stored string must round-trip byte-identically or Plasma's page reports unsaved changes.
    QCOMPARE(plasmaapply::computeVideoUrls(QString(), target),
             QStringLiteral("[{\"filename\":\"file:///v/b.mp4\",\"enabled\":true,\"duration\":0,"
                            "\"customDuration\":0,\"playbackRate\":0,\"alternativePlaybackRate\":0,"
                            "\"loop\":false,\"dayNightPhase\":4}]"));

    // Plugin-written (canonical field order) input stays canonical: only enabled flags change.
    const QString pluginState = QStringLiteral(
        "[{\"filename\":\"file:///v/a.mp4\",\"enabled\":true,\"duration\":0,\"customDuration\":0,"
        "\"playbackRate\":1.5,\"alternativePlaybackRate\":0,\"loop\":true,\"dayNightPhase\":4}]");
    QCOMPARE(plasmaapply::computeVideoUrls(pluginState, target),
             QStringLiteral("[{\"filename\":\"file:///v/a.mp4\",\"enabled\":false,\"duration\":0,"
                            "\"customDuration\":0,\"playbackRate\":1.5,\"alternativePlaybackRate\":0,"
                            "\"loop\":true,\"dayNightPhase\":4},"
                            "{\"filename\":\"file:///v/b.mp4\",\"enabled\":true,\"duration\":0,"
                            "\"customDuration\":0,\"playbackRate\":0,\"alternativePlaybackRate\":0,"
                            "\"loop\":false,\"dayNightPhase\":4}]"));
}

void tst_PlasmaApply::testIniEditor()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/appletsrc");
    const QByteArray original =
        "[Containments][1]\n"
        "formfactor=0\n"
        "# a comment\n"
        "someKey=keep\n"
        "\n"
        "[Containments][2]\n"
        "formfactor=4\n";
    writeFile(path, original);

    plasmaapply::Ini ini = plasmaapply::Ini::fromFile(path);
    const QStringList wsec{QStringLiteral("Containments"),
                           QStringLiteral("1"),
                           QStringLiteral("Wallpaper"),
                           QStringLiteral("org.kde.image"),
                           QStringLiteral("General")};
    QCOMPARE(ini.get(wsec, QStringLiteral("Image")), QString());
    ini.set(wsec, QStringLiteral("Image"), QStringLiteral("file:///x.png"));
    QCOMPARE(ini.get(wsec, QStringLiteral("Image")), QStringLiteral("file:///x.png"));
    ini.set({QStringLiteral("Containments"), QStringLiteral("1")},
            QStringLiteral("formfactor"),
            QStringLiteral("2"));
    QCOMPARE(ini.get({QStringLiteral("Containments"), QStringLiteral("1")},
                     QStringLiteral("formfactor")),
             QStringLiteral("2"));
    QVERIFY(ini.save(path));

    const QByteArray saved = readFile(path);
    // Untouched content survives byte-for-byte; the new key lands appended to its section.
    QVERIFY(saved.contains("# a comment\n"));
    QVERIFY(saved.contains("someKey=keep\n"));
    QVERIFY(saved.contains("[Containments][1][Wallpaper][org.kde.image][General]\n"));
    QVERIFY(saved.contains("Image=file:///x.png\n"));
    QVERIFY(saved.indexOf("formfactor=2\n") < saved.indexOf("[Containments][1][Wallpaper]"));
    // The atomic write left no tmp file behind.
    const QStringList leftovers = QDir(dir.path()).entryList(QDir::Files | QDir::Hidden)
                                     .filter(QRegularExpression(QStringLiteral("^\\.kwal-rc-")));
    QCOMPARE(leftovers.size(), 0);
}

void tst_PlasmaApply::testIniPreservation()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/kscreenlockerrc");
    const QByteArray original =
        "[Greeter]\n"
        "WallpaperPlugin=org.kde.image\n"
        "\n"
        "[Greeter][Wallpaper][org.kde.image][General]\n"
        "Image=file:///old.png\n";
    writeFile(path, original);

    plasmaapply::Ini ini = plasmaapply::Ini::fromFile(path);
    QVERIFY(ini.save(path));
    QCOMPARE(readFile(path), original);

    const QString missing = dir.path() + QStringLiteral("/absent");
    plasmaapply::Ini empty = plasmaapply::Ini::fromFile(missing);
    empty.set({QStringLiteral("Greeter")}, QStringLiteral("WallpaperPlugin"), QStringLiteral("org.kde.image"));
    QVERIFY(empty.save(missing));
    QVERIFY(readFile(missing).contains("[Greeter]\nWallpaperPlugin=org.kde.image\n"));
}

void tst_PlasmaApply::testDesktopIds()
{
    const QByteArray content =
        "[Containments][1]\n"
        "formfactor=0\n"
        "lastScreen=0\n"
        "\n"
        "[Containments][2]\n"
        "formfactor=4\n"
        "\n"
        "[Containments][x]\n"
        "formfactor=0\n";
    plasmaapply::Ini ini(QString::fromUtf8(content));
    // Only the numeric containment with formfactor 0 (a desktop) counts.
    QCOMPARE(ini.desktopIds(), QStringList{QStringLiteral("1")});
}

void tst_PlasmaApply::testPruneVideoUrls()
{
    PlasmaApply apply;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString alive = dir.path() + QStringLiteral("/alive.mp4");
    const QString spaced = dir.path() + QStringLiteral("/my clip.mp4");
    writeFile(alive, QByteArray("video"));
    writeFile(spaced, QByteArray("video"));

    const QString aliveUrl = plasmaapply::fileUrl(alive);
    const QString spacedUrl = plasmaapply::fileUrl(spaced);
    const QString deadUrl = plasmaapply::fileUrl(dir.path() + QStringLiteral("/dead.mp4"));

    // Dead file:// URL dropped; the live one and the non-file:// entry stay, in order.
    const QString playlist = QStringLiteral(
        "[{\"filename\":\"%1\",\"enabled\":true,\"duration\":0},"
        "{\"filename\":\"%2\",\"enabled\":false,\"duration\":0},"
        "{\"filename\":\"http://remote/clip.mp4\",\"enabled\":false}]")
        .arg(deadUrl, spacedUrl);
    const QJsonArray pruned = QJsonDocument::fromJson(apply.pruneVideoUrls(playlist).toUtf8()).array();
    QCOMPARE(pruned.size(), 2);
    QCOMPARE(pruned.at(0).toObject().value(QStringLiteral("filename")).toString(), spacedUrl);
    QCOMPARE(pruned.at(0).toObject().value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(pruned.at(1).toObject().value(QStringLiteral("filename")).toString(),
             QStringLiteral("http://remote/clip.mp4"));

    const QString spacedOnly = QStringLiteral("[{\"filename\":\"%1\",\"enabled\":true}]").arg(spacedUrl);
    QCOMPARE(apply.pruneVideoUrls(spacedOnly), spacedOnly);

    // Nothing dead: byte-identical return; empty and non-JSON input pass through untouched.
    const QString aliveOnly = QStringLiteral("[{\"filename\":\"%1\",\"enabled\":true}]").arg(aliveUrl);
    QCOMPARE(apply.pruneVideoUrls(aliveOnly), aliveOnly);
    QCOMPARE(apply.pruneVideoUrls(QString()), QString());
    QCOMPARE(apply.pruneVideoUrls(QStringLiteral("{ not json")), QStringLiteral("{ not json"));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    tst_PlasmaApply test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_plasmaapply.moc"
