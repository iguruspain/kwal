#include "templateinstaller.h"
#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"
#include "ulaunchercontroller.h"

#include <KConfig>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>
#include <QtTest>

#include <map>

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

// A minimal valid Ulauncher theme: theme.css colors plus manifest highlight
// colors, mirroring the bundled theme_template.
void writeTheme(const QString &dir)
{
    QDir(dir).mkpath(QStringLiteral("."));
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color bg_color #1a2b3c;\n"
                             "@define-color window_bg @bg_color;\n"
                             "@define-color input_color #ff8800;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"),
              QStringLiteral("{\"name\": \"test\","
                             "\"when_selected\": \"#4a90d9\","
                             "\"matched_text_hl_colors\": {"
                             "\"when_selected\": \"#4a90d9\","
                             "\"when_not_selected\": \"#5a6a7a\"}}"));
}
} // namespace

// Index of the theme entry whose path matches, or -1.
int indexOfPath(const UlauncherController &controller, const QString &path)
{
    const auto entries = controller.themeEntries();
    for (int i = 0; i < entries.size(); i++)
        if (entries.at(i).toMap().value(QStringLiteral("path")).toString() == path)
            return i;
    return -1;
}

class tst_Ulauncher : public QObject
{
    Q_OBJECT

private slots:
    void testExtractCssColors();
    void testReplaceCssColors();
    void testCssColorOrder();
    void testParseManifestColors();
    void testUpdateManifestColors();
    void testListThemeDirs();
    void testPreviewReflectsTheme();
    void testPreviewLiveOverride();
    void testPreviewResolvesReferences();
    void testStripCssComments();
    void testPreviewInjectsThemeCss();
    void testPreviewIconFilePath();
    void testPreviewRows();
    void testPreviewEmptyUsesDefaults();
    void testApplyThemeWritesAndBacksUp();
    void testRestoreThemeBackup();
    void testCreateAndDeleteTheme();
    void testSetActiveAndResolveTheme();
    void testControllerLoadsSections();
    void testControllerThemeEntries();
    void testControllerEditUpdatesPreview();
    void testControllerPreviewScale();
    void testControllerWindowWidth();
    void testControllerPreviewSettings();
    void testControllerSaveWritesFile();
    void testControllerCreateDelete();
    void testControllerApplyToUlauncher();
};

void tst_Ulauncher::testExtractCssColors()
{
    const QString css = QStringLiteral("@define-color a #111111;\n@define-color b @a;\n");
    const auto colors = tools::extractCssColors(css);
    QCOMPARE(colors.size(), 2);
    QCOMPARE(colors.value("a"), QStringLiteral("#111111"));
    QCOMPARE(colors.value("b"), QStringLiteral("@a"));
}

void tst_Ulauncher::testReplaceCssColors()
{
    const QString css = QStringLiteral("@define-color a #111111;\n.foo { color: @a; }\n");
    tools::ColorMap updates;
    updates["a"] = QStringLiteral("#222222");
    const QString out = tools::replaceCssColors(css, updates);
    QVERIFY(out.contains(QStringLiteral("@define-color a #222222;")));
    QVERIFY(out.contains(QStringLiteral("color: @a;")));
}

void tst_Ulauncher::testCssColorOrder()
{
    const QString css = QStringLiteral("@define-color zebra #010101;\n@define-color alpha #020202;\n");
    const QStringList order = tools::cssColorOrder(css);
    QStringList expected;
    expected << QStringLiteral("zebra") << QStringLiteral("alpha");
    QCOMPARE(order, expected);
}

void tst_Ulauncher::testParseManifestColors()
{
    const QString manifest = QStringLiteral(
        "{\"when_selected\":\"#ff0000\","
        "\"matched_text_hl_colors\":{\"when_selected\":\"#00ff00\","
        "\"when_not_selected\":\"#0000ff\"}}");
    const auto colors = tools::parseManifestColors(manifest);
    QCOMPARE(colors.value("when_selected"), QStringLiteral("#ff0000"));
    QCOMPARE(colors.value("matched_text_hl_colors_when_not_selected"),
             QStringLiteral("#0000ff"));
}

