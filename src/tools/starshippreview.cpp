#include "tools/starshippreview.h"

#include <toml++/toml.hpp>

#include <QHash>
#include <QMap>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QTime>

#include <map>
#include <string>
#include <vector>

namespace tools
{

namespace
{

// ---------------------------------------------------------------------------
// Constants (port of the original app's starship_preview.py)
// ---------------------------------------------------------------------------

// Fallback ANSI colors (Gruvbox-style) for the preview.
const QMap<QString, QString> STANDARD_COLORS = {
    {"black", "#000000"},
    {"red", "#cc241d"},
    {"green", "#98971a"},
    {"yellow", "#d79921"},
    {"blue", "#458588"},
    {"purple", "#b16286"},
    {"cyan", "#689d6a"},
    {"white", "#a89984"},
    {"orange", "#fe8019"},
    {"gray", "#928374"},
    {"grey", "#928374"},
};

// Font family of the rendered terminal.
const char *const CSS_FONT_FAMILY = "'CaskaydiaCove Nerd Font', monospace";

// Nerd Font powerline connectors, built from code points so no editor or
// formatter can drop the private-use-area characters.
const QSet<QChar> POWERLINE_GLYPHS = {
    QChar(0xE0B0), QChar(0xE0B2), QChar(0xE0B4), QChar(0xE0B6), QChar(0xE0BC), QChar(0xE0BE),
};

// Triangle connectors get the `glyph2` CSS class for independent styling.
const QSet<QChar> TRIANGLE_CONNECTORS = {QChar(0xE0B0), QChar(0xE0B2)};

// The git status symbol shown in the demo context.
const QChar GIT_STATUS_SYMBOL = QChar(0xF42E);

// Modules that may be expanded when encountered as $module_name in a format string.
const QSet<QString> WHITELIST_MODULES = {
    "os", "username", "directory", "python", "time", "git_branch", "git_status", "character",
};

// Default format templates per module when the format key is absent.
const QMap<QString, QString> DEFAULT_MODULE_FORMATS = {
    {"os", "[$symbol]($style)"},
    {"character", "$symbol"},
};
const QString DEFAULT_FORMAT_FALLBACK = "$symbol$version";

// Python version the original app ran on; kept for the venv cleanup patterns.
const QString PYTHON_VERSION = "v3.14";

// ---------------------------------------------------------------------------
// Regexes (port of the original app)
// ---------------------------------------------------------------------------

const QRegularExpression RE_UNRESOLVED_VAR("\\$[a-zA-Z_0-9]+");
const QRegularExpression RE_UNRESOLVED_VAR_BRACE("\\$\\{[a-zA-Z_0-9]+\\}");
const QRegularExpression RE_EMPTY_GROUP("(?<!\\])\\(\\s*\\)");
const QRegularExpression RE_EMPTY_GROUP_SPACES("(?<!\\])\\(\\s+\\)");
const QRegularExpression RE_INTERNAL_GROUP("(?<!\\])\\(([^()]*)\\)");
const QRegularExpression RE_VAR_IN_GROUP("\\$[a-zA-Z_0-9]+|\\$\\{[a-zA-Z_0-9]+\\}");
const QRegularExpression RE_DOUBLE_SPACES("\\s{2,}");
const QRegularExpression RE_FG_COLOR("fg:([a-zA-Z0-9_#]+)");
const QRegularExpression RE_BG_COLOR("bg:([a-zA-Z0-9_#]+)");
const QRegularExpression RE_VAR_NAME("\\$([a-zA-Z0-9_]+)");

// ---------------------------------------------------------------------------
// Color resolution
// ---------------------------------------------------------------------------

QString formatCssColor(const QString &value)
{
    const QString v = value.trimmed();
    return v.isEmpty() ? QStringLiteral("inherit") : v;
}

QString resolveColor(const QString &key, const std::map<QString, QString> &palette)
{
    if (key.isEmpty())
        return QStringLiteral("inherit");

    QString cleanKey = key;
    cleanKey.remove(QStringLiteral("fg:"));
    cleanKey.remove(QStringLiteral("bg:"));
    cleanKey = cleanKey.trimmed().toLower();

    const auto it = palette.find(cleanKey);
    if (it != palette.end())
        return formatCssColor(it->second);
    const auto stdIt = STANDARD_COLORS.constFind(cleanKey);
    if (stdIt != STANDARD_COLORS.constEnd())
        return formatCssColor(stdIt.value());
    return formatCssColor(cleanKey);
}

QStringList buildElementCss(const QString &style, const std::map<QString, QString> &palette)
{
    QStringList css;
    if (style.toLower().contains(QStringLiteral("bold")))
        css.append(QStringLiteral("font-weight: bold"));

    const QRegularExpressionMatch bgMatch = RE_BG_COLOR.match(style);
    const QRegularExpressionMatch fgMatch = RE_FG_COLOR.match(style);

    if (bgMatch.hasMatch())
        css.append(QStringLiteral("background-color: %1")
            .arg(resolveColor(bgMatch.captured(1), palette)));
    if (fgMatch.hasMatch())
        css.append(QStringLiteral("color: %1").arg(resolveColor(fgMatch.captured(1), palette)));
    else if (!style.isEmpty() && !style.contains(QStringLiteral("bg:"))) {
        QString bareColor = style;
        bareColor.remove(QStringLiteral("bold"));
        bareColor = bareColor.trimmed();
        if (!bareColor.isEmpty())
            css.append(QStringLiteral("color: %1").arg(resolveColor(bareColor, palette)));
    }
    return css;
}

// ---------------------------------------------------------------------------
// Prompt document model and parsing
// ---------------------------------------------------------------------------

struct Element
{
    enum Type { Text, LineBreak } type = Text;
    QString style;
    QString content; // Text: already cleaned; LineBreak: unused
};

struct Segment
{
    enum Type { Block, LineBreak, Variable } type;
    QString content; // Block: inner text
    QString name;    // Block: style; Variable: module name
    int start = 0;
    int end = 0;
};

struct Block
{
    bool valid = false;
    QString content;
    QString style;
    int end = 0; // index just past the closing bracket
};

// Matches [content] or [content](style) starting at text[i] == '['.
// Returns the parsed block and the index just past the closing bracket.
Block findBlock(const QString &text, int i)
{
    Block block;
    int depth = 0;
    int j = i;

    for (; j < text.length(); ++j) {
        const QChar c = text.at(j);
        if (c == QChar('['))
            ++depth;
        else if (c == QChar(']')) {
            --depth;
            if (depth == 0)
                break;
        }
    }
    if (depth != 0 || j >= text.length())
        return block;

    const QString content = text.mid(i + 1, j - i - 1);
    int styleEnd = j + 1;

    // Optional (style) part after the closing bracket.
    if (j + 1 < text.length() && text.at(j + 1) == QChar('(')) {
        int k = j + 1;
        while (k < text.length() && text.at(k) != QChar(')'))
            ++k;
        if (k < text.length()) {
            block.style = text.mid(j + 2, k - j - 2);
            styleEnd = k + 1;
        }
    }

    block.valid = true;
    block.content = content;
    block.end = styleEnd;
    return block;
}

// The $name starting at text[i] (caller must check text[i] == '$').
bool findVariable(const QString &text, int i, QString &name, int &end)
{
    const QRegularExpressionMatch m = RE_VAR_NAME.match(text, i);
    if (!m.hasMatch() || m.capturedStart() != i)
        return false;
    name = m.captured(1);
    end = m.capturedEnd();
    return true;
}

// Splits a format string into [blocks], $line_break and $variables.
std::vector<Segment> getSegments(const QString &text)
{
    std::vector<Segment> segments;
    int i = 0;
    while (i < text.length()) {
        if (text.at(i) == QChar('[')) {
            const Block blk = findBlock(text, i);
            if (blk.valid) {
                Segment seg;
                seg.type = Segment::Block;
                seg.content = blk.content;
                seg.name = blk.style;
                seg.start = i;
                seg.end = blk.end;
                segments.push_back(seg);
                i = blk.end;
                continue;
            }
        }
        if (text.mid(i).startsWith(QStringLiteral("$line_break"))) {
            Segment seg;
            seg.type = Segment::LineBreak;
            seg.start = i;
            seg.end = i + 11;
            segments.push_back(seg);
            i += 11;
            continue;
        }
        if (text.at(i) == QChar('$')) {
            QString name;
            int end = -1;
            if (findVariable(text, i, name, end)) {
                Segment seg;
                seg.type = Segment::Variable;
                seg.name = name;
                seg.start = i;
                seg.end = end;
                segments.push_back(seg);
                i = end;
                continue;
            }
        }
        ++i;
    }
    return segments;
}

// ---------------------------------------------------------------------------
// Content cleaning
// ---------------------------------------------------------------------------

QString removeUnresolvedVars(const QString &text)
{
    QString out = text;
    out.replace(RE_UNRESOLVED_VAR, QString());
    out.replace(RE_UNRESOLVED_VAR_BRACE, QString());
    return out;
}

QString unescapeStarship(const QString &text)
{
    QString out = text;
    out.replace(QStringLiteral("\\("), QStringLiteral("("));
    out.replace(QStringLiteral("\\)"), QStringLiteral(")"));
    out.replace(QStringLiteral("\\["), QStringLiteral("["));
    out.replace(QStringLiteral("\\]"), QStringLiteral("]"));
    out.replace(QStringLiteral("\\ "), QStringLiteral(" "));
    return out;
}

// Removes empty optional groups and the venv/git-status artifacts that result
// from them (port of _strip_empty_optional_groups).
QString stripEmptyOptionalGroups(const QString &text)
{
    QString out = text;
    for (int i = 0; i < 4; ++i) {
        out.replace(RE_EMPTY_GROUP, QString());
        out.replace(RE_EMPTY_GROUP_SPACES, QString());
        out.replace(QStringLiteral("(("), QStringLiteral("("));
        out.replace(QStringLiteral("))"), QStringLiteral(")"));
        out.replace(QStringLiteral("()"), QString());
        out.replace(QStringLiteral("( )"), QString());
        out.replace(QStringLiteral("(") + GIT_STATUS_SYMBOL + QStringLiteral(" )"),
                    QStringLiteral(" ") + GIT_STATUS_SYMBOL);
        out.replace(QStringLiteral("( ") + PYTHON_VERSION,
                    PYTHON_VERSION + QStringLiteral(" (.venv)"));
        out.replace(PYTHON_VERSION + QStringLiteral(" )"),
                    QStringLiteral(" ") + PYTHON_VERSION + QStringLiteral(" (.venv)"));
    }
    return out;
}

QString cleanFinalContent(const QString &text)
{
    if (text.isEmpty())
        return {};
    QString out = removeUnresolvedVars(text);
    out = unescapeStarship(out);
    out = stripEmptyOptionalGroups(out);
    out.replace(RE_DOUBLE_SPACES, QStringLiteral(" "));
    return out.trimmed();
}

// True when a text run is insignificant and should produce no element.
bool isInsufficientText(const QString &content)
{
    return content.trimmed().isEmpty() && content != QStringLiteral(" ");
}

// ---------------------------------------------------------------------------
// Module format resolution
// ---------------------------------------------------------------------------

// Substitutes $name / ${name} tokens in one pass (port of substituteVariables).
QString substituteVariables(const QString &text, const QHash<QString, QString> &values)
{
    auto isNameChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QChar('_');
    };

