#include "starshipcontroller.h"

#include "tools/starshipconfig.h"
#include "tools/starshippreview.h"

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

// Fill the palette draft lists from `palettes`, putting the document's active
// palette first (parity with the original app: index 0 edits the active one).
void fillPaletteDrafts(const tools::PaletteMap &palettes, const QString &activeName,
                       QStringList &names, QList<QVariantList> &values, QList<QVariantList> &keys)
{
    names.clear();
    values.clear();
    keys.clear();

    QStringList ordered;
    if (!activeName.isEmpty() && palettes.contains(activeName))
        ordered.append(activeName);
    for (const auto &[pname, colors] : palettes) {
        if (pname != activeName)
            ordered.append(pname);
    }

    for (const QString &pname : ordered) {
        names.append(pname);
        QVariantList keyList;
        QVariantList valueList;
        for (const auto &[ckey, cval] : palettes.at(pname)) {
            keyList.append(ckey);
            valueList.append(cval);
        }
        keys.append(keyList);
        values.append(valueList);
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

QString StarshipController::currentPreviewHtml() const
{
    return m_currentPreviewHtml;
}

double StarshipController::previewScale() const
{
    return m_previewScale;
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
        result.append(QVariant::fromValue(list));
    }
    return result;
}

QVariantList StarshipController::paletteKeys() const
{
    QVariantList result;
    for (const QVariantList &list : m_paletteKeys) {
        result.append(QVariant::fromValue(list));
    }
    return result;
}

void StarshipController::refresh()
{
    const tools::StarshipConfigResult result = tools::readStarshipConfig(m_configPath);
    m_configValid = result.valid;

    m_doc = toml::table();
    if (result.valid) {
        try {
            m_doc = toml::parse_file(m_configPath.toStdString());
        } catch (const std::exception &) {
            m_configValid = false;
            m_doc = toml::table();
        }
    }

    tools::PaletteMap palettes;
    QString active;
    if (result.valid) {
        palettes = result.palettes;
        active = result.activePalette;
    }
    fillPaletteDrafts(palettes, active, m_paletteNames, m_paletteValues, m_paletteKeys);

    emit paletteNamesChanged();
    emit paletteValuesChanged();
    emit paletteKeysChanged();

    // Select the active palette by default
    if (!m_paletteNames.isEmpty()) {
        setDraftPaletteIndex(0);
    }

    updateHasBackup();
    updateHasConfig();
    regenerateCurrentPreview();
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

    m_doc = doc;
    m_selectedTemplate = name;
    emit selectedTemplateChanged();

    fillPaletteDrafts(tools::extractPalettes(doc), tools::getActivePaletteName(doc),
                      m_paletteNames, m_paletteValues, m_paletteKeys);

    emit paletteNamesChanged();
    emit paletteValuesChanged();
    emit paletteKeysChanged();

    // Select the active palette by default
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
    if (m_paletteNames.isEmpty()) {
        emit operationResult(false, tr("No palette data available to apply."));
        return;
    }

    const tools::PaletteMap palettes = draftPalettes();
    // The first palette in the draft list is the document's active one.
    const QString activePalette = m_paletteNames.first();

    bool ok;
    if (!m_selectedTemplate.isEmpty()) {
        // Template mode: the template document becomes the new config file.
        toml::table doc = m_doc;
        tools::replacePalettesInDoc(doc, palettes, activePalette);
        ok = tools::writeStarshipDoc(m_configPath, doc);
    } else {
        ok = tools::applyStarshipPalettes(m_configPath, palettes, activePalette);
    }

    if (ok) {
        refresh();
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

void StarshipController::setColorValue(const QString &key, const QString &color)
{
    if (m_draftPaletteIndex < 0 || m_draftPaletteIndex >= m_paletteValues.size())
        return;

    const QVariantList &keys = m_paletteKeys[m_draftPaletteIndex];
    for (int i = 0; i < keys.size(); ++i) {
        if (keys[i].toString() != key)
            continue;
        setDraftColorIndex(i);
        setDraftColor(color);
        return;
    }
}

void StarshipController::setPreviewScale(double scale)
{
    if (m_previewScale == scale)
        return;
    m_previewScale = scale;
    emit previewScaleChanged();
    regeneratePreview();
    regenerateCurrentPreview();
}

void StarshipController::regeneratePreview()
{
    // Draft preview of the selected source document (template, or the current
    // config when no template is selected): the document's own format and
    // modules with the draft palettes substituted in. Colors follow the
    // document's `palette` key (paletteIndex -1), as in the original app.
    QString html;
    if (!m_doc.empty()) {
        toml::table doc = m_doc;
        if (!m_paletteNames.isEmpty())
            // Replace the palettes table but keep the document's own key.
            tools::replacePalettesInDoc(doc, draftPalettes(), tools::getActivePaletteName(m_doc));
        html = tools::starshipPreviewHtml(doc, -1, m_previewScale, 800);
    }

    if (m_previewHtml != html) {
        m_previewHtml = html;
        emit previewHtmlChanged();
    }
}

void StarshipController::regenerateCurrentPreview()
{
    // Renders the file on disk as-is: no draft palettes applied, and the
    // file's own active palette selects the colors (paletteIndex -1).
    QString html;
    if (m_configValid) {
        try {
            const toml::table doc = toml::parse_file(m_configPath.toStdString());
            html = tools::starshipPreviewHtml(doc, -1, m_previewScale, 800);
        } catch (const std::exception &) {
            html.clear();
        }
    }

    if (m_currentPreviewHtml != html) {
        m_currentPreviewHtml = html;
        emit currentPreviewHtmlChanged();
    }
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

tools::PaletteMap StarshipController::draftPalettes() const
{
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
    return palettes;
}