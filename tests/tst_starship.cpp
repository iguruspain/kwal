#include "starshipcontroller.h"
#include "tools/starshipconfig.h"
#include "tools/starshippreview.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <toml++/toml.hpp>

namespace {
void writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(content.toUtf8());
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}
}

class tst_Starship : public QObject
{
    Q_OBJECT

private slots:
    void testExtractPalettes();
    void testGetActivePalette();
    void testReadStarshipConfig();
    void testApplyStarshipPalettes();
    void testRestoreStarshipBackup();
    void testPreviewHtml();
    void testControllerLoadsAndApplies();
};

void tst_Starship::testExtractPalettes()
{
    const toml::table doc = toml::parse(
        "palette = \"Test\"\n"
        "[palettes.Test]\n"
        "bg = \"#1a1a1a\"\n"
        "fg = \"#ffffff\"\n"
        "[palettes.Other]\n"
        "bg = \"#222222\"\n");
    const auto palettes = tools::extractPalettes(doc);
    QCOMPARE(palettes.size(), 2);
    QCOMPARE(palettes.at("Test").at("bg"), QStringLiteral("#1a1a1a"));
    QCOMPARE(palettes.at("Test").at("fg"), QStringLiteral("#ffffff"));
    QCOMPARE(palettes.at("Other").at("bg"), QStringLiteral("#222222"));
}

void tst_Starship::testGetActivePalette()
{
    const toml::table doc =
        toml::parse("palette = \"Test\"\n[palettes.Test]\nbg = \"#111111\"\n");
    QCOMPARE(tools::getActivePaletteName(doc), QStringLiteral("Test"));

    const toml::table docNoActive = toml::parse("[palettes.Test]\nbg = \"#111111\"\n");
    QVERIFY(tools::getActivePaletteName(docNoActive).isEmpty());
}

void tst_Starship::testReadStarshipConfig()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/starship.toml");
    writeFile(path, QStringLiteral("palette=\"Test\"\n[palettes.Test]\nbg=\"#111111\"\nfg=\"#ffffff\"\n"));

    const auto result = tools::readStarshipConfig(path);
    QVERIFY(result.valid);
    QCOMPARE(result.palettes.size(), 1);
    QCOMPARE(result.activePalette, QStringLiteral("Test"));
    QCOMPARE(result.palettes.at("Test").at("bg"), QStringLiteral("#111111"));

    const auto missing = tools::readStarshipConfig(tmp.path() + QStringLiteral("/nope.toml"));
    QVERIFY(!missing.valid);
}

void tst_Starship::testApplyStarshipPalettes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/starship.toml");
    writeFile(path, QStringLiteral("palette=\"Old\"\n[palettes.Old]\nbg=\"#000000\"\n"));

    tools::PaletteMap palettes;
    palettes["New"]["bg"] = QStringLiteral("#123456");
    palettes["New"]["fg"] = QStringLiteral("#ffffff");
    QVERIFY(tools::applyStarshipPalettes(path, palettes, QStringLiteral("New")));
    QVERIFY(tools::hasStarshipBackup(path));

    const auto result = tools::readStarshipConfig(path);
    QVERIFY(result.valid);
    QCOMPARE(result.activePalette, QStringLiteral("New"));
    QCOMPARE(result.palettes.at("New").at("bg"), QStringLiteral("#123456"));
}

void tst_Starship::testRestoreStarshipBackup()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/starship.toml");
    writeFile(path, QStringLiteral("palette=\"Old\"\n[palettes.Old]\nbg=\"#000000\"\n"));

    tools::PaletteMap palettes;
    palettes["New"]["bg"] = QStringLiteral("#123456");
    QVERIFY(tools::applyStarshipPalettes(path, palettes, QStringLiteral("New")));

    QVERIFY(tools::restoreStarshipBackup(path));
    const auto result = tools::readStarshipConfig(path);
    QCOMPARE(result.activePalette, QStringLiteral("Old"));
    QCOMPARE(result.palettes.at("Old").at("bg"), QStringLiteral("#000000"));
}

void tst_Starship::testPreviewHtml()
{
    const toml::table doc = toml::parse(
        "palette=\"Test\"\n"
        "[palettes.Test]\n"
        "bg=\"#1a2b3c\"\n"
        "fg=\"#ffffff\"\n"
        "format=\"$directory\"\n");
    const QString html = tools::starshipPreviewHtml(doc, 0, 1.0, 800);
    QVERIFY(html.contains(QStringLiteral("<html")));
    QVERIFY(html.contains(QStringLiteral("terminal")));
    QVERIFY(html.contains(QStringLiteral("#181818")));
}

void tst_Starship::testControllerLoadsAndApplies()
{
    const QString path = tools::starshipConfigPath();
    QDir(QFileInfo(path).absolutePath()).mkpath(QStringLiteral("."));
    writeFile(path, QStringLiteral(
        "palette=\"Test\"\n"
        "format=\"$username $directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#111111\"\n"
        "fg=\"#ffffff\"\n"
        "[username]\n"
        "format=\"[$user]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));

    StarshipController controller;
    QCOMPARE(controller.hasConfig(), true);
    QVERIFY(controller.paletteNames().contains(QStringLiteral("Test")));
    QVERIFY(!controller.previewHtml().isEmpty());
    // The prompt is rendered from the parsed file (regression: the preview
    // used to come out empty because the parse validity was never recorded).
    QVERIFY(controller.previewHtml().contains("dev/scripts"));
    QVERIFY(controller.currentPreviewHtml().contains("dev/scripts"));

    // Edit a row by key (the QML color rows use this path).
    controller.setColorValue("fg", "#33aaff");
    QCOMPARE(controller.draftColor(), QStringLiteral("#33aaff"));
    QCOMPARE(controller.paletteValues().at(0).toList().at(1).toString(),
             QStringLiteral("#33aaff"));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#33aaff")));
    // The on-disk file is untouched until apply.
    QVERIFY(!controller.currentPreviewHtml().contains(QStringLiteral("#33aaff")));

    controller.setDraftColorIndex(0); // the "bg" key
    controller.setDraftColor(QStringLiteral("#aabbcc"));

    QSignalSpy spy(&controller, &StarshipController::operationResult);
    controller.applyConfig();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    QVERIFY(readFile(path).contains(QStringLiteral("#aabbcc")));
    QVERIFY(readFile(path).contains(QStringLiteral("#33aaff")));
    QVERIFY(controller.hasBackup());

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

int main(int argc, char *argv[])
{
    // Sandbox user dirs so starshipConfigPath / QStandardPaths never touch the
    // real home directory.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-tst-starship-") +
                         QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_Starship test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_starship.moc"