    QString out;
    out.reserve(text.length());
    int i = 0;
    while (i < text.length()) {
        if (text.at(i) == QChar('$')) {
            QString name;
            int end = -1;
            if (i + 1 < text.length() && text.at(i + 1) == QChar('{')) {
                const int close = text.indexOf(QChar('}'), i + 2);
                if (close > i) {
                    name = text.mid(i + 2, close - i - 2);
                    end = close + 1;
                }
            } else {
                int j = i + 1;
                while (j < text.length() && isNameChar(text.at(j)))
                    ++j;
                if (j > i + 1) {
                    name = text.mid(i + 1, j - i - 1);
                    end = j;
                }
            }
            if (end > 0) {
                out += values.value(name);
                i = end;
                continue;
            }
        }
        out += text.at(i);
        ++i;
    }
    return out;
}

// Collapses optional groups whose content is only variables (port of
// _evaluate_optional_groups): a group with no static text disappears.
QString evaluateOptionalGroups(const QString &text)
{
    QString out = text;
    bool changed = true;
    while (changed) {
        changed = false;
        const QRegularExpressionMatch m = RE_INTERNAL_GROUP.match(out);
        if (!m.hasMatch())
            break;

        QString inner = m.captured(1);
        const QString check = inner.replace(RE_VAR_IN_GROUP, QString())
                                    .replace(QStringLiteral("\\("), QString())
                                    .replace(QStringLiteral("\\)"), QString())
                                    .replace(QStringLiteral("\\["), QString())
                                    .replace(QStringLiteral("\\]"), QString())
                                    .trimmed();
        if (check.isEmpty())
            out.remove(m.capturedStart(), m.capturedLength());
        else
            out.replace(m.capturedStart(), m.capturedLength(),
                        QString(QChar(0x01)) + inner + QString(QChar(0x02)));
        changed = true;
    }
    out.replace(QChar(0x01), QChar('('));
    out.replace(QChar(0x02), QChar(')'));
    return out;
}

// ---------------------------------------------------------------------------
// Runtime context
// ---------------------------------------------------------------------------

// Build the runtime context for the preview (port of _build_context): a fixed
// demo environment plus user overrides.
QMap<QString, QString> buildContext(const std::map<QString, QString> &overrides)
{
    QMap<QString, QString> context;
    context[QStringLiteral("os")] = QStringLiteral("Arch");

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString username = env.value(QStringLiteral("USER"));
    if (username.isEmpty())
        username = env.value(QStringLiteral("LOGNAME"));
    context[QStringLiteral("username")] = username;

    context[QStringLiteral("directory")] = QStringLiteral("~/dev/scripts");
    context[QStringLiteral("python")] = QStringLiteral("v3.14");
    context[QStringLiteral("time")] = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    context[QStringLiteral("git_branch")] = QStringLiteral("main");
    context[QStringLiteral("git_status")].append(GIT_STATUS_SYMBOL);

    for (const auto &[key, value] : overrides)
        context.insert(key, value);
    return context;
}

// ---------------------------------------------------------------------------
// Prompt renderer
// ---------------------------------------------------------------------------

// Helpers defined below, used by PromptRenderer::toHtml().
QString classifyContentClass(const QString &content);
QString buildHtmlWrapper(const QString &innerHtml, double scale, int width);
class PromptRenderer
{
public:
    PromptRenderer(const toml::table &doc, const QMap<QString, QString> &context,
                   const std::map<QString, QString> &palette, double scale, int width)
        : m_doc(doc), m_context(context), m_palette(palette), m_scale(scale), m_width(width)
    {
    }