void tst_Ulauncher::testUpdateManifestColors()
{
    const QString manifest =
        QStringLiteral("{\"colors\":{\"bg_color\":\"#000000\"}}");
    tools::ColorMap updates;
    updates[QStringLiteral("colors_bg_color")] = QStringLiteral("#123456");
    const QString out = tools::updateManifestColors(manifest, updates);
    QVERIFY(!out.isEmpty());
    QVERIFY(out.contains(QStringLiteral("\"bg_color\": \"#123456\"")));
}

void tst_Ulauncher::testListThemeDirs()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Only sub-directories with a manifest.json are listed, sorted by name.
    writeTheme(tmp.path() + QStringLiteral("/b"));
    writeTheme(tmp.path() + QStringLiteral("/a"));
    QDir(tmp.path()).mkpath(QStringLiteral("/empty"));
    const QStringList dirs = tools::listThemeDirs(tmp.path());
    QCOMPARE(dirs.size(), 2);
    QCOMPARE(QFileInfo(dirs.first()).fileName(), QStringLiteral("a"));
    QCOMPARE(QFileInfo(dirs.last()).fileName(), QStringLiteral("b"));
    QVERIFY(tools::listThemeDirs(tmp.path() + QStringLiteral("/missing")).isEmpty());
}

void tst_Ulauncher::testPreviewReflectsTheme()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    const QString html = tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString());
    QVERIFY(html.contains(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(QStringLiteral("#1a2b3c")));
    QVERIFY(html.contains(QStringLiteral("#ff8800")));
}

void tst_Ulauncher::testPreviewLiveOverride()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    std::map<QString, QString> overrides;
    overrides[QStringLiteral("window_bg")] = "#00ff00";
    const QString html = tools::ulauncherPreviewHtml(dir, overrides, 0.75, 0, QString(), QString());
    QVERIFY(html.contains(QStringLiteral("#00ff00")));
}

void tst_Ulauncher::testPreviewResolvesReferences()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);
    // window_bg references bg_color; the preview must resolve the reference.

    const QString html = tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString());
    QVERIFY(html.contains(QStringLiteral("#1a2b3c")));
}

void tst_Ulauncher::testStripCssComments()
{
    const QString css = QStringLiteral(
        "@define-color a #111111;\n"
        "/* @define-color a #222222; */\n"
        ".foo { color: @a; }\n");
    // Commented-out declarations must not be parsed.
    const auto colors = tools::extractCssColors(css);
    QCOMPARE(colors.size(), 1);
    QCOMPARE(colors.value("a"), QStringLiteral("#111111"));
    QVERIFY(!tools::stripCssComments(css)
                 .contains(QStringLiteral("@define-color a #222222")));
    QVERIFY(tools::cssColorOrder(css) == QStringList({QStringLiteral("a")}));
}

void tst_Ulauncher::testPreviewInjectsThemeCss()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg rgba(1, 2, 3, 0.5);\n"
                             ".selected.item-box {\n"
                             "    background-image: linear-gradient(to right, #3e4388, #125e68);\n"
                             "    border-radius: 10px;\n"
                             "}\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

    const QString html = tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString());
    // The real theme CSS is injected: the selected-row gradient survives,
    // the translucent window background (rgba) passes through, and the
    // window shadow is drawn over a transparent page background.
    QVERIFY(html.contains(QStringLiteral("linear-gradient")));
    QVERIFY(html.contains(QStringLiteral("rgba(1, 2, 3, 0.5)")));
    QVERIFY(html.contains(QStringLiteral("background:transparent")));
    QVERIFY(html.contains(QStringLiteral("box-shadow")));
    QVERIFY(!html.contains(QStringLiteral("@define-color")));
}

