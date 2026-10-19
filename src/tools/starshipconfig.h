#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <map>
#include <toml++/toml.hpp>

namespace tools
{

// Starship config path: ~/.config/starship.toml (XDG_CONFIG_HOME aware).
QString starshipConfigPath();

// Starship templates directory: ~/.config/kwal/templates/starship
QString starshipTemplateDir();

// Parsed palette entry: name -> {key -> value}
using PaletteMap = std::map<QString, std::map<QString, QString>>;

// Result of reading a starship config file.
struct StarshipConfigResult
{
    QString configPath;       // Display path (e.g. ~/.config/starship.toml)
    PaletteMap palettes;      // All palettes found in the file
    QString activePalette;    // Name of the active palette (from `palette = '...'`)
    bool valid = false;       // Whether the file was parsed successfully
};

// Reads and parses a starship.toml file, extracting palettes.
StarshipConfigResult readStarshipConfig(const QString &path = QString());

// Applies the given palettes to the starship config atomically.
// Creates a .bak backup on first write. `activePalette` sets the top-level `palette` key.
// Returns true on success.
bool applyStarshipPalettes(const QString &configPath,
                           const PaletteMap &palettes,
                           const QString &activePalette = QString());

// Restores starship.toml from its .bak backup.
bool restoreStarshipBackup(const QString &configPath = QString());

// Whether a .bak backup exists for the starship config.
bool hasStarshipBackup(const QString &configPath = QString());

// Loads a template file from the starship template directory.
// Returns empty table on failure.
toml::table loadStarshipTemplate(const QString &templateName);

// Lists available template names (without .toml extension) from the template directory.
QStringList listStarshipTemplates();

// Extracts the palette map from a parsed TOML document.
PaletteMap extractPalettes(const toml::table &doc);

// Gets the active palette name from the document (top-level `palette` key).
QString getActivePaletteName(const toml::table &doc);

// Replaces all palettes in the TOML document with the given palette map.
// Updates the top-level `palette` key to `activePalette` if provided.
void replacePalettesInDoc(toml::table &doc, const PaletteMap &palettes, const QString &activePalette = QString());

} // namespace tools