    QString render() const
    {
        QString format;
        if (const auto node = m_doc.get("format"); node && node->is_string())
            format = QString::fromStdString(node->as_string()->get());
        format.replace(QStringLiteral("\\\n"), QString());
        format.remove(QChar('\n'));

        std::vector<Element> elements;
        processTextSegment(format, QString(), elements);
        return toHtml(elements);
    }

private:
    void processTextSegment(const QString &text, const QString &defaultStyle,
                            std::vector<Element> &elements) const
    {
        const std::vector<Segment> segments = getSegments(text);
        if (segments.empty()) {
            if (!isInsufficientText(text))
                elements.push_back({Element::Text, defaultStyle, cleanFinalContent(text)});
            return;
        }

        int lastIdx = 0;
        for (const Segment &seg : segments) {
            const QString pre = text.mid(lastIdx, seg.start - lastIdx);
            if (!isInsufficientText(pre))
                elements.push_back({Element::Text, defaultStyle, cleanFinalContent(pre)});

            if (seg.type == Segment::Block) {
                const QString innerStyle =
                    (seg.name == QStringLiteral("$style") || seg.name == QStringLiteral("${style}"))
                        ? defaultStyle
                        : seg.name;
                processTextSegment(seg.content, innerStyle, elements);
            } else if (seg.type == Segment::LineBreak) {
                elements.push_back({Element::LineBreak, QString(), QString()});
            } else if (seg.type == Segment::Variable) {
                if (WHITELIST_MODULES.contains(seg.name)) {
                    const auto node = m_doc.get(seg.name.toStdString());
                    if (node && node->is_table()) {
                        const auto &cfg = *node->as_table();
                        bool disabled = false;
                        if (cfg.contains("disabled") && cfg["disabled"].is_boolean())
                            disabled = cfg["disabled"].as_boolean()->get();
                        if (!disabled) {
                            const QString expanded = solveModuleFormat(seg.name, cfg);
                            QString moduleStyle;
                            if (cfg.contains("style") && cfg["style"].is_string())
                                moduleStyle = QString::fromStdString(cfg["style"].as_string()->get());
                            processTextSegment(expanded, moduleStyle, elements);
                        }
                    }
                }
            }
            lastIdx = seg.end;
        }

        const QString post = text.mid(lastIdx);
        if (!isInsufficientText(post))
            elements.push_back({Element::Text, defaultStyle, cleanFinalContent(post)});
    }