void tst_Ulauncher::testPreviewRows()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    const QList<tools::UlauncherRow> rows = {
        tools::UlauncherRow(QStringLiteral("no-such-icon"), QStringLiteral("Spotify"),
                            QStringLiteral("Alt+1"), true),
        tools::UlauncherRow(QString(), QStringLiteral("Spectacle"),
                            QStringLiteral("Alt+2"), false),
    };
    const QString html =
        tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString(), rows,
                                    QStringLiteral("sptf"));
    // Fuzzy-match highlighting, per selection state, of the typed query's
    // characters inside each name.
    QVERIFY(html.contains(QStringLiteral(
        "<span class=\"hl-s\">S</span><span class=\"hl-s\">p</span>o"
        "<span class=\"hl-s\">t</span>i<span class=\"hl-s\">f</span>y")));
    QVERIFY(html.contains(QStringLiteral(
        "<span class=\"hl-n\">S</span><span class=\"hl-n\">p</span>ec"
        "<span class=\"hl-n\">t</span>acle")));
    // The input shows the typed query; shortcuts are rendered.
    QVERIFY(html.contains(QStringLiteral(">sptf<span class=\"caret\">")));
    QVERIFY(html.contains(QStringLiteral(">Alt+1<")));
    QVERIFY(html.contains(QStringLiteral(">Alt+2<")));
    // Unknown icons fall back to the placeholder.
    QVERIFY(html.contains(QStringLiteral("item-icon-svg")));
}

void tst_Ulauncher::testPreviewIconFilePath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    // An existing file path is used verbatim as the icon source.
    const QString iconPath = dir + QStringLiteral("/icon.png");
    {
        QFile f(iconPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\x89PNG");
    }
    const QList<tools::UlauncherRow> rows = {
        tools::UlauncherRow(iconPath, QStringLiteral("App"), QStringLiteral("Alt+1"), true),
    };
    const QString html =
        tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString(), rows,
                                    QStringLiteral("ap"));
    QVERIFY(html.contains(QStringLiteral("<img class=\"item-icon\" src=\"")
                          + QUrl::fromLocalFile(iconPath).toString(QUrl::FullyEncoded)
                          + QStringLiteral("\">")));
    // No placeholder markup for the resolved icon (the class name itself
    // still appears in the CSS rules).
    QVERIFY(!html.contains(QStringLiteral("<svg class=\"item-icon-svg\"")));
}

void tst_Ulauncher::testPreviewEmptyUsesDefaults()
{
    const QString html = tools::ulauncherPreviewHtml(QString(), {}, 0.75, 0, QString(), QString());
    QVERIFY(html.contains(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(QStringLiteral("#2b2b2b"))); // default window background
}

void tst_Ulauncher::testApplyThemeWritesAndBacksUp()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    tools::ColorMap themeColors;
    themeColors[QStringLiteral("window_bg")] = QStringLiteral("#222222");
    const QString error = tools::applyTheme(dir, {}, themeColors);
    QVERIFY(error.isEmpty());

    QVERIFY(readFile(dir + QStringLiteral("/theme.css"))
                .contains(QStringLiteral("@define-color window_bg #222222;")));
    QVERIFY(tools::hasThemeBackup(dir));
}

void tst_Ulauncher::testRestoreThemeBackup()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeTheme(dir);

    tools::ColorMap themeColors;
    themeColors[QStringLiteral("window_bg")] = QStringLiteral("#222222");
    QVERIFY(tools::applyTheme(dir, {}, themeColors).isEmpty());

    QVERIFY(tools::restoreThemeBackup(dir));
    QVERIFY(readFile(dir + QStringLiteral("/theme.css"))
                .contains(QStringLiteral("@define-color window_bg @bg_color;")));
}

