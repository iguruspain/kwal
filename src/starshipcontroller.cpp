#include "starshipcontroller.h"

#include "tools/starshipconfig.h"
#include "tools/starshippreview.h"
#include "tools/fileutil.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>
#include <toml++/toml.hpp>

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("StarshipController", text);
}
}

StarshipController::StarshipController(QObject *parent)
    : QObject(parent)
{
    m_configPath = tools::starshipConfigPath();
    loadTemplateList();
    refresh();
}

QString StarshipController::configPath() const
{
    return m_configPath;
}

QString StarshipController::selectedTemplate() const
{
    return m_selectedTemplate;
}

int StarshipController::templateIndex() const
{
    return m_templateIndex;
}

void StarshipController::setTemplateIndex(int index)
{
    if (m_templateIndex != index) {
        m_templateIndex = index;
        emit templateIndexChanged();

        if (index >= 0 && index < m_templateNames.size()) {
            loadTemplate(m_templateNames[index]);
        } else {
            // Index -1 means current config
            clearSelection();
        }
    }
}

QString StarshipController::draftColor() const
{
    return m_draftColor;
}

void StarshipController::setDraftColor(const QString &color)
{
    if (m_draftColor != color) {
        m_draftColor = color;
        emit draftColorChanged();

        // Apply the draft color to the current palette/color index
        if (m_draftPaletteIndex >= 0 && m_draftPaletteIndex < m_paletteValues.size() &&
            m_draftColorIndex >= 0 && m_draftColorIndex < m_paletteValues[m_draftPaletteIndex].size()) {
            m_paletteValues[m_draftPaletteIndex][m_draftColorIndex] = color;
            emit paletteValuesChanged();
        }

        regeneratePreview();
    }
}

int StarshipController::draftPaletteIndex() const
{
    return m_draftPaletteIndex;
}

void StarshipController::setDraftPaletteIndex(int index)
{
    if (m_draftPaletteIndex != index) {
        m_draftPaletteIndex = index;
        emit draftPaletteIndexChanged();

        // Reset color index when palette changes
        if (index >= 0 && index < m_paletteValues.size() && !m_paletteValues[index].isEmpty()) {
            setDraftColorIndex(0);
        } else {
            setDraftColorIndex(-1);
        }
    }
}

int StarshipController::draftColorIndex() const
{
    return m_draftColorIndex;
}

void StarshipController::setDraftColorIndex(int index)
{
    if (m_draftColorIndex != index) {
        m_draftColorIndex = index;
        emit draftColorIndexChanged();

        // Update draft color to reflect the selected color
        if (index >= 0 && m_draftPaletteIndex >= 0 &&
            m_draftPaletteIndex < m_paletteValues.size() &&
            index < m_paletteValues[m_draftPaletteIndex].size()) {
            m_draftColor = m_paletteValues[m_draftPaletteIndex][index].toString();
            emit draftColorChanged();
        } else {
            m_draftColor = QStringLiteral("transparent");
            emit draftColorChanged();
        }
    }
}

QString StarshipController::previewHtml() const
{
    return m_previewHtml;
}

bool StarshipController::hasBackup() const
{
    return m_hasBackup;
}

bool StarshipController::hasConfig() const
{
    return m_hasConfig;
}

QStringList StarshipController::templateNames() const
{
    return m_templateNames;
}

QVariantList StarshipController::paletteNames() const
{
    QVariantList result;
    for (const QString &name : m_paletteNames) {
        result.append(name);
    }
    return result;
}

QVariantList StarshipController::paletteValues() const
{
    QVariantList result;
    for (const QVariantList &list : m_paletteValues) {
        result.append(list);
    }
    return result;
}

QVariantList StarshipController::paletteKeys() const
{
    QVariantList result;
    for (const QVariantList &list : m_paletteKeys) {
        result.append(list);
    }
    return result;
}

void StarshipController::refresh()
{
    const tools::StarshipConfigResult result = tools::readStarshipConfig(m_configPath);

    if (result.valid) {
        m_paletteNames.clear();
        m_paletteValues.clear();
        m_paletteKeys.clear();

        for (const auto &[pname, colors] : result.palettes) {
            m_paletteNames.append(pname);
            QVariantList values;
            QVariantList keys;
            for (const auto &[ckey, cval] : colors) {
                keys.append(ckey);
                values.append(cval);
            }
            m_paletteKeys.append(keys);
            m_paletteValues.append(values);
        }

        emit paletteNamesChanged();
        emit paletteValuesChanged();
        emit paletteKeysChanged();

        // Select first palette by default
        if (!m_paletteNames.isEmpty()) {
            setDraftPaletteIndex(0);
        }
    } else {
        m_paletteNames.clear();
        m_paletteValues.clear();
        m_paletteKeys.clear();
        emit paletteNamesChanged();
        emit paletteValuesChanged();
        emit paletteKeysChanged();
    }

    updateHasBackup();
    updateHasConfig();
    regeneratePreview();
}

