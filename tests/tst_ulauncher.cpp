#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"
#include "ulaunchercontroller.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
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
 }

class tst_Ulauncher : public QObject
{
    Q_OBJECT

private slots:
    void testExtractCssColors();
    void testReplaceCssColors();
    void testParseManifestColors();
    void testUpdateManifestColors();
    void testPreviewReflectsTheme();
    void testPreviewLiveOverride();
    void testPreviewResolvesReferences();
    void testPreviewEmptyUsesDefaults();
    void testApplyThemeWritesAndBacksUp();
    void testRestoreThemeBackup();
    void testControllerLoadsColors();
    void testControllerEditUpdatesPreview();
    void testControllerApplyWritesFile();
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

void tst_Ulauncher::testPreviewReflectsTheme()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #1a2b3c;\n@define-color input_color #ff8800;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

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
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #1a2b3c;\n"));

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
    // window_bg references bg_color; the preview must resolve the reference.
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color bg_color #334455;\n@define-color window_bg @bg_color;\n"));

    const QString html = tools::ulauncherPreviewHtml(dir, {}, 0.75, 0, QString(), QString());
    QVERIFY(html.contains(QStringLiteral("#334455")));
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
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #111111;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

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
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #111111;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

    tools::ColorMap themeColors;
    themeColors[QStringLiteral("window_bg")] = QStringLiteral("#222222");
    QVERIFY(tools::applyTheme(dir, {}, themeColors).isEmpty());

    QVERIFY(tools::restoreThemeBackup(dir));
    QVERIFY(readFile(dir + QStringLiteral("/theme.css"))
                .contains(QStringLiteral("@define-color window_bg #111111;")));
}

void tst_Ulauncher::testControllerLoadsColors()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #1a2b3c;\n@define-color input_color #ff8800;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

    UlauncherController controller;
    controller.setThemePath(dir);

    const int index = controller.colorNames().indexOf(QStringLiteral("window_bg"));
    QVERIFY(index >= 0);
    QCOMPARE(controller.colorValues().at(index).toString(), QStringLiteral("#1a2b3c"));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#1a2b3c")));
}

void tst_Ulauncher::testControllerEditUpdatesPreview()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #1a2b3c;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

    UlauncherController controller;
    controller.setThemePath(dir);
    controller.setColorIndex(controller.colorNames().indexOf(QStringLiteral("window_bg")));

    QVERIFY(controller.previewHtml().contains(QStringLiteral("#1a2b3c")));
    controller.setDraftColor(QStringLiteral("#00ff00"));
    QVERIFY(controller.previewHtml().contains(QStringLiteral("#00ff00")));
}

void tst_Ulauncher::testControllerApplyWritesFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path();
    writeFile(dir + QStringLiteral("/theme.css"),
              QStringLiteral("@define-color window_bg #1a2b3c;\n"));
    writeFile(dir + QStringLiteral("/manifest.json"), QStringLiteral("{}"));

    UlauncherController controller;
    controller.setThemePath(dir);
    controller.setColorIndex(controller.colorNames().indexOf(QStringLiteral("window_bg")));
    controller.setDraftColor(QStringLiteral("#222233"));

    QSignalSpy spy(&controller, &UlauncherController::operationResult);
    controller.applyConfig();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    QVERIFY(readFile(dir + QStringLiteral("/theme.css"))
                .contains(QStringLiteral("@define-color window_bg #222233;")));
    QVERIFY(controller.hasBackup());
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