    QString solveModuleFormat(const QString &name, const toml::table &cfg) const
    {
        QString fmt;
        if (cfg.contains("format") && cfg["format"].is_string())
            fmt = QString::fromStdString(cfg["format"].as_string()->get());
        else
            fmt = DEFAULT_MODULE_FORMATS.value(name, DEFAULT_FORMAT_FALLBACK);

        const QHash<QString, QString> subs = buildModuleSubstitutions(name, cfg);

        QString result = substituteVariables(fmt, subs);
        result = evaluateOptionalGroups(result);
        result = substituteVariables(result, subs);
        result.replace(RE_DOUBLE_SPACES, QStringLiteral(" "));
        if (name == QStringLiteral("os"))
            result = result.trimmed();
        return result;
    }

    QHash<QString, QString> buildModuleSubstitutions(const QString &name,
                                                     const toml::table &cfg) const
    {
        // Base: every scalar key of the module configuration.
        QHash<QString, QString> subs;
        for (const auto &[key, val] : cfg) {
            const QString k = QString::fromStdString(std::string(key));
            if (val.is_string())
                subs.insert(k, QString::fromStdString(val.as_string()->get()));
            else if (val.is_boolean())
                subs.insert(k, val.as_boolean()->get() ? QStringLiteral("True")
                                                       : QStringLiteral("False"));
            else if (val.is_integer())
                subs.insert(k, QString::number(val.as_integer()->get()));
            else if (val.is_floating_point())
                subs.insert(k, QString::number(val.as_floating_point()->get()));
        }

        // Fallback for a missing style key (username uses style_user).
        if (!subs.contains(QStringLiteral("style"))) {
            QString style;
            if (name == QStringLiteral("username") && cfg.contains("style_user")
                && cfg["style_user"].is_string())
                style = QString::fromStdString(cfg["style_user"].as_string()->get());
            subs.insert(QStringLiteral("style"), style);
        }

        // Runtime values derived from the demo context; these override any
        // scalars of the same name.
        QString version = m_context.value(name);
        if (name == QStringLiteral("os"))
            version.clear(); // do not leak the OS name as a version

        subs.insert(QStringLiteral("path"), m_context.value(QStringLiteral("directory")));
        subs.insert(QStringLiteral("user"), m_context.value(QStringLiteral("username")));
        subs.insert(QStringLiteral("username"), m_context.value(QStringLiteral("username")));
        subs.insert(QStringLiteral("time"), m_context.value(QStringLiteral("time")));
        subs.insert(QStringLiteral("version"), version);
        subs.insert(QStringLiteral("branch"), m_context.value(QStringLiteral("git_branch")));
        subs.insert(QStringLiteral("all_status"), m_context.value(QStringLiteral("git_status")));
        subs.insert(QStringLiteral("ahead_behind"), QString());
        subs.insert(QStringLiteral("virtualenv"), QString());

        // Module-specific symbol resolution.
        if (name == QStringLiteral("character")) {
            QString symbol = QStringLiteral("❯");
            if (cfg.contains("success_symbol") && cfg["success_symbol"].is_string())
                symbol = QString::fromStdString(cfg["success_symbol"].as_string()->get());
            else if (cfg.contains("symbol") && cfg["symbol"].is_string())
                symbol = QString::fromStdString(cfg["symbol"].as_string()->get());
            subs.insert(QStringLiteral("symbol"), symbol);
        }
        if (name == QStringLiteral("os")) {
            const QString osVal = m_context.value(QStringLiteral("os"), QStringLiteral("Arch"));
            if (cfg.contains("symbols") && cfg["symbols"].is_table()) {
                const auto &symbols = *cfg["symbols"].as_table();
                const auto it = symbols.find(osVal.toStdString());
                if (it != symbols.end() && it->second.is_string())
                    subs.insert(QStringLiteral("symbol"),
                                QString::fromStdString(it->second.as_string()->get()));
            }
            subs.insert(QStringLiteral("os"), QString()); // hide the literal OS name
        }

        return subs;
    }