void tst_Ulauncher::testCreateAndDeleteTheme()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source");
    writeTheme(source);

    const QString newPath = tools::createTheme(source, QStringLiteral("Copy"));
    QVERIFY(!newPath.isEmpty());
    QCOMPARE(QFileInfo(newPath).fileName(), QStringLiteral("Copy"));
    QVERIFY(readFile(newPath + QStringLiteral("/manifest.json"))
                .contains(QStringLiteral("\"name\": \"Copy\"")));

    // Duplicate names are rejected.
    QVERIFY(tools::createTheme(source, QStringLiteral("Copy")).isEmpty());

    // The active theme cannot be deleted.
    QVERIFY(!tools::deleteTheme(newPath, newPath));
    QVERIFY(tools::deleteTheme(newPath, source));
    QVERIFY(!QDir(newPath).exists());
}

void tst_Ulauncher::testSetActiveAndResolveTheme()
{
    // Themes resolve only from the known roots, so use a user theme.
    const QString themeDir = tools::userThemesDir() + QStringLiteral("/Maugen");
    writeTheme(themeDir);
    writeFile(themeDir + QStringLiteral("/manifest.json"),
              QStringLiteral("{\"name\": \"Maugen\","
                             "\"matched_text_hl_colors\": {\"when_selected\": \"#4a90d9\","
                             "\"when_not_selected\": \"#5a6a7a\"}}"));

    const QString error = tools::setActiveThemePath(themeDir);
    QVERIFY(error.isEmpty());
    QCOMPARE(tools::resolveActiveThemePath(), themeDir);

    // Clean up shared state for the following tests.
    QDir(themeDir).removeRecursively();
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                  + QStringLiteral("/ulauncher/settings.json"));
}

void tst_Ulauncher::testControllerLoadsSections()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + QStringLiteral("/theme");
    writeTheme(dir);

    UlauncherController controller;
    controller.setThemePath(dir);

    const auto sections = controller.paletteSections();
    QCOMPARE(sections.size(), 2);

    const QVariantMap manifest = sections.at(0).toMap();
    QCOMPARE(manifest.value(QStringLiteral("title")).toString(),
             QStringLiteral("Manifest Colors"));
    const auto manifestRows = manifest.value(QStringLiteral("colors")).toList();
    QCOMPARE(manifestRows.size(), 2);
    QCOMPARE(manifestRows.at(0).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("when_selected"));
    QCOMPARE(manifestRows.at(0).toMap().value(QStringLiteral("display")).toString(),
             QStringLiteral("#4a90d9"));

    const QVariantMap theme = sections.at(1).toMap();
    QCOMPARE(theme.value(QStringLiteral("title")).toString(),
             QStringLiteral("Theme Colors"));
    const auto themeRows = theme.value(QStringLiteral("colors")).toList();
    QCOMPARE(themeRows.size(), 3); // bg_color, window_bg, input_color
    QCOMPARE(themeRows.at(0).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("bg_color"));
    // window_bg references bg_color; the swatch shows the resolved value.
    QCOMPARE(themeRows.at(1).toMap().value(QStringLiteral("display")).toString(),
             QStringLiteral("#1a2b3c"));

    QVERIFY(controller.previewHtml().contains(QStringLiteral("#1a2b3c")));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#4a90d9")));
}

