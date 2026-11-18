#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace tools
{

using ColorMap = QMap<QString, QString>;

struct ThemeData
{
    ColorMap manifestColors;  // from manifest.json (matched_text_hl_colors, when_selected, ...)
    ColorMap themeColors;     // from theme.css @define-color entries
    QString manifestJson;     // raw text
    QString cssContent;       // raw text
    bool valid = false;
};

// Installed by templateinstaller.
QString bundledTemplateDir();

QString userThemesDir();

// Sub-directories containing manifest.json, sorted by name.
QStringList listThemeDirs(const QString &root);

// So commented-out declarations are never parsed.
QString stripCssComments(const QString &cssContent);

// In order of appearance, not alphabetical.
QStringList cssColorOrder(const QString &cssContent);

// Requires manifest.json + theme.css.
ThemeData parseTheme(const QString &themePath);

ColorMap extractCssColors(const QString &cssContent);

ColorMap parseManifestColors(const QString &manifestJson);

// Empty string on failure.
QString replaceCssColors(const QString &cssContent, const ColorMap &newColors);

// Empty string on failure.
QString updateManifestColors(const QString &manifestJson, const ColorMap &newColors);

// Writes both files atomically with .bak backups; returns error message, empty on success.
QString applyTheme(const QString &themePath, const ColorMap &manifestColors, const ColorMap &themeColors);

bool hasThemeBackup(const QString &themePath);

bool restoreThemeBackup(const QString &themePath);

// Copies source; manifest name/display_name become newName; empty path on failure.
QString createTheme(const QString &sourcePath, const QString &newName);

// Refuses the active theme and template dirs.
bool deleteTheme(const QString &themePath, const QString &activeThemePath);

QString resolveActiveThemePath();

// ~/.config/ulauncher/settings.json; its `theme_name` marks the active theme.
QString ulauncherSettingsPath();

// Writes the manifest name (dir name as fallback) into `theme_name`; error message, empty on success.
QString setActiveThemePath(const QString &themePath);

} // namespace tools