void StarshipController::loadTemplate(const QString &name)
{
    if (name.isEmpty()) return;

    auto doc = tools::loadStarshipTemplate(name);
    if (doc.empty()) {
        emit operationResult(false, tr("Failed to load template: %1").arg(name));
        return;
    }

    m_selectedTemplate = name;
    emit selectedTemplateChanged();

    // Extract palettes from template
    auto palettes = tools::extractPalettes(doc);

    m_paletteNames.clear();
    m_paletteValues.clear();
    m_paletteKeys.clear();

    for (const auto &[pname, colors] : palettes) {
        m_paletteNames.append(pname);
        QVariantList values;
        QVariantList keys;
        for (const auto &[ckey, cval] : colors) {
            keys.append(ckey);
            values.append(cval);
        }
        m_paletteKeys.append(keys);
        m_paletteValues.append(values);
    }

    emit paletteNamesChanged();
    emit paletteValuesChanged();
    emit paletteKeysChanged();

    // Select first palette by default
    if (!m_paletteNames.isEmpty()) {
        setDraftPaletteIndex(0);
    }

    regeneratePreview();
}

void StarshipController::clearSelection()
{
    m_templateIndex = -1;
    emit templateIndexChanged();

    m_selectedTemplate.clear();
    emit selectedTemplateChanged();

    refresh();
}

void StarshipController::applyConfig()
{
    // Build PaletteMap from current palette values
    tools::PaletteMap palettes;
    for (int i = 0; i < m_paletteNames.size(); ++i) {
        std::map<QString, QString> colors;
        const QVariantList &keys = m_paletteKeys[i];
        const QVariantList &values = m_paletteValues[i];
        for (int j = 0; j < keys.size() && j < values.size(); ++j) {
            colors[keys[j].toString()] = values[j].toString();
        }
        palettes[m_paletteNames[i]] = std::move(colors);
    }

    // Determine active palette name
    QString activePalette;
    if (m_draftPaletteIndex >= 0 && m_draftPaletteIndex < m_paletteNames.size()) {
        activePalette = m_paletteNames[m_draftPaletteIndex];
    }

    bool ok = tools::applyStarshipPalettes(m_configPath, palettes, activePalette);

    if (ok) {
        updateHasBackup();
        m_configValid = tools::readStarshipConfig(m_configPath).valid;
        emit operationResult(true, tr("Starship configuration applied successfully."));
    } else {
        emit operationResult(false, tr("Failed to apply starship configuration."));
    }
}

void StarshipController::restoreBackup()
{
    bool ok = tools::restoreStarshipBackup(m_configPath);

    if (ok) {
        updateHasBackup();
        refresh();
        emit operationResult(true, tr("Starship configuration restored from backup."));
    } else {
        emit operationResult(false, tr("No backup found or restore failed."));
    }
}

void StarshipController::regeneratePreview()
{
    // Build a toml::table from current palette data for preview
    toml::table doc;

    // Copy format and other config from current config if available
    if (m_configValid) {
        try {
            doc = toml::parse_file(m_configPath.toStdString());
        } catch (...) {
            doc.clear();
        }
    }

    // Replace palettes with current draft state
    tools::PaletteMap palettes;
    for (int i = 0; i < m_paletteNames.size(); ++i) {
        std::map<QString, QString> colors;
        const QVariantList &keys = m_paletteKeys[i];
        const QVariantList &values = m_paletteValues[i];
        for (int j = 0; j < keys.size() && j < values.size(); ++j) {
            colors[keys[j].toString()] = values[j].toString();
        }
        palettes[m_paletteNames[i]] = std::move(colors);
    }

    QString activePalette;
    if (m_draftPaletteIndex >= 0 && m_draftPaletteIndex < m_paletteNames.size()) {
        activePalette = m_paletteNames[m_draftPaletteIndex];
    }

    tools::replacePalettesInDoc(doc, palettes, activePalette);

    // Generate HTML preview
    m_previewHtml = tools::starshipPreviewHtml(doc, m_draftPaletteIndex, 1.0, 800, {});
    emit previewHtmlChanged();
}

void StarshipController::updateHasBackup()
{
    bool has = tools::hasStarshipBackup(m_configPath);
    if (m_hasBackup != has) {
        m_hasBackup = has;
        emit hasBackupChanged();
    }
}

void StarshipController::updateHasConfig()
{
    QFileInfo info(m_configPath);
    bool has = info.exists() && info.isFile();
    if (m_hasConfig != has) {
        m_hasConfig = has;
        emit hasConfigChanged();
    }
}

void StarshipController::loadTemplateList()
{
    m_templateNames = tools::listStarshipTemplates();
    emit templateNamesChanged();
}

#include "starshipcontroller.moc"