void tst_Ulauncher::testControllerThemeEntries()
{
    // Templates from the installed template dir, user themes from the
    // Ulauncher config location; both are sandboxed via XDG_CONFIG_HOME.
    const QString templateTheme =
        tools::templateDir() + QStringLiteral("/ulauncher/Maugen");
    writeTheme(templateTheme);
    const QString userTheme = tools::userThemesDir() + QStringLiteral("/Custom");
    writeTheme(userTheme);

    {
        UlauncherController controller;
        const auto entries = controller.themeEntries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Maugen (Template)"));
        QVERIFY(entries.at(0).toMap().value(QStringLiteral("isTemplate")).toBool());
        QCOMPARE(entries.at(1).toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Custom"));
        QCOMPARE(controller.selectedThemeIndex(), 0);
    }

    // The active theme marker and selection follow settings.json.
    QVERIFY(tools::setActiveThemePath(userTheme).isEmpty());
    {
        UlauncherController controller;
        const auto entries = controller.themeEntries();
        QVERIFY(!entries.at(0).toMap().value(QStringLiteral("isCurrent")).toBool());
        QVERIFY(entries.at(1).toMap().value(QStringLiteral("isCurrent")).toBool());
        QCOMPARE(entries.at(1).toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Custom (Current)"));
        QCOMPARE(controller.selectedThemeIndex(), 1);
    }
}

void tst_Ulauncher::testControllerEditUpdatesPreview()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + QStringLiteral("/theme");
    writeTheme(dir);

    UlauncherController controller;
    controller.setThemePath(dir);

    QVERIFY(controller.previewHtml().contains(QStringLiteral("#ff8800")));
    controller.setColorValue(QStringLiteral("input_color"), QStringLiteral("#00ff00"));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#00ff00")));
    // Unsaved edits must not touch the file.
    QVERIFY(readFile(dir + QStringLiteral("/theme.css")).contains(QStringLiteral("#ff8800")));
}

void tst_Ulauncher::testControllerPreviewScale()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + QStringLiteral("/theme");
    writeTheme(dir);

    UlauncherController controller;
    controller.setThemePath(dir);

    QVERIFY(controller.previewHtml().contains(QStringLiteral("scale(0.75)")));
    controller.setPreviewScale(0.5);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("scale(0.5)")));
    // Out-of-range values are clamped.
    controller.setPreviewScale(5.0);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("scale(2)")));
    // Re-setting the same scale does not regenerate.
    const QString before = controller.previewHtml();
    controller.setPreviewScale(2.0);
    QCOMPARE(controller.previewHtml(), before);
}

void tst_Ulauncher::testControllerWindowWidth()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + QStringLiteral("/theme");
    writeTheme(dir);

    UlauncherController controller;
    controller.setThemePath(dir);
    controller.setPreviewWindowWidth(800);
    QCOMPARE(controller.previewWindowWidth(), 800);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("width:800px")));
    // Out-of-range values are clamped.
    controller.setPreviewWindowWidth(100);
    QCOMPARE(controller.previewWindowWidth(), 400);
    QVERIFY(controller.previewHtml().contains(QStringLiteral("width:400px")));
    controller.setPreviewWindowWidth(5000);
    QCOMPARE(controller.previewWindowWidth(), 1200);
}

void tst_Ulauncher::testControllerPreviewSettings()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + QStringLiteral("/kwalrc");

    KConfig config(file, KConfig::SimpleConfig);
    UlauncherController controller(config.group(QStringLiteral("Ulauncher")));

    // Fresh config: the preview settings fall back to the built-in defaults.
    QCOMPARE(controller.previewWindowWidth(), 900);
    QCOMPARE(controller.previewQuery(), QStringLiteral("sptf"));
    QCOMPARE(controller.sampleRows().size(), 2);

    controller.setPreviewWindowWidth(1050);
    controller.setPreviewQuery(QStringLiteral("vim"));
    controller.setSampleRow(0, QVariantMap{
        {QStringLiteral("icon"), QStringLiteral("terminal")},
        {QStringLiteral("name"), QStringLiteral("Vim")},
        {QStringLiteral("shortcut"), QStringLiteral("F1")},
        {QStringLiteral("selected"), true}});
    controller.addSampleRow();
    QCOMPARE(controller.sampleRows().size(), 3);
    controller.removeSampleRow(2);
    QCOMPARE(controller.sampleRows().size(), 2);

    // The preview reflects the typed query and the edited row. The name
    // "Vim" is fully fuzzy-matched by "vim", so it renders as highlighted
    // characters, not a plain word.
    QVERIFY(controller.previewHtml().contains(QStringLiteral(">vim<span class=\"caret\">")));
    QVERIFY(controller.previewHtml().contains(
        QStringLiteral("<span class=\"hl-s\">V</span><span class=\"hl-s\">i</span>"
                       "<span class=\"hl-s\">m</span>")));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("F1")));

    // The edits are persisted and survive a reload.
    UlauncherController reloaded(config.group(QStringLiteral("Ulauncher")));
    QCOMPARE(reloaded.previewWindowWidth(), 1050);
    QCOMPARE(reloaded.previewQuery(), QStringLiteral("vim"));
    QCOMPARE(reloaded.sampleRows().size(), 2);
    const QVariantMap row = reloaded.sampleRows().at(0).toMap();
    QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("Vim"));
    QVERIFY(row.value(QStringLiteral("selected")).toBool());
}