    QString toHtml(const std::vector<Element> &elements) const
    {
        QString inner;
        for (const Element &el : elements) {
            if (el.type == Element::LineBreak) {
                inner += QStringLiteral("</div><div class=\"prompt-line\">");
                continue;
            }
            if (el.content.isEmpty())
                continue;

            const QStringList css = buildElementCss(el.style, m_palette);
            QString escaped = el.content;
            escaped.replace(QStringLiteral(" "), QStringLiteral("&nbsp;"));
            inner += QStringLiteral("<span class=\"%1\" style=\"%2\">%3</span>")
                .arg(classifyContentClass(el.content),
                     css.join(QStringLiteral("; ")), escaped);
        }
        return buildHtmlWrapper(inner, m_scale, m_width);
    }

    const toml::table &m_doc;
    const QMap<QString, QString> &m_context;
    const std::map<QString, QString> &m_palette;
    double m_scale = 1.0;
    int m_width = 800;
};

QString classifyContentClass(const QString &content)
{
    bool hasGlyph = false;
    bool hasTriangle = false;
    for (const QChar &ch : content) {
        if (POWERLINE_GLYPHS.contains(ch)) {
            hasGlyph = true;
            if (TRIANGLE_CONNECTORS.contains(ch))
                hasTriangle = true;
        }
    }
    if (!hasGlyph)
        return QStringLiteral("text-node");
    return hasTriangle ? QStringLiteral("glyph glyph2") : QStringLiteral("glyph");
}

// ---------------------------------------------------------------------------
// HTML wrapper
// ---------------------------------------------------------------------------

QString buildPreviewCss(double scale, int width)
{
    return QStringLiteral(
        "<style>"
        "body {"
        "background: transparent;"
        "margin: 0;"
        "padding: 0;"
        "display: flex;"
        "justify-content: center;"
        "align-items: flex-start;"
        "width: 100vw;"
        "overflow: hidden;"
        "font-family: %3;"
        "}"
        "body::-webkit-scrollbar { display: none; }"
        ".scaler {"
        "padding: 40px;"
        "transform: scale(%1);"
        "transform-origin: top center;"
        "display: inline-block;"
        "}"
        ".terminal {"
        "display: inline-block;"
        "width: %2px;"
        "background: rgba(24,24,24,0.6);"
        "border: 1px solid rgba(48,48,48,0.6);"
        "border-radius: 8px;"
        "padding: 15px 20px;"
        "box-shadow: 0 20px 40px rgba(0,0,0,0.8);"
        "white-space: nowrap;"
        "overflow: hidden;"
        "}"
        ".prompt-line {"
        "display: flex; align-items: center;"
        "height: 32px; line-height: 32px;"
        "}"
        "span { display: inline-flex; align-items: center; justify-content: center; height: 100%; }"
        ".text-node {"
        "padding: 0 4px; font-size: 14px;"
        "white-space: pre; line-height: 1;"
        "}"
        ".glyph:not(.glyph2) {"
        "display: inline-flex; align-items: center; justify-content: center;"
        "font-size: 27px; margin: 0px -0.5px 0 -0.5px; line-height: 1;"
        "}"
        ".glyph2 {"
        "display: inline-flex; align-items: center; justify-content: center;"
        "font-size: 27px; margin: 0 -0.5px; line-height: 1;"
        "}"
        "</style>")
        .arg(scale, 0, 'g', 15)
        .arg(width)
        .arg(CSS_FONT_FAMILY);
}

QString buildHtmlWrapper(const QString &innerHtml, double scale, int width)
{
    return QStringLiteral("<html><head>")
        + buildPreviewCss(scale, width)
        + QStringLiteral("</head><body><div class=\"scaler\"><div class=\"terminal\">"
                        "<div class=\"prompt-line\">")
        + innerHtml
        + QStringLiteral("</div></div></div></body></html>");
}

// Resolves the palette to use for the preview: the palette table at the given
// index, or the document's active `palette` when the index is negative.
std::map<QString, QString> resolveActivePalette(const toml::table &doc, int paletteIndex)
{
    std::map<QString, QString> palette;
    const auto palettesNode = doc.get("palettes");
    if (!palettesNode || !palettesNode->is_table())
        return palette;
    const auto &palettesTable = *palettesNode->as_table();

    QString paletteName;
    if (paletteIndex >= 0) {
        int idx = 0;
        for (const auto &[pname, pval] : palettesTable) {
            if (idx == paletteIndex) {
                paletteName = QString::fromStdString(std::string(pname));
                break;
            }
            ++idx;
        }
    } else if (const auto paletteKey = doc.get("palette"); paletteKey && paletteKey->is_string()) {
        paletteName = QString::fromStdString(paletteKey->as_string()->get());
    }

    const auto it = palettesTable.find(paletteName.toStdString());
    if (it != palettesTable.end() && it->second.is_table()) {
        for (const auto &[ckey, cval] : *it->second.as_table())
            if (cval.is_string())
                palette[QString::fromStdString(std::string(ckey))] =
                    QString::fromStdString(cval.as_string()->get());
    }
    return palette;
}

} // namespace

QString starshipPreviewHtml(const toml::table &doc, int paletteIndex, double scale, int width,
                            const std::map<QString, QString> &overrides)
{
    const QMap<QString, QString> context = buildContext(overrides);
    const std::map<QString, QString> palette = resolveActivePalette(doc, paletteIndex);
    PromptRenderer renderer(doc, context, palette, scale, width);
    return renderer.render();
}

} // namespace tools
