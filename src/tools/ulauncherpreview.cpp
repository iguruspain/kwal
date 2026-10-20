#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>

#include <functional>
#include <map>

namespace tools
{

namespace {

// Default color for a theme variable when it is unset (sentinel), missing, or
// unresolvable. Keeps the mockup legible before the user applies a palette.
QString defaultFor(const QString &name)
{
    if (name == "bg_color" || name == "window_bg")
        return QStringLiteral("#2b2b2b");
    if (name == "window_border_color" || name == "prefs_background")
        return QStringLiteral("#3a3a3a");
    if (name == "input_color" || name == "caret_color")
        return QStringLiteral("#f0f0f0");
    if (name == "item_name" || name == "item_name_selected" || name == "selected_fg_color")
        return QStringLiteral("#ffffff");
    if (name == "item_text" || name == "item_text_selected")
        return QStringLiteral("#c8c8c8");
    if (name == "item_box_selected" || name == "selected_bg_color")
        return QStringLiteral("#3d6fb4");
    if (name == "item_shortcut_color" || name == "item_shortcut_color_sel")
        return QStringLiteral("#b0b0b0");
    if (name == "matched_text_hl_colors_when_not_selected"
        || name == "matched_text_hl_colors_when_selected")
        return QStringLiteral("#4a90d9");
    return QStringLiteral("#c8c8c8");
}

QString normalizeHex(const QColor &c)
{
    if (!c.isValid())
        return QStringLiteral("#c8c8c8");
    const QString rgb = QStringLiteral("%1%2%3")
                            .arg(c.red(), 2, 16, QChar('0'))
                            .arg(c.green(), 2, 16, QChar('0'))
                            .arg(c.blue(), 2, 16, QChar('0'));
    if (c.alpha() == 255)
        return QStringLiteral("#%1").arg(rgb);
    return QStringLiteral("#%1%2").arg(rgb).arg(c.alpha(), 2, 16, QChar('0'));
}

// Resolve a raw GTK CSS value (sentinel, @ref, darker(), alpha(), or a plain
// color) to a browser color string, using `eff` (live overrides over the
// parsed @define-color map) to follow variable references.
QString resolveColor(const QString &name,
                     const QString &raw,
                     const std::map<QString, QString> &eff,
                     int depth)
{
    const QString v = raw.trimmed();
    if (v.isEmpty() || v.startsWith("provisional_") || v == "transparent")
        return defaultFor(name);
    if (depth <= 0)
        return defaultFor(name);

    // @variable reference
    if (v.startsWith('@') && v.size() > 1) {
        const QString other = v.mid(1);
        auto it = eff.find(other);
        if (it != eff.end())
            return resolveColor(other, it->second, eff, depth - 1);
        return defaultFor(name);
    }

    // darker(X)
    const QRegularExpression darkRe(QStringLiteral(R"(darker\s*\(\s*([^)]+)\s*\))"));
    const QRegularExpressionMatch dm = darkRe.match(v);
    if (dm.hasMatch()) {
        QString inner = dm.captured(1).trimmed();
        if (inner.startsWith('@'))
            if (auto it = eff.find(inner.mid(1)); it != eff.end())
                inner = it->second;
        QColor c(inner);
        if (!c.isValid())
            c = QColor(defaultFor(name));
        c.setRedF(c.redF() * 0.75);
        c.setGreenF(c.greenF() * 0.75);
        c.setBlueF(c.blueF() * 0.75);
        return normalizeHex(c);
    }

    // alpha(X, factor)
    const QRegularExpression alphaRe(
        QStringLiteral(R"(alpha\s*\(\s*([^,]+),\s*([0-9.]+)\s*\))"));
    const QRegularExpressionMatch am = alphaRe.match(v);
    if (am.hasMatch()) {
        QString inner = am.captured(1).trimmed();
        const double factor = am.captured(2).toDouble();
        if (inner.startsWith('@'))
            if (auto it = eff.find(inner.mid(1)); it != eff.end())
                inner = it->second;
        QColor c(inner);
        if (!c.isValid())
            c = QColor(defaultFor(name));
        c.setAlphaF(factor);
        return normalizeHex(c);
    }

    // rgb()/rgba() function values: Qt's QColor cannot parse them, but
    // browsers render them natively, so pass them through unchanged.
    if (QRegularExpression(QStringLiteral(R"(^rgba?\s*\()")).match(v).hasMatch())
        return v;

    // Direct color
    const QColor c(v);
    if (c.isValid())
        return normalizeHex(c);
    return defaultFor(name);
}

// Replace every match of `re` in `in` with the result of `fn(matched text)`.
QString replaceMatches(const QString &in,
                       const QRegularExpression &re,
                       const std::function<QString(const QString &)> &fn)
{
    QString out;
    out.reserve(in.size());
    int last = 0;
    for (QRegularExpressionMatchIterator it = re.globalMatch(in); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        out += in.mid(last, m.capturedStart() - last);
        out += fn(m.captured(0));
        last = m.capturedEnd();
    }
    out += in.mid(last);
    return out;
}

// Turn a theme's theme.css into plain web CSS: strip comments and the
// @define-color declarations, then resolve @variable references and
// alpha()/darker() function calls using `eff`. The resulting selectors (.app,
// .input, .item-box, .selected...) are the real theme rules applied to the
// mockup markup.
QString transformCss(const QString &css, const std::map<QString, QString> &eff)
{
    QString out = stripCssComments(css);

    // 1. Drop the @define-color declarations (their values live in `eff`).
    out.remove(QRegularExpression(QStringLiteral("@define-color\\s+[\\w-]+\\s+[^;]+;")));

    // 2. Resolve @variable references.
    out = replaceMatches(
        out,
        QRegularExpression(QStringLiteral("@([\\w-]+)")),
        [&eff](const QString &matched) {
            const QString varName = matched.mid(1);
            const auto found = eff.find(varName);
            if (found == eff.end())
                return matched;
            return resolveColor(varName, found->second, eff, 4);
        });

    // 3. Resolve alpha()/darker() function calls (inner @vars already gone).
    out = replaceMatches(
        out,
        QRegularExpression(QStringLiteral("(?:alpha|darker)\\s*\\([^)]*\\)")),
        [](const QString &matched) {
            return resolveColor(QString(), matched, {}, 4);
        });

    return out;
}

// Resolve an icon name to a file on disk, searching the standard icon
// locations. Returns an empty string when no icon is found.
QString resolveIcon(const QString &name)
{
    if (name.isEmpty())
        return QString();
    if (QFileInfo::exists(name))
        return name;

    static const std::map<QString, QStringList> candidates = {
        {QStringLiteral("spotify"),
         {QStringLiteral("spotify.png"), QStringLiteral("spotify-client.png")}},
        {QStringLiteral("spectacle"), {QStringLiteral("spectacle.svg")}},
        {QStringLiteral("firefox"), {QStringLiteral("firefox.png"), QStringLiteral("firefox.svg")}},
    };
    static const QStringList searchPaths = {
        QStringLiteral("/usr/share/icons/hicolor/128x128/apps"),
        QStringLiteral("/usr/share/icons/breeze/apps/48"),
        QStringLiteral("/usr/share/icons/hicolor/scalable/apps"),
    };

    const auto it = candidates.find(name.toLower());
    if (it == candidates.end())
        return QString();
    for (const QString &dir : searchPaths)
        for (const QString &file : it->second) {
            const QString path = dir + QStringLiteral("/") + file;
            if (QFileInfo::exists(path))
                return path;
        }
    return QString();
}

QString escapeHtml(const QString &text)
{
    QString out = text;
    out.replace(QLatin1String("&"), QStringLiteral("&amp;"));
    out.replace(QLatin1String("<"), QStringLiteral("&lt;"));
    out.replace(QLatin1String(">"), QStringLiteral("&gt;"));
    return out;
}

// Wrap the characters of `name` that match `query` (case-insensitive, in
// order, like Ulauncher's fuzzy match) in a span with class `hlClass`.
QString highlightText(const QString &name, const QString &query, const QString &hlClass)
{
    if (query.isEmpty() || query.length() > name.length())
        return escapeHtml(name);

    QString out;
    out.reserve(name.length() * 4);
    int qi = 0;
    for (const QChar ch : name) {
        const QString esc = escapeHtml(QString(ch));
        if (qi < query.length() && ch.toLower() == query.at(qi).toLower()) {
            out += QStringLiteral("<span class=\"%1\">%2</span>").arg(hlClass, esc);
            ++qi;
        } else {
            out += esc;
        }
    }
    return out;
}

QString iconMarkup(const QString &iconName)
{
    const QString path = resolveIcon(iconName);
    if (!path.isEmpty())
        return QStringLiteral("<img class=\"item-icon\" src=\"%1\">")
            .arg(QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
    return QStringLiteral(
        "<svg class=\"item-icon-svg\" viewBox=\"0 0 24 24\">"
        "<rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"4\"/></svg>");
}

QString rowHtml(const UlauncherRow &row)
{
    const QString hlClass = row.selected ? QStringLiteral("hl-s") : QStringLiteral("hl-n");
    return QStringLiteral(
        "<div class=\"%1item-box\">"
        "<div class=\"item-icon-container\">%2</div>"
        "<div class=\"item-text-container\">"
        "<div class=\"item-name\">%3</div>"
        "</div>"
        "<div class=\"item-shortcut\">%4</div>"
        "</div>")
        .arg(row.selected ? QStringLiteral("selected ") : QString(),
             iconMarkup(row.icon),
             highlightText(row.name, row.highlight, hlClass),
             escapeHtml(row.shortcut));
}

// Sample items shown when no row list is given.
QList<UlauncherRow> sampleRows()
{
    return {
        UlauncherRow(QStringLiteral("spotify"), QStringLiteral("Spotify"),
                     QStringLiteral("sptf"), QStringLiteral("Alt+1"), true),
        UlauncherRow(QStringLiteral("spectacle"), QStringLiteral("Spectacle"),
                     QStringLiteral("sptf"), QStringLiteral("Alt+2"), false),
    };
}

QString gearSvg(const QString &prefsBg, const QString &prefsFg)
{
    return QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"28\" height=\"28\" "
        "shape-rendering=\"geometricPrecision\" fill-rule=\"evenodd\" clip-rule=\"evenodd\" "
        "viewBox=\"0 0 24 24\">"
        "<circle cx=\"12\" cy=\"12\" r=\"12\" fill=\"%1\"/>"
        "<path fill-rule=\"nonzero\" fill=\"%2\" opacity=\"0.95\" "
        "d=\"M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58"
        "c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96"
        "c-.5-.38-1.03-.7-1.62-.94L14.4 2.81c-.04-.24-.24-.41-.48-.41h-3.84"
        "c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96"
        "c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58"
        "c-.05.3-.09.63-.09.94s.02.64.07.94l2.03 1.58c-.18.14-.23.41-.12.61"
        "l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54"
        "c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56"
        "1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61"
        "l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 "
        "3.6 3.6-1.62 3.6-3.6 3.6z\"/></svg>")
        .arg(prefsBg, prefsFg);
}
// The settings gear icon: Ulauncher's own bundled gear (what the real app
// renders), falling back to the system's preferences-system-symbolic. Real
// Ulauncher draws it in a fixed gray (untintable), so the SVG is inlined in
// the document and tinted gray by CSS (.prefs-btn svg path).
QString gearIconPath()
{
    static const QStringList candidates = {
        QStringLiteral("/usr/share/ulauncher/icons/gear.svg"),
        QStringLiteral("/usr/share/icons/breeze/actions/24/preferences-system-symbolic.svg"),
        QStringLiteral("/usr/share/icons/Adwaita/symbolic/categories/preferences-system-symbolic.svg"),
        QStringLiteral("/usr/share/icons/hicolor/scalable/actions/preferences-system-symbolic.svg"),
    };
    for (const QString &candidate : candidates)
        if (QFileInfo::exists(candidate))
            return candidate;
    return QString();
}

QString gearMarkup()
{
    const QString path = gearIconPath();
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QString svg = QString::fromUtf8(file.readAll());
            if (!svg.isEmpty()) {
                // An inline <svg> without a viewBox maps user units 1:1 to its
                // CSS box, so the glyph would not scale up; add a viewBox
                // matching the svg's own width/height to let CSS size it.
                if (!svg.contains(QLatin1String("viewBox"))) {
                    const auto widthMatch =
                        QRegularExpression(QStringLiteral("width=['\"]?(\\d+)")).match(svg);
                    const auto heightMatch =
                        QRegularExpression(QStringLiteral("height=['\"]?(\\d+)")).match(svg);
                    if (widthMatch.hasMatch() && heightMatch.hasMatch())
                        svg.replace(
                            QLatin1String("<svg"),
                            QStringLiteral("<svg viewBox=\"0 0 %1 %2\"")
                                .arg(widthMatch.captured(1), heightMatch.captured(1)));
                }
                return svg;
            }
        }
    }
    // No icon file available: the generated gear as a last resort.
    return gearSvg(QStringLiteral("transparent"), QStringLiteral("#4f4b4b"));
}

} // namespace

QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors,
                             double scale, int windowWidth,
                             const QString &manifestJson,
                             const QString &cssContent,
                             const QList<UlauncherRow> &rows)
{
    // 1. Obtain the theme CSS content.
    QString css = cssContent;
    if (css.isEmpty() && !themePath.isEmpty()) {
        QFile file(QDir(themePath).filePath(QStringLiteral("theme.css")));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            css = QString::fromUtf8(file.readAll());
    }

    // 2. Effective values: parsed @define-color map, then live overrides.
    const ColorMap parsed = extractCssColors(css);
    std::map<QString, QString> eff;
    for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it)
        eff[it.key()] = it.value();
    for (const auto &[key, value] : liveColors)
        eff[key] = value;

    // 3. Manifest highlight colors, when not provided as live overrides.
    if (!manifestJson.isEmpty()) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8(), &error);
        if (!error.error && doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QJsonObject hl =
                obj.value(QStringLiteral("matched_text_hl_colors")).toObject();
            const QStringList hlRows = {QStringLiteral("when_selected"),
                                        QStringLiteral("when_not_selected")};
            for (const QString &hlRow : hlRows) {
                const QString key = QStringLiteral("matched_text_hl_colors_") + hlRow;
                if (eff.contains(key))
                    continue;
                if (obj.contains(hlRow))
                    eff[key] = obj.value(hlRow).toString();
                else if (hl.contains(hlRow))
                    eff[key] = hl.value(hlRow).toString();
            }
        }
    }

    const auto resolve = [&eff](const QString &name) {
        const auto it = eff.find(name);
        return resolveColor(name, it != eff.end() ? it->second : QString(), eff, 4);
    };

    const QString windowBg = resolve("window_bg");
    const QString border = resolve("window_border_color");
    const QString inputColor = resolve("input_color");
    const QString caretColor = resolve("caret_color");
    const QString itemText = resolve("item_text");
    const QString itemName = resolve("item_name");
    const QString selBox = resolve("item_box_selected");
    const QString selName = resolve("item_name_selected");
    const QString shortcutColor = resolve("item_shortcut_color");
    const QString shortcutShadow = resolve("item_shortcut_shadow");
    const QString shortcutSel = resolve("item_shortcut_color_sel");
    const QString shortcutSelShadow = resolve("item_shortcut_shadow_sel");
    const QString hlSel = resolve("matched_text_hl_colors_when_selected");
    const QString hlNoSel = resolve("matched_text_hl_colors_when_not_selected");

    const int width = windowWidth > 0 ? windowWidth : 650;
    const double s = scale > 0.0 ? scale : 0.75;

    // 4. Transform the real theme CSS (comments and @define-color stripped,
    //    variables and functions resolved) so its selectors style the mockup.
    const QString themeCss = transformCss(css, eff);

    // 5. Build the rows and the input query (the selected row's highlight).
    const QList<UlauncherRow> items = rows.isEmpty() ? sampleRows() : rows;
    QString rowsHtml;
    QString query;
    for (const UlauncherRow &row : items) {
        rowsHtml += rowHtml(row);
        if (row.selected)
            query = row.highlight;
    }
    if (query.isEmpty() && !items.isEmpty())
        query = items.first().highlight;

    QString html = QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>\n"
        "html,body{margin:0;padding:0;width:100%;height:100%;background:transparent;"
        "font-family:'Cantarell','Inter','Segoe UI',sans-serif;overflow:hidden;}\n"
        "body{display:flex;justify-content:center;align-items:flex-start;}\n"
        "body::-webkit-scrollbar{display:none;}\n"
        ".app{width:%1px;box-sizing:border-box;background-color:%2;"
        "border:2px solid %3;border-radius:14px;overflow:hidden;position:relative;"
        "padding:4px;}\n"
        ".input{display:flex;align-items:center;justify-content:space-between;"
        "padding:35px 45px;}\n"
        ".input-text{font-size:50px;font-weight:300;color:%4;flex:1;}\n"
        ".caret{display:inline-block;width:3px;height:46px;background:%5;"
        "vertical-align:middle;margin-left:4px;animation:blink 1.1s step-end infinite;}\n"
        "@keyframes blink{50%{opacity:0;}}\n"
        ".prefs-btn{display:flex;align-items:center;justify-content:center;opacity:.8;"
        "border-radius:50%;min-width:28px;min-height:28px;}\n"
        ".prefs-btn svg{width:40px;height:40px;display:block;}\n"
        ".prefs-btn svg path{fill:#4f4b4b;}\n"
        ".results{padding-bottom:25px;}\n"
        ".item-box{display:flex;align-items:center;padding:22px 30px;margin:0 16px;"
        "border-radius:10px;}\n"
        ".item-icon-container{width:70px;display:flex;justify-content:center;"
        "align-items:center;flex:0 0 auto;}\n"
        ".item-icon{width:48px;height:48px;object-fit:contain;}\n"
        ".item-icon-svg{width:48px;height:48px;fill:%6;}\n"
        ".item-text-container{display:flex;flex-direction:column;justify-content:center;"
        "padding-left:25px;flex:1;min-width:0;}\n"
        ".item-name{font-size:26px;font-weight:500;color:%7;white-space:nowrap;"
        "overflow:hidden;text-overflow:ellipsis;}\n"
        ".item-shortcut{margin-left:auto;padding-left:25px;font-size:20px;color:%8;"
        "text-shadow:1px 1px 1px %9;white-space:nowrap;}\n"
        ".selected.item-box{background-color:%10;}\n"
        ".selected.item-box .item-name{color:%11;}\n"
        ".selected.item-box .item-shortcut{color:%12;text-shadow:1px 1px 1px %13;}\n"
        ".hl-s{color:%14;}.hl-n{color:%15;}\n"
        "%16\n"
        ".scaler{padding:50px;transform:scale(%17);transform-origin:top center;"
        "display:inline-block;}\n"
        ".app{box-shadow:0 25px 50px rgba(0,0,0,0.8);}\n"
        "</style></head><body><div class=\"scaler\"><div class=\"app\">\n"
        "<div class=\"input\">"
        "<div class=\"input-text\">%18<span class=\"caret\"></span></div>"
        "<div class=\"prefs-btn\">%19</div>"
        "</div>\n"
        "<div class=\"results\">\n%20\n</div>\n"
        "</div></div></body></html>")
        .arg(width)
        .arg(windowBg)
        .arg(border)
        .arg(inputColor)
        .arg(caretColor)
        .arg(itemText)
        .arg(itemName)
        .arg(shortcutColor)
        .arg(shortcutShadow)
        .arg(selBox)
        .arg(selName)
        .arg(shortcutSel)
        .arg(shortcutSelShadow)
        .arg(hlSel)
        .arg(hlNoSel)
        .arg(themeCss)
        .arg(s)
        .arg(escapeHtml(query))
        .arg(gearMarkup())
        .arg(rowsHtml);

    return html;
}

} // namespace tools
