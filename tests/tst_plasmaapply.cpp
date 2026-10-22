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
    // Unreserved characters pass through, everything else is percent-encoded
    // in uppercase hex (the encoding Plasma keeps in config file URLs).
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/wall.png")),
             QStringLiteral("file:///home/user/wall.png"));
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/my wall.png")),
             QStringLiteral("file:///home/user/my%20wall.png"));
    QCOMPARE(plasmaapply::fileUrl(QStringLiteral("/home/user/á.mp4")),
             QStringLiteral("file:///home/user/%C3%A1.mp4"));
    QCOMPARE(plasmaapply::rawVideoUrl(QStringLiteral("/home/user/clip.mp4")),
             QStringLiteral("file:///home/user/clip.mp4"));
}

void tst_PlasmaApply::testComputeVideoUrls()
{
    const QString target = QStringLiteral("file:///v/b.mp4");

    // No existing playlist: the target is appended with the plugin UI's
    // default fields.
    QJsonArray entries = QJsonDocument::fromJson(plasmaapply::computeVideoUrls(QString(), target).toUtf8()).array();
    QCOMPARE(entries.size(), 1);
    QJsonObject first = entries.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("filename")).toString(), target);
    QCOMPARE(first.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(first.value(QStringLiteral("loop")).toBool(), false);
    QCOMPARE(first.value(QStringLiteral("dayNightPhase")).toInt(), 4);

    // Existing playlist: the target becomes enabled, the others disabled,
    // order and per-entry custom fields preserved.
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
    // The section is absent: get is empty, set creates it.
    QCOMPARE(ini.get(wsec, QStringLiteral("Image")), QString());
    ini.set(wsec, QStringLiteral("Image"), QStringLiteral("file:///x.png"));
    QCOMPARE(ini.get(wsec, QStringLiteral("Image")), QStringLiteral("file:///x.png"));
    // A set on an existing key replaces the value in place.
    ini.set({QStringLiteral("Containments"), QStringLiteral("1")},
            QStringLiteral("formfactor"),
            QStringLiteral("2"));
    QCOMPARE(ini.get({QStringLiteral("Containments"), QStringLiteral("1")},
                     QStringLiteral("formfactor")),
             QStringLiteral("2"));
    QVERIFY(ini.save(path));

    const QByteArray saved = readFile(path);
    // Everything not touched survives byte-for-byte; the new key landed in
    // its section, appended after the last one.
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

    // A load + save without any edit is byte-for-byte identical.
    plasmaapply::Ini ini = plasmaapply::Ini::fromFile(path);
    QVERIFY(ini.save(path));
    QCOMPARE(readFile(path), original);

    // A missing file loads as an empty editor and set still works.
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    tst_PlasmaApply test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_plasmaapply.moc"