void tst_Ulauncher::testControllerSaveWritesFile()
{
    const QString dir = tools::userThemesDir() + QStringLiteral("/SaveMe");
    writeTheme(dir);
    writeFile(dir + QStringLiteral("/manifest.json"),
              QStringLiteral("{\"name\": \"SaveMe\","
                             "\"when_selected\": \"#4a90d9\","
                             "\"matched_text_hl_colors\": {"
                             "\"when_selected\": \"#4a90d9\","
                             "\"when_not_selected\": \"#5a6a7a\"}}"));

    UlauncherController controller;
    controller.selectTheme(indexOfPath(controller, dir));
    controller.setColorValue(QStringLiteral("input_color"), QStringLiteral("#222233"));
    controller.setColorValue(QStringLiteral("when_selected"), QStringLiteral("#112233"));

    QSignalSpy spy(&controller, &UlauncherController::operationResult);
    controller.saveTheme();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    QVERIFY(readFile(dir + QStringLiteral("/theme.css")).contains(
        QStringLiteral("@define-color input_color #222233;")));
    QVERIFY(readFile(dir + QStringLiteral("/manifest.json")).contains(
        QStringLiteral("\"when_selected\": \"#112233\"")));
    QVERIFY(controller.hasBackup());
}

void tst_Ulauncher::testControllerCreateDelete()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/theme");
    writeTheme(source);

    UlauncherController controller;
    controller.setThemePath(source);

    QSignalSpy spy(&controller, &UlauncherController::operationResult);
    controller.createTheme(QStringLiteral("Copy"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    QVERIFY(QDir(tools::userThemesDir() + QStringLiteral("/Copy")).exists());
    QCOMPARE(QFileInfo(controller.themeEntries()
                           .at(controller.selectedThemeIndex())
                           .toMap()
                           .value(QStringLiteral("path"))
                           .toString())
             .fileName(),
             QStringLiteral("Copy"));

    controller.deleteTheme();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    QVERIFY(!QDir(tools::userThemesDir() + QStringLiteral("/Copy")).exists());
}

void tst_Ulauncher::testControllerApplyToUlauncher()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString themeDir = tools::userThemesDir() + QStringLiteral("/ApplyMe");
    writeTheme(themeDir);
    writeFile(themeDir + QStringLiteral("/manifest.json"),
              QStringLiteral("{\"name\": \"ApplyMe\","
                             "\"matched_text_hl_colors\": {\"when_selected\": \"#4a90d9\","
                             "\"when_not_selected\": \"#5a6a7a\"}}"));

    UlauncherController controller;
    controller.selectTheme(indexOfPath(controller, themeDir));

    QSignalSpy spy(&controller, &UlauncherController::operationResult);
    controller.applyToUlauncher();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    QCOMPARE(tools::resolveActiveThemePath(), themeDir);

    // The applied theme now drives the "current theme" preview.
    QVERIFY(controller.currentPreviewHtml().contains(QStringLiteral("#1a2b3c")));
}

int main(int argc, char *argv[])
{
    // Sandbox user dirs so resolveActiveThemePath / QStandardPaths never touch
    // the real home directory.
    const QString base = QDir::tempPath() + QStringLiteral("/kwal-tst-ulauncher-") +
                         QString::number(QCoreApplication::applicationPid());
    qputenv("HOME", base.toUtf8());
    qputenv("XDG_CONFIG_HOME", (base + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_CACHE_HOME", (base + QStringLiteral("/cache")).toUtf8());
    QCoreApplication app(argc, argv);

    tst_Ulauncher test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ulauncher.moc"
