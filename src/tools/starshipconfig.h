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

// ~/.config/starship.toml (XDG_CONFIG_HOME aware).
QString starshipConfigPath();

// ~/.config/kwal/templates/starship
QString starshipTemplateDir();

using PaletteMap = std::map<QString, std::map<QString, QString>>;

struct StarshipConfigResult
{
    QString configPath;       // display path, ~-abbreviated
    PaletteMap palettes;
    QString activePalette;    // top-level `palette` key
    bool valid = false;
};

StarshipConfigResult readStarshipConfig(const QString &path = QString());

// Backs up to .bak and writes atomically; activePalette sets the top-level `palette` key.
bool applyStarshipPalettes(const QString &configPath,
                           const PaletteMap &palettes,
                           const QString &activePalette = QString());

bool restoreStarshipBackup(const QString &configPath = QString());

bool hasStarshipBackup(const QString &configPath = QString());

// Empty table on failure.
toml::table loadStarshipTemplate(const QString &templateName);

// Template names without .toml extension.
QStringList listStarshipTemplates();

PaletteMap extractPalettes(const toml::table &doc);

QString getActivePaletteName(const toml::table &doc);

void replacePalettesInDoc(toml::table &doc, const PaletteMap &palettes, const QString &activePalette = QString());

bool writeStarshipDoc(const QString &configPath, const toml::table &doc);

} // namespace tools