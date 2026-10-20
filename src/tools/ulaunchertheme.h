#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace tools
{

// Theme color pair: variable name -> CSS color value
using ColorMap = QMap<QString, QString>;

// Result of parsing a theme directory
struct ThemeData
{
    ColorMap manifestColors;  // from manifest.json (matched_text_hl_colors, when_selected, etc.)
    ColorMap themeColors;     // from theme.css @define-color entries
    QString manifestJson;     // raw manifest.json content
    QString cssContent;       // raw theme.css content
    bool valid = false;
};

// Resolve the bundled template directory (installed by templateinstaller)
QString bundledTemplateDir();

// Resolve the user themes directory (~/.config/ulauncher/user-themes)
QString userThemesDir();

// List theme directories (sub-directories containing a manifest.json) in
// root, sorted by name. Returns an empty list when root does not exist.
QStringList listThemeDirs(const QString &root);

// Remove /* ... */ comment blocks from CSS content, so commented-out
// declarations are never parsed.
QString stripCssComments(const QString &cssContent);

// Return the @define-color names in cssContent in order of appearance.
QStringList cssColorOrder(const QString &cssContent);

// Parse a theme directory (must contain manifest.json and theme.css)
ThemeData parseTheme(const QString &themePath);

// Extract color variables from theme.css content (standalone, no file I/O)
ColorMap extractCssColors(const QString &cssContent);

// Parse manifest.json content for color-related keys
ColorMap parseManifestColors(const QString &manifestJson);

// Replace color values in theme.css content with new values
// Returns modified CSS content, or empty string on failure
QString replaceCssColors(const QString &cssContent, const ColorMap &newColors);

// Update manifest.json content with new color values
// Returns modified manifest JSON, or empty string on failure
QString updateManifestColors(const QString &manifestJson, const ColorMap &newColors);

// Apply theme: write manifest.json and theme.css atomically, creating .bak backups
// Returns empty string on success, error message on failure
QString applyTheme(const QString &themePath, const ColorMap &manifestColors, const ColorMap &themeColors);

// Check if backup files exist for a theme
bool hasThemeBackup(const QString &themePath);

// Restore theme from .bak files
bool restoreThemeBackup(const QString &themePath);

// Create a new theme directory by copying a source theme
// Updates manifest.json name/display_name to newName
// Returns new theme path on success, empty string on failure
QString createTheme(const QString &sourcePath, const QString &newName);

// Delete a user theme directory (safety checks: not active, not template)
bool deleteTheme(const QString &themePath, const QString &activeThemePath);

// Resolve the currently active theme path from ulauncher settings.json
// Returns empty string if not found or not configured
QString resolveActiveThemePath();

// Set themePath as the active theme by writing its manifest name (or the
// directory name as fallback) to theme_name in ulauncher settings.json.
// Returns empty string on success, error message on failure.
QString setActiveThemePath(const QString &themePath);

// Read base_width from ulauncher settings.json (the width of the real
// launcher window). Returns 680, Ulauncher's own default, when the setting
// is missing or unreadable.
int ulauncherBaseWidth();

} // namespace tools