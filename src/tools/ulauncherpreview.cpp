#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

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
    const QRegularExpression darkRe(QStringLiteral(R"(darker\(\s*([^)]+)\s*\))"));
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
        QStringLiteral(R"(alpha\(\s*([^,]+),\s*([0-9.]+)\s*\))"));
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

    // Direct color
    const QColor c(v);
    if (c.isValid())
        return normalizeHex(c);
    return defaultFor(name);
}

const char *const kGenericIcon =
    R"(<svg class="ico" viewBox="0 0 24 24"><rect x="3" y="3" width="18" height="18" rx="4"/></svg>)";

QString appRow(const QString &name, const QString &desc, bool selected)
{
    return QStringLiteral("<div class=\"%1row\">%2<span class=\"name\">%3</span>"
                          "<span class=\"desc\">%4</span></div>")
        .arg(selected ? QStringLiteral("sel ") : QString())
        .arg(QString::fromLatin1(kGenericIcon))
        .arg(name, desc);
}

} // namespace

QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors,
                             double scale, int windowWidth,
                             const QString &manifestJson,
                             const QString &cssContent)
{
    Q_UNUSED(manifestJson);

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

    const QString hlKey = QStringLiteral("matched_text_hl_colors_when_not_selected");

    // 3. Resolve the canonical colors used by the mockup.
    const auto resolve = [&eff](const QString &name) {
        const auto it = eff.find(name);
        return resolveColor(name, it != eff.end() ? it->second : QString(), eff, 4);
    };

    const QString windowBg = resolve("window_bg");
    const QString border = resolve("window_border_color");
    const QString inputColor = resolve("input_color");
    const QString itemName = resolve("item_name");
    const QString itemText = resolve("item_text");
    const QString selBox = resolve("item_box_selected");
    const QString selName = resolve("item_name_selected");
    const QString selText = resolve("item_text_selected");
    const QString hl = resolve(hlKey);

    const int width = windowWidth > 0 ? windowWidth : 520;
    const double s = scale > 0.0 ? scale : 0.75;

    // 4. Build the self-contained launcher mockup.
    QString rows;
    rows += appRow("Firefox", "Web browser", false);
    rows += appRow("Files", "Manage your files", true);
    rows += appRow("Terminal", "Run shell commands", false);
    rows += appRow("Settings", "System preferences", false);

    QString html = QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>\n"
        "html,body{margin:0;height:100%;background:#1b1b1b;"
        "font-family:'Inter','Segoe UI',sans-serif;"
        "display:flex;justify-content:center;overflow:hidden;}\n"
        "body{padding-top:24px;box-sizing:border-box;}\n"
        ".win{width:%1px;box-sizing:border-box;background:%2;"
        "border:2px solid %3;border-radius:14px;padding:8px;"
        "transform:scale(%4);transform-origin:top center;}\n"
        ".field{display:flex;align-items:center;gap:10px;padding:10px;"
        "color:%5;font-size:22px;}\n"
        ".field .ico{width:24px;height:24px;fill:%5;opacity:.7;flex:0 0 auto;}\n"
        ".field .txt{flex:1;}\n"
        ".caret{display:inline-block;width:2px;height:22px;background:%5;"
        "vertical-align:middle;margin-left:2px;animation:blink 1.1s step-end infinite;}\n"
        "@keyframes blink{50%{opacity:0;}}\n"
        ".list{margin-top:6px;}\n"
        ".row{display:flex;align-items:center;gap:12px;padding:9px 12px;border-radius:10px;}\n"
        ".row .ico{width:26px;height:26px;fill:%6;flex:0 0 auto;}\n"
        ".row .name{color:%7;font-size:15px;font-weight:600;white-space:nowrap;}\n"
        ".row .desc{color:%6;font-size:13px;flex:1;overflow:hidden;"
        "text-overflow:ellipsis;white-space:nowrap;}\n"
        ".row.sel{background:%8;}\n"
        ".row.sel .ico{fill:%9;}\n"
        ".row.sel .name{color:%10;}\n"
        ".row.sel .desc{color:%9;}\n"
        ".hl{color:%11;font-weight:700;}\n"
        "</style></head><body><div class=\"win\">\n"
        "<div class=\"field\">"
        "<svg class=\"ico\" viewBox=\"0 0 24 24\">"
        "<path d=\"M10 2a8 8 0 1 0 4.9 14.3l5.4 5.4 1.4-1.4-5.4-5.4A8 8 0 0 0 10 2zm0 2a6 6 0 1 1 0 12 6 6 0 0 1 0-12z\"/></svg>"
        "<span class=\"txt\">fire<span class=\"hl\">fox</span><span class=\"caret\"></span></span>"
        "</div>\n"
        "<div class=\"list\">\n%12\n</div>\n"
        "</div></body></html>")
        .arg(width)
        .arg(windowBg)
        .arg(border)
        .arg(s)
        .arg(inputColor)
        .arg(itemText)
        .arg(itemName)
        .arg(selBox)
        .arg(selName)
        .arg(selText)
        .arg(hl)
        .arg(rows);

    return html;
}

} // namespace tools
