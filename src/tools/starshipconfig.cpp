#include "tools/starshipconfig.h"

#include "tools/fileutil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

#include <toml++/toml.hpp>
#include <sstream>

namespace tools
{

QString starshipConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/starship.toml");
}

QString starshipTemplateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwal/templates/starship");
}

StarshipConfigResult readStarshipConfig(const QString &path)
{
    StarshipConfigResult result;
    const QString cfgPath = path.isEmpty() ? starshipConfigPath() : path;

    QFileInfo info(cfgPath);
    if (!info.exists() || !info.isFile()) {
        result.configPath = QStringLiteral("~/.config/starship.toml");
        return result;
    }

    try {
        result.configPath = QStringLiteral("~/.config/starship.toml");
        auto doc = toml::parse_file(cfgPath.toStdString());
        result.valid = true;
        result.palettes = extractPalettes(doc);
        result.activePalette = getActivePaletteName(doc);
    } catch (const toml::parse_error &e) {
        qWarning() << "Failed to parse starship config:" << QString::fromStdString(e.what());
        result.valid = false;
    } catch (const std::exception &e) {
        qWarning() << "Error reading starship config:" << e.what();
        result.valid = false;
    }

    return result;
}

PaletteMap extractPalettes(const toml::table &doc)
{
    PaletteMap palettes;
    if (auto palettesNode = doc.get("palettes"); palettesNode && palettesNode->is_table()) {
        for (const auto &[pname, pval] : *palettesNode->as_table()) {
            if (pval.is_table()) {
                std::map<QString, QString> colors;
                for (const auto &[ckey, cval] : *pval.as_table()) {
                    if (cval.is_string()) {
                        colors[QString::fromStdString(std::string(ckey))] = QString::fromStdString(cval.as_string()->get());
                    } else if (cval.is_value()) {
                        colors[QString::fromStdString(std::string(ckey))] = QString::fromStdString(cval.as_string()->get());
                    }
                }
                palettes[QString::fromStdString(std::string(pname))] = std::move(colors);
            }
        }
    }
    return palettes;
}

QString getActivePaletteName(const toml::table &doc)
{
    if (auto paletteNode = doc.get("palette"); paletteNode && paletteNode->is_value()) {
        return QString::fromStdString(paletteNode->as_string()->get());
    }
    return QString();
}

void replacePalettesInDoc(toml::table &doc, const PaletteMap &palettes, const QString &activePalette)
{
    // Remove existing palettes table
    doc.erase("palettes");

    // Create new palettes table
    toml::table newPalettes;
    for (const auto &[pname, colors] : palettes) {
        toml::table paletteTable;
        for (const auto &[ckey, cval] : colors) {
            paletteTable.insert(ckey.toStdString(), cval.toStdString());
        }
        newPalettes.insert(pname.toStdString(), std::move(paletteTable));
    }
    doc.insert("palettes", std::move(newPalettes));

    // Update active palette key
    if (!activePalette.isEmpty()) {
        doc.insert_or_assign("palette", activePalette.toStdString());
    } else {
        doc.erase("palette");
    }
}

bool applyStarshipPalettes(const QString &configPath,
                           const PaletteMap &palettes,
                           const QString &activePalette)
{
    const QString cfgPath = configPath.isEmpty() ? starshipConfigPath() : configPath;

    // Ensure config directory exists
    QFileInfo info(cfgPath);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    // Create backup if this is the first write
    backupFile(cfgPath);

    toml::table doc;
    if (QFileInfo::exists(cfgPath)) {
        try {
            doc = toml::parse_file(cfgPath.toStdString());
        } catch (const toml::parse_error &) {
            // If parsing fails, start with empty doc
            doc.clear();
        } catch (const std::exception &) {
            doc.clear();
        }
    }

    replacePalettesInDoc(doc, palettes, activePalette);

    // Serialize TOML to string
    std::ostringstream oss;
    oss << toml::toml_formatter{ doc };
    const std::string tomlString = oss.str();

    return writeFileAtomic(cfgPath, QString::fromStdString(tomlString));
}

bool restoreStarshipBackup(const QString &configPath)
{
    const QString cfgPath = configPath.isEmpty() ? starshipConfigPath() : configPath;
    return restoreBackup(cfgPath);
}

bool hasStarshipBackup(const QString &configPath)
{
    const QString cfgPath = configPath.isEmpty() ? starshipConfigPath() : configPath;
    return hasBackup(cfgPath);
}

toml::table loadStarshipTemplate(const QString &templateName)
{
    const QString tmplPath = starshipTemplateDir() + QLatin1Char('/') + templateName + QStringLiteral(".toml");
    QFileInfo info(tmplPath);
    if (!info.exists() || !info.isFile()) {
        return toml::table();
    }

    try {
        return toml::parse_file(tmplPath.toStdString());
    } catch (const toml::parse_error &e) {
        qWarning() << "Failed to parse template" << templateName << ":" << QString::fromStdString(e.what());
    } catch (const std::exception &e) {
        qWarning() << "Error loading template" << templateName << ":" << e.what();
    }
    return toml::table();
}

QStringList listStarshipTemplates()
{
    QStringList names;
    QDir dir(starshipTemplateDir());
    if (!dir.exists()) {
        return names;
    }

    const auto entries = dir.entryInfoList(QStringList() << QStringLiteral("*.toml"),
                                           QDir::Files | QDir::NoDotAndDotDot,
                                           QDir::Name);
    for (const QFileInfo &fi : entries) {
        names.append(fi.baseName());
    }
    return names;
}

} // namespace tools