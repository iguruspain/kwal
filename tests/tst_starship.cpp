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
    void testPreviewFollowsPaletteSelection();
    void testRefreshIfChangedFollowsExternalWrite();
    void testRefreshIfChangedKeepsTemplateDraft();
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

    // A second apply backs up the first applied state, not the original.
    palettes["New"]["bg"] = QStringLiteral("#abcdef");
    QVERIFY(tools::applyStarshipPalettes(path, palettes, QStringLiteral("New")));

    QVERIFY(tools::restoreStarshipBackup(path));
    const auto result = tools::readStarshipConfig(path);
    QVERIFY(result.valid);
    QCOMPARE(result.activePalette, QStringLiteral("New"));
    QCOMPARE(result.palettes.at("New").at("bg"), QStringLiteral("#123456"));
}

void tst_Starship::testPreviewHtml()
{
    // `format` must stay in the root table (before any [table] header).
    const toml::table doc = toml::parse(
        "palette=\"Test\"\n"
        "format=\"$directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#1a2b3c\"\n"
        "fg=\"#ffffff\"\n");
    const QString html = tools::starshipPreviewHtml(doc, 0, 1.0, 800);
    QVERIFY(html.contains(QStringLiteral("<html")));
    QVERIFY(html.contains(QStringLiteral("terminal")));
    QVERIFY(html.contains(QStringLiteral("rgba(24,24,24,0.6)")));
    // The blinking terminal cursor is part of the preview.
    QCOMPARE(html.count(QStringLiteral("<span class=\"caret\"></span>")), 1);
    QVERIFY(html.contains(QStringLiteral("caret-blink")));

    // With a line break the caret stays on the prompt line, before the
    // line that splits it from the rest.
    const toml::table doc2 = toml::parse(
        "palette=\"Test\"\n"
        "format=\"$username $line_break $directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#1a2b3c\"\n"
        "fg=\"#ffffff\"\n");
    const QString html2 = tools::starshipPreviewHtml(doc2, 0, 1.0, 800);
    // With a line break the caret still sits at the very end of the format,
    // i.e. after the line break.
    QCOMPARE(html2.count(QStringLiteral("<span class=\"caret\"></span>")), 1);
    QVERIFY(html2.indexOf(QStringLiteral("<span class=\"caret\">"))
                > html2.indexOf(QStringLiteral("</div><div class=\"prompt-line\">")));
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

void tst_Starship::testPreviewFollowsPaletteSelection()
{
    const QString path = tools::starshipConfigPath();
    QDir(QFileInfo(path).absolutePath()).mkpath(QStringLiteral("."));
    writeFile(path, QStringLiteral(
        "palette=\"A\"\n"
        "format=\"$directory\"\n"
        "[palettes.A]\n"
        "fg=\"#a00000\"\n"
        "bg=\"#111111\"\n"
        "[palettes.B]\n"
        "fg=\"#00aa00\"\n"
        "bg=\"#222222\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));

    StarshipController controller;
    // The document's active palette (A) is listed first and previewed first.
    QCOMPARE(controller.draftPaletteIndex(), 0);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#a00000")));
    QVERIFY(!controller.previewHtml().contains(QStringLiteral("#00aa00")));

    // Switching the palette (as the combo does) re-renders the draft
    // preview with that palette's colors.
    controller.setDraftPaletteIndex(1);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#00aa00")));
    QVERIFY(!controller.previewHtml().contains(QStringLiteral("#a00000")));

    // The on-disk preview is unaffected by the draft selection.
    QVERIFY(controller.currentPreviewHtml().contains(QStringLiteral("#a00000")));

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

// The tab used to re-read starship.toml only on Reset, so a rewrite by
// matugen stayed invisible until then.
void tst_Starship::testRefreshIfChangedFollowsExternalWrite()
{
    const QString path = tools::starshipConfigPath();
    QDir(QFileInfo(path).absolutePath()).mkpath(QStringLiteral("."));
    writeFile(path, QStringLiteral(
        "palette=\"Test\"\n"
        "format=\"$directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#111111\"\n"
        "fg=\"#ffffff\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));

    StarshipController controller;
    QCOMPARE(controller.paletteValues().at(0).toList().at(0).toString(),
             QStringLiteral("#111111"));

    // An activation that finds the file untouched must not drop the palette
    // the user is drafting.
    controller.setColorValue(QStringLiteral("fg"), QStringLiteral("#33aaff"));
    controller.refreshIfChanged();
    QCOMPARE(controller.draftColor(), QStringLiteral("#33aaff"));
    QCOMPARE(controller.paletteValues().at(0).toList().at(1).toString(),
             QStringLiteral("#33aaff"));

    // An external rewrite is followed on the next activation: the draft now
    // describes a file that no longer exists, so it is replaced.
    writeFile(path, QStringLiteral(
        "palette=\"Test\"\n"
        "format=\"$directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#222222\"\n"
        "fg=\"#eeeeee\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));
    controller.refreshIfChanged();
    QCOMPARE(controller.paletteValues().at(0).toList().at(0).toString(),
             QStringLiteral("#222222"));
    QCOMPARE(controller.paletteValues().at(0).toList().at(1).toString(),
             QStringLiteral("#eeeeee"));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#222222")));
    QVERIFY(!controller.previewHtml().contains(QStringLiteral("#33aaff")));

    // The re-read re-armed the fingerprint: the next activation is a no-op
    // and keeps a fresh draft edit.
    controller.setColorValue(QStringLiteral("bg"), QStringLiteral("#00ff00"));
    controller.refreshIfChanged();
    QCOMPARE(controller.paletteValues().at(0).toList().at(0).toString(),
             QStringLiteral("#00ff00"));

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

// While a template is being edited, an external rewrite of the applied
// config must follow the "Current" side only: the template's draft palettes
// are the work in progress, not the config's.
void tst_Starship::testRefreshIfChangedKeepsTemplateDraft()
{
    const QString path = tools::starshipConfigPath();
    QDir(QFileInfo(path).absolutePath()).mkpath(QStringLiteral("."));
    writeFile(path, QStringLiteral(
        "palette=\"Test\"\n"
        "format=\"$directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#111111\"\n"
        "fg=\"#ffffff\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));

    const QString templateDir = tools::starshipTemplateDir();
    QDir(templateDir).mkpath(QStringLiteral("."));
    const QString tmpl = templateDir + QStringLiteral("/Alt.toml");
    writeFile(tmpl, QStringLiteral(
        "palette=\"Alt\"\n"
        "format=\"$directory\"\n"
        "[palettes.Alt]\n"
        "bg=\"#777777\"\n"
        "fg=\"#888888\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));

    StarshipController controller;
    QCOMPARE(controller.templateNames().contains(QStringLiteral("Alt")), true);
    controller.setTemplateIndex(0);
    QCOMPARE(controller.selectedTemplate(), QStringLiteral("Alt"));
    QCOMPARE(controller.paletteValues().at(0).toList().at(0).toString(),
             QStringLiteral("#777777"));

    controller.setColorValue(QStringLiteral("bg"), QStringLiteral("#00ff00"));
    const QString currentBefore = controller.currentPreviewHtml();
    QVERIFY(currentBefore.contains(QStringLiteral("#111111")));

    writeFile(path, QStringLiteral(
        "palette=\"Test\"\n"
        "format=\"$directory\"\n"
        "[palettes.Test]\n"
        "bg=\"#222222\"\n"
        "fg=\"#eeeeee\"\n"
        "[directory]\n"
        "format=\"[$path]($style)\"\n"
        "style=\"fg:fg bg:bg\"\n"));
    controller.refreshIfChanged();

    // The template and its draft colors stay exactly as they were...
    QCOMPARE(controller.selectedTemplate(), QStringLiteral("Alt"));
    QCOMPARE(controller.paletteValues().at(0).toList().at(0).toString(),
             QStringLiteral("#00ff00"));
    // ...while the applied-config preview follows the rewritten file.
    QVERIFY(controller.currentPreviewHtml().contains(QStringLiteral("#222222")));
    QVERIFY(!controller.currentPreviewHtml().contains(QStringLiteral("#111111")));

    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
    QFile::remove(tmpl);
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
