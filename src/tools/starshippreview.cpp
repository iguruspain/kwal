#include "tools/starshippreview.h"

#include <toml++/toml.hpp>

#include <QMap>
#include <QString>
#include <QRegularExpression>
#include <QDateTime>
#include <QProcessEnvironment>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace tools
{

namespace
{

// CSS constants matching the Python implementation
constexpr const char *CSS_FONT_FAMILY = "'CaskaydiaCove Nerd Font', monospace";
constexpr int CSS_SHADOW_BLUR = 40;
constexpr int CSS_SHADOW_Y = 20;
constexpr const char *CSS_SHADOW_COLOR = "rgba(0,0,0,0.8)";
constexpr const char *CSS_TERMINAL_BACKGROUND = "#181818";
constexpr const char *CSS_TERMINAL_BORDER = "#303030";
constexpr int CSS_TERMINAL_BORDER_RADIUS = 8;
constexpr int CSS_TERMINAL_PADDING_Y = 15;
constexpr int CSS_TERMINAL_PADDING_X = 20;
constexpr int CSS_PROMPT_LINE_HEIGHT = 32;
constexpr int CSS_TEXT_NODE_FONT_SIZE = 14;
constexpr const char *CSS_GLYPH_FONT_SIZE = "27px";

// Powerline glyphs
constexpr const char *POWERLINE_GLYPHS = "";
constexpr const char *TRIANGLE_CONNECTORS = "";

// Fallback ANSI colors (Gruvbox-style) for the preview
const QMap<QString, QString> STANDARD_COLORS = {
    {"black", "#282828"}, {"red", "#cc241d"}, {"green", "#98971a"}, {"yellow", "#d79921"},
    {"blue", "#458588"}, {"magenta", "#b16286"}, {"cyan", "#689d6a"}, {"white", "#a89984"},
    {"bright_black", "#928374"}, {"bright_red", "#fb4934"}, {"bright_green", "#b8bb26"},
    {"bright_yellow", "#fabd2f"}, {"bright_blue", "#83a598"}, {"bright_magenta", "#d3869b"},
    {"bright_cyan", "#8ec07c"}, {"bright_white", "#ebdbbb"},
    {"color_fg0", "#fbf1c7"}, {"color_bg1", "#3c3836"}, {"color_bg3", "#665c54"},
    {"color_blue", "#458588"}, {"color_aqua", "#689d6a"}, {"color_green", "#98971a"},
    {"color_orange", "#d65d0e"}, {"color_purple", "#b16286"}, {"color_red", "#cc241d"},
    {"color_yellow", "#d79921"}
};

// Modules that may be expanded when encountered as $module_name in a format string
const QSet<QString> WHITELIST_MODULES = {
    "os", "username", "directory", "git_branch", "git_status", "git_commit",
    "nodejs", "python", "rust", "golang", "c", "cpp", "php", "java", "kotlin",
    "haskell", "docker_context", "conda", "pixi", "time", "cmd_duration",
    "line_break", "character", "memory_usage", "cpu", "battery"
};

// Default format templates per module when `format` key is absent
const QMap<QString, QString> DEFAULT_MODULE_FORMATS = {
    {"os", "$symbol"},
    {"username", "[$user]($style)"},
    {"directory", "[$path]($style)"},
    {"git_branch", "[[$symbol $branch](fg:black)]($style)"},
    {"git_status", "[($all_status$ahead_behind)](fg:black bg:green)"},
    {"nodejs", "[[$symbol( $version)](fg:black)]($style)"},
    {"python", "[[$symbol( $version)( ($virtualenv))](fg:black)]($style)"},
    {"rust", "[[$symbol( $version)](fg:black)]($style)"},
    {"golang", "[[$symbol( $version)](fg:black)]($style)"},
    {"c", "[[$symbol( $version)](fg:black)]($style)"},
    {"cpp", "[[$symbol( $version)](fg:black)]($style)"},
    {"php", "[[$symbol( $version)](fg:black)]($style)"},
    {"java", "[[$symbol( $version)](fg:black)]($style)"},
    {"kotlin", "[[$symbol( $version)](fg:black)]($style)"},
    {"haskell", "[[$symbol( $version)](fg:black)]($style)"},
    {"docker_context", "[[$symbol( $context)](fg:black)]($style)"},
    {"conda", "[[$symbol$environment](fg:black)]($style)"},
    {"pixi", "[[$symbol( $version)( $environment)](fg:black)]($style)"},
    {"time", "[[ $time ](fg:black)]($style)"},
    {"cmd_duration", "[[ $duration ](fg:black)]($style)"},
    {"line_break", "\n"},
    {"character", "[$symbol]($style)"},
    {"memory_usage", "$symbol$usage"},
    {"cpu", "$symbol$usage"},
    {"battery", "$symbol$percentage"}
};

// Regex patterns (compiled once for performance)
const QRegularExpression RE_UNRESOLVED_VAR(R"(\$[a-zA-Z_0-9]+)");
const QRegularExpression RE_UNRESOLVED_VAR_BRACE(R"(\$\{[a-zA-Z_0-9]+\})");
const QRegularExpression RE_EMPTY_GROUP(R"(?<!\]\()\s*\)");
const QRegularExpression RE_EMPTY_GROUP_SPACES(R"(?<!\]\()\s+\)");
const QRegularExpression RE_INTERNAL_GROUP(R"(?<!\]\()([^()]*)\)");
const QRegularExpression RE_VAR_IN_GROUP(R"(\$[a-zA-Z_0-9]+|\$\{[a-zA-Z_0-9]+\})");
const QRegularExpression RE_DOUBLE_SPACES(R"(\s{2,})");
const QRegularExpression RE_FG_COLOR(R"(fg:([a-zA-Z0-9_#]+))");
const QRegularExpression RE_BG_COLOR(R"(bg:([a-zA-Z0-9_#]+))");
const QRegularExpression RE_TRIANGLE_CONNECTOR(R"([])");
const QRegularExpression RE_VAR_NAME(R"(^\$([a-zA-Z0-9_]+))");

// Helper: format a color value to CSS (handles hex, rgb, rgba, named colors)
QString formatCssColor(const QString &val)
{
    QString v = val.trimmed();
    if (v.isEmpty()) return "inherit";

    // Already a valid CSS color format
    if (v.startsWith('#') || v.startsWith("rgb") || v.startsWith("rgba") ||
        v.startsWith("hsl") || v.startsWith("hsla") || v == "transparent" ||
        v == "inherit" || v == "currentColor") {
        return v;
    }

    // Named color - pass through
    return v;
}

// Build the CSS style block
QString buildPreviewCss(double scale, int width)
{
    // The CSS from the Python implementation, adapted for Qt WebEngine
    return QString(R"(
            <style>
                body {
                    background: transparent;
                    margin: 0;
                    padding: 0;
                    font-family: %1;
                }
                .scaler {
                    transform: scale(%2);
                    transform-origin: top left;
                    width: %3px;
                }
                .terminal {
                    background: %4;
                    border: 1px solid %5;
                    border-radius: %6px;
                    padding: %7px %8px;
                    min-height: %9px;
                    line-height: %9px;
                    font-size: %10px;
                    display: inline-block;
                    white-space: pre;
                    box-shadow: 0 %11px %12px %13;
                }
                .prompt-line {
                    display: flex;
                    flex-wrap: wrap;
                    align-items: center;
                }
                .text-node {
                    font-size: %10px;
                }
                .glyph {
                    font-size: %14;
                    line-height: %9px;
                }
                .glyph2 {
                    /* triangle connectors get independent styling if needed */
                }
                span {
                    display: inline-flex;
                    align-items: center;
                }
            </style>
        )")
        .arg(CSS_FONT_FAMILY)
        .arg(scale, 0, 'f', 2)
        .arg(width)
        .arg(CSS_TERMINAL_BACKGROUND)
        .arg(CSS_TERMINAL_BORDER)
        .arg(CSS_TERMINAL_BORDER_RADIUS)
        .arg(CSS_TERMINAL_PADDING_Y)
        .arg(CSS_TERMINAL_PADDING_X)
        .arg(CSS_PROMPT_LINE_HEIGHT)
        .arg(CSS_TEXT_NODE_FONT_SIZE)
        .arg(CSS_SHADOW_Y)
        .arg(CSS_SHADOW_BLUR)
        .arg(CSS_SHADOW_COLOR)
        .arg(CSS_GLYPH_FONT_SIZE);
}

// Build complete HTML wrapper
QString buildHtmlWrapper(const QString &innerHtml, double scale, int width)
{
    return QStringLiteral("<html><head>") + buildPreviewCss(scale, width) +
           QStringLiteral("</head><body><div class=\"scaler\"><div class=\"terminal\"><div class=\"prompt-line\">") +
           innerHtml +
           QStringLiteral("</div></div></div></body></html>");
}

// Resolve a color key using active palette and standard colors
QString resolveColor(const QString &key, const std::map<QString, QString> &activePalette)
{
    if (key.isEmpty()) return "inherit";

    QString cleanKey = key.toLower();
    cleanKey.remove("fg:").remove("bg:").trimmed();

    auto it = activePalette.find(cleanKey);
    if (it != activePalette.end()) {
        return formatCssColor(it->second);
    }

    auto stdIt = STANDARD_COLORS.find(cleanKey);
    if (stdIt != STANDARD_COLORS.end()) {
        return formatCssColor(stdIt.value());
    }

    return formatCssColor(cleanKey);
}

// Build CSS declarations from a Starship style string
QStringList buildElementCss(const QString &styleStr, const std::map<QString, QString> &activePalette)
{
    QStringList css;

    if (styleStr.contains("bold", Qt::CaseInsensitive)) {
        css.append(QStringLiteral("font-weight: bold"));
    }

    QRegularExpressionMatch bgMatch = RE_BG_COLOR.match(styleStr);
    QRegularExpressionMatch fgMatch = RE_FG_COLOR.match(styleStr);

    if (bgMatch.hasMatch()) {
        css.append(QStringLiteral("background-color: %1").arg(resolveColor(bgMatch.captured(1), activePalette)));
    }
    if (fgMatch.hasMatch()) {
        css.append(QStringLiteral("color: %1").arg(resolveColor(fgMatch.captured(1), activePalette)));
    } else if (!styleStr.trimmed().isEmpty() && !styleStr.contains("bg:")) {
        QString bareColor = styleStr;
        bareColor.remove("bold").trimmed();
        if (!bareColor.isEmpty()) {
            css.append(QStringLiteral("color: %1").arg(resolveColor(bareColor, activePalette)));
        }
    }

    return css;
}

// Classify content as glyph or text-node
QString classifyContentClass(const QString &content)
{
    // Check for powerline glyphs
    bool hasGlyph = false;
    bool hasTriangle = false;
    for (const QChar &ch : content) {
        if (QString(POWERLINE_GLYPHS).contains(ch)) {
            hasGlyph = true;
            if (QString(TRIANGLE_CONNECTORS).contains(ch)) {
                hasTriangle = true;
            }
        }
    }
    if (hasGlyph) {
        return hasTriangle ? QStringLiteral("glyph glyph2") : QStringLiteral("glyph");
    }
    return QStringLiteral("text-node");
}

// Find a [content](style) block
struct BracketBlock {
    QString content;
    QString style;
    int start;
    int end;
    bool valid = false;
};

BracketBlock findBracketBlock(const QString &text, int start)
{
    BracketBlock result;
    if (start >= text.length() || text[start] != '[') return result;

    int depth = 1;
    int j = start + 1;
    while (j < text.length() && depth > 0) {
        if (text[j] == '[') depth++;
        else if (text[j] == ']') depth--;
        j++;
    }

    if (depth != 0 || j >= text.length() || text[j] != '(') return result;

    QString content = text.mid(start + 1, j - start - 2);
    int k = j + 1;
    while (k < text.length() && text[k] != ')') k++;
    if (k >= text.length()) return result;

    QString style = text.mid(j + 1, k - j - 1);
    result.content = content;
    result.style = style;
    result.start = start;
    result.end = k + 1;
    result.valid = true;
    return result;
}

// Find a $variable
struct VariableMatch {
    QString name;
    int end;
    bool valid = false;
};

VariableMatch findVariable(const QString &text, int start)
{
    VariableMatch result;
    if (start >= text.length() || text[start] != '$') return result;

    QRegularExpressionMatch match = RE_VAR_NAME.match(text.mid(start));
    if (match.hasMatch()) {
        result.name = match.captured(1);
        result.end = start + match.capturedEnd();
        result.valid = true;
    }
    return result;
}

// Tokenize a format string into segments
struct Segment {
    enum Type { Block, LineBreak, Variable, Text } type;
    QString content;   // for Block: inner content; for Variable: var name
    QString style;     // for Block: style
    int start;
    int end;
};

QVector<Segment> getSegments(const QString &text)
{
    QVector<Segment> segments;
    int i = 0;
    int len = text.length();

    while (i < len) {
        QChar ch = text[i];

        if (ch == '[') {
            BracketBlock block = findBracketBlock(text, i);
            if (block.valid) {
                segments.append({Segment::Block, block.content, block.style, block.start, block.end});
                i = block.end;
                continue;
            }
        }

        if (text.mid(i).startsWith("$line_break")) {
            segments.append({Segment::LineBreak, QString(), QString(), i, i + 11});
            i += 11;
            continue;
        }

        if (ch == '$') {
            VariableMatch var = findVariable(text, i);
            if (var.valid) {
                segments.append({Segment::Variable, QString(), var.name, var.end - var.name.length() - 1, var.end});
                i = var.end;
                continue;
            }
        }

        i++;
    }

    return segments;
}

// Clean Starship formatting artifacts from rendered text
QString cleanFinalContent(const QString &text)
{
    if (text.isEmpty()) return QString();

    QString result = text;

    // 1. Remove unresolved variables
    result.replace(RE_UNRESOLVED_VAR, QString());
    result.replace(RE_UNRESOLVED_VAR_BRACE, QString());

    // 2. Unescape Starship escape sequences
    result.replace("\\(", "(").replace("\\)", ")");
    result.replace("\\[", "[").replace("\\]", "]");
    result.replace("\\ ", " ");

    // 3. Strip residual empty optional groups
    int pythonMajor = 3;
    int pythonMinor = 12; // reasonable default
    QString pythonVer = QString("v%1.%2").arg(pythonMajor).arg(pythonMinor);

    for (int iter = 0; iter < 4; ++iter) {
        result.replace(RE_EMPTY_GROUP, QString());
        result.replace(RE_EMPTY_GROUP_SPACES, QString());
        result.replace("((", "(").replace("))", ")");
        result.replace("()", "").replace("( )", "");
        result.replace("( )", " ");
        result.replace(QString(" (%1)").arg(pythonVer), QString("%1 (.venv)").arg(pythonVer));
        result.replace(QString("(%1 )").arg(pythonVer), QString(" %1 (.venv)").arg(pythonVer));
    }

    // 4. Normalize whitespace
    result.replace(RE_DOUBLE_SPACES, " ");
    return result.trimmed();
}

// Substitute variables in text
QString substituteVariables(const QString &text, const QMap<QString, QString> &subs)
{
    QString result = text;
    for (auto it = subs.constBegin(); it != subs.constEnd(); ++it) {
        result.replace(QString("$%1").arg(it.key()), it.value());
        result.replace(QString("${%1}").arg(it.key()), it.value());
    }
    return result;
}

// Evaluate optional groups (...)
QString evaluateOptionalGroups(QString text)
{
    bool changed = true;
    while (changed) {
        changed = false;
        QRegularExpressionMatch match = RE_INTERNAL_GROUP.match(text);
        if (!match.hasMatch()) break;

        QString inner = match.captured(1);
        QString contentCheck = inner;
        contentCheck = contentCheck.replace(RE_VAR_IN_GROUP, QString())
            .replace("\\(", "").replace("\\)", "")
            .replace("\\[", "").replace("\\]", "").trimmed();

        if (contentCheck.isEmpty()) {
            // Group is empty -> remove entirely
            text = text.left(match.capturedStart()) + text.mid(match.capturedEnd());
        } else {
            // Mark with control chars to skip next iteration
            text = text.left(match.capturedStart()) + QChar(0x0001) + inner + QChar(0x0002) + text.mid(match.capturedEnd());
        }
        changed = true;
    }

    text.replace(QChar(0x0001), "(").replace(QChar(0x0002), ")");
    return text;
}

// Build module substitutions
QMap<QString, QString> buildModuleSubstitutions(const QString &moduleName,
                                                 const toml::table &moduleCfg,
                                                 const QMap<QString, QString> &context)
{
    QMap<QString, QString> subs;

    // Base: all scalar keys from the module config
    for (const auto &[key, val] : moduleCfg) {
        if (val.is_string() || val.is_integer() || val.is_floating_point() || val.is_boolean()) {
            subs[QString::fromStdString(std::string(key))] = QString::fromStdString(val.as_string()->get());
        }
    }

    // Fallback for missing `style` key
    if (!subs.contains("style")) {
        if (moduleName == "username" && moduleCfg.contains("style_user")) {
            subs["style"] = QString::fromStdString(moduleCfg["style_user"].as_string()->get());
        } else if (moduleCfg.contains("style")) {
            subs["style"] = QString::fromStdString(moduleCfg["style"].as_string()->get());
        }
    }

    // Runtime values derived from context
    QString vVal = context.value(moduleName, QString());
    if (moduleName == "os") vVal = QString(); // Don't leak "Arch" as the OS module version

    QMap<QString, QString> runtimeValues;
    runtimeValues["path"] = context.value("directory", QString());
    runtimeValues["user"] = context.value("username", QString());
    runtimeValues["username"] = context.value("username", QString());
    runtimeValues["time"] = context.value("time", QString());
    runtimeValues["version"] = vVal;
    runtimeValues["branch"] = context.value("git_branch", QString());
    runtimeValues["all_status"] = context.value("git_status", QString());
    runtimeValues["ahead_behind"] = QString();
    runtimeValues["virtualenv"] = QString();

    // Module-specific symbol resolution
    if (moduleName == "character") {
        QString symbol = moduleCfg.contains("success_symbol")
            ? QString::fromStdString(moduleCfg["success_symbol"].as_string()->get())
            : (moduleCfg.contains("symbol") ? QString::fromStdString(moduleCfg["symbol"].as_string()->get()) : QString("❯"));
        runtimeValues["symbol"] = symbol;
    }

    if (moduleName == "os") {
        QString osVal = context.value("os", QString("Arch"));
        if (moduleCfg.contains("symbols") && moduleCfg["symbols"].is_table()) {
            auto symbolsTable = moduleCfg["symbols"].as_table();
            if (symbolsTable->contains(osVal.toStdString())) {
                runtimeValues["symbol"] = QString::fromStdString((*symbolsTable)[osVal.toStdString()].as_string()->get());
            }
        }
        runtimeValues["os"] = QString(); // Hide the literal OS name
    }

    // Merge runtime values (they override config values)
    for (auto it = runtimeValues.constBegin(); it != runtimeValues.constEnd(); ++it) {
        subs[it.key()] = it.value();
    }

    return subs;
}

// Solve a module's format string into rendered text
QString solveModuleFormat(const QString &moduleName,
                          const toml::table &moduleCfg,
                          const QMap<QString, QString> &context)
{
    if (!moduleCfg.is_table()) return QString();

    // 1. Resolve format template
    QString defaultFmt = DEFAULT_MODULE_FORMATS.value(moduleName, "$symbol$version");
    QString fmt = moduleCfg.contains("format")
        ? QString::fromStdString(moduleCfg["format"].as_string()->get())
        : defaultFmt;

    // 2. Build substitution map
    QMap<QString, QString> subs = buildModuleSubstitutions(moduleName, moduleCfg, context);

    // 3. First pass: substitute variables
    QString result = substituteVariables(fmt, subs);

    // 4. Evaluate optional groups
    result = evaluateOptionalGroups(result);

    // 5. Final pass: catch any remaining variables from outer scope
    result = substituteVariables(result, subs);

    // 6. Cleanup
    result.replace(RE_DOUBLE_SPACES, " ");
    if (moduleName == "os") result = result.trimmed();

    return result;
}

// Recursive text segment processor
void processTextSegment(const QString &text,
                        const QString &defaultStyle,
                        const QString &group,
                        bool isStarship,
                        const toml::table &doc,
                        const QMap<QString, QString> &context,
                        const std::map<QString, QString> &activePalette,
                        QString &htmlOutput)
{
    QVector<Segment> segments = getSegments(text);

    if (segments.isEmpty()) {
        QString cleaned = cleanFinalContent(text);
        if (!cleaned.isEmpty() || text == " ") {
            QStringList css = buildElementCss(defaultStyle, activePalette);
            QString cls = classifyContentClass(cleaned);
            QString escaped = cleaned.replace(" ", "&nbsp;");
            htmlOutput += QString("<span class=\"%1\" style=\"%2\">%3</span>")
                              .arg(cls)
                              .arg(css.join("; "))
                              .arg(escaped);
        }
        return;
    }

    int lastIdx = 0;
    for (const Segment &seg : segments) {
        // Text before this segment
        QString pre = text.mid(lastIdx, seg.start - lastIdx);
        if (!pre.isEmpty()) {
            processTextSegment(pre, defaultStyle, group, isStarship, doc, context, activePalette, htmlOutput);
        }

        if (seg.type == Segment::Block) {
            QString innerStyle = seg.style;
            if (innerStyle == "$style" || innerStyle == "${style}") {
                innerStyle = defaultStyle;
            }
            processTextSegment(seg.content, innerStyle, group, isStarship, doc, context, activePalette, htmlOutput);
        } else if (seg.type == Segment::LineBreak) {
            htmlOutput += QStringLiteral("</div><div class=\"prompt-line\">");
        } else if (seg.type == Segment::Variable) {
            if (WHITELIST_MODULES.contains(seg.content)) {
                if (doc.contains(seg.content.toStdString())) {
                    auto moduleNode = doc[seg.content.toStdString()];
                    if (moduleNode.is_table()) {
                        auto moduleTable = moduleNode.as_table();
                        bool disabled = false;
                        if (moduleTable->contains("disabled")) {
                            disabled = (*moduleTable)["disabled"].as_boolean()->get();
                        }
                        if (!disabled) {
                            QString expanded = solveModuleFormat(seg.content, *moduleTable, context);
                            QString moduleStyle = moduleTable->contains("style")
                                ? QString::fromStdString((*moduleTable)["style"].as_string()->get())
                                : QString();
                            processTextSegment(expanded, moduleStyle, seg.content, true, doc, context, activePalette, htmlOutput);
                        }
                    }
                }
            }
        }

        lastIdx = seg.end;
    }

    // Trailing text after the last segment
    QString post = text.mid(lastIdx);
    if (!post.isEmpty()) {
        processTextSegment(post, defaultStyle, group, isStarship, doc, context, activePalette, htmlOutput);
    }
}

} // anonymous namespace

QString starshipPreviewHtml(const toml::table &doc,
                                   int paletteIndex,
                                   double scale,
                                   int width,
                                   const std::map<QString, QString> &overrides)
{
    // Build runtime context with defaults
    QMap<QString, QString> context;
    context["os"] = QStringLiteral("Arch");
    context["username"] = QProcessEnvironment::systemEnvironment().value("USER", QStringLiteral("user"));
    context["directory"] = QStringLiteral("~/dev/scripts");
    context["python"] = QStringLiteral("v3.12");
    context["time"] = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    context["git_branch"] = QStringLiteral("main");
    context["git_status"] = QStringLiteral("");

    // Apply overrides
    for (const auto &[key, val] : overrides) {
        context[key] = val;
    }

    // Resolve active palette
    std::map<QString, QString> activePalette;
    if (auto palettesNode = doc.get("palettes"); palettesNode && palettesNode->is_table()) {
        auto palettesTable = palettesNode->as_table();
        QString activePaletteName;

        if (paletteIndex >= 0) {
            // Use palette by index
            int idx = 0;
            for (const auto &[pname, pval] : *palettesTable) {
                if (idx == paletteIndex) {
                    activePaletteName = QString::fromStdString(std::string(pname));
                    break;
                }
                idx++;
            }
        } else {
            // Use document's active palette
            if (auto paletteNode = doc.get("palette"); paletteNode && paletteNode->is_value()) {
                activePaletteName = QString::fromStdString(paletteNode->as_string()->get());
            }
        }

        if (!activePaletteName.isEmpty() && palettesTable->contains(activePaletteName.toStdString())) {
            auto paletteNode = (*palettesTable)[activePaletteName.toStdString()];
            if (paletteNode.is_table()) {
                for (const auto &[ckey, cval] : *paletteNode.as_table()) {
                    if (cval.is_string()) {
                        activePalette[QString::fromStdString(std::string(ckey))] = QString::fromStdString(cval.as_string()->get());
                    }
                }
            }
        }
    }

    // Get format string
    QString formatStr;
    if (auto fmtNode = doc.get("format"); fmtNode && fmtNode->is_value()) {
        formatStr = QString::fromStdString(fmtNode->as_string()->get());
        formatStr.replace("\\\n", "").replace("\n", "");
    } else {
        formatStr = QString();
    }

    // Process format string
    QString htmlSegments;
    processTextSegment(formatStr, QString(), QStringLiteral("root"), false, doc, context, activePalette, htmlSegments);

    // Wrap in HTML
    return buildHtmlWrapper(htmlSegments, scale, width);
}

} // namespace tools