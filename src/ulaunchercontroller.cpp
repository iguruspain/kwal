#include "ulaunchercontroller.h"

#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <map>

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("UlauncherController", text);
}

const QStringList canonicalColors()
{
    return {
        "bg_color",
        "window_bg",
        "window_border_color",
        "prefs_background",
        "selected_bg_color",
        "selected_fg_color",
        "input_color",
        "caret_color",
        "item_name",
        "item_text",
        "item_box_selected",
        "item_text_selected",
        "item_name_selected",
        "item_shortcut_color",
        "item_shortcut_shadow",
        "item_shortcut_color_sel",
        "item_shortcut_shadow_sel",
        "matched_text_hl_colors_when_selected",
        "matched_text_hl_colors_when_not_selected",
    };
}
}

const QStringList UlauncherController::kCanonicalColors = canonicalColors();

UlauncherController::UlauncherController(QObject *parent)
    : QObject(parent)
{
    selectActiveTheme();
    loadColors();
    regeneratePreview();
    updateHasBackup();
}

QString UlauncherController::themePath() const
{
    return m_themePath;
}

QString UlauncherController::themeName() const
{
    return m_themeName;
}

QStringList UlauncherController::colorNames() const
{
    return m_colorNames;
}

QVariantList UlauncherController::colorValues() const
{
    return m_colorValues;
}

int UlauncherController::colorIndex() const
{
    return m_colorIndex;
}

void UlauncherController::setColorIndex(int index)
{
    if (index < 0 || index >= m_colorNames.size())
        return;
    if (m_colorIndex != index) {
        m_colorIndex = index;
        emit colorIndexChanged();
        m_draftColor = displayColorFor(m_colorNames.at(index));
        emit draftColorChanged();
    }
}

QString UlauncherController::draftColor() const
{
    return m_draftColor;
}

void UlauncherController::setDraftColor(const QString &color)
{
    if (m_draftColor == color)
        return;
    m_draftColor = color;
    emit draftColorChanged();

    if (m_colorIndex >= 0 && m_colorIndex < m_colorNames.size()) {
        const QString name = m_colorNames.at(m_colorIndex);
        if (m_themeColors.contains(name))
            m_themeColors[name] = color;
        else
            m_manifestColors[name] = color;
        emit colorValuesChanged();
        regeneratePreview();
    }
}

QString UlauncherController::previewHtml() const
{
    return m_previewHtml;
}

bool UlauncherController::hasBackup() const
{
    return m_hasBackup;
}

void UlauncherController::setThemePath(const QString &path)
{
    if (m_themePath == path)
        return;
    m_themePath = path;
    m_themeName = tr("Custom theme");
    emit themePathChanged();
    emit themeNameChanged();
    loadColors();
    regeneratePreview();
    updateHasBackup();
}

void UlauncherController::refresh()
{
    selectActiveTheme();
    loadColors();
    regeneratePreview();
    updateHasBackup();
}

void UlauncherController::applyConfig()
{
    const QString error = tools::applyTheme(m_themePath, m_manifestColors, m_themeColors);
    if (error.isEmpty()) {
        updateHasBackup();
        m_previewHtml.clear(); // Force the next preview to read the updated files.
        m_cssContent.clear();
        m_manifestJson.clear();
        loadColors();
        regeneratePreview();
        emit operationResult(true, tr("Ulauncher theme applied."));
    } else {
        emit operationResult(false, error);
    }
}

void UlauncherController::restoreBackup()
{
    if (tools::restoreThemeBackup(m_themePath)) {
        loadColors();
        regeneratePreview();
        updateHasBackup();
        emit operationResult(true, tr("Ulauncher theme restored from backup."));
    } else {
        emit operationResult(false, tr("No backup found to restore."));
    }
}

void UlauncherController::selectActiveTheme()
{
    const QString active = tools::resolveActiveThemePath();
    if (!active.isEmpty() && QFileInfo(active).isDir()) {
        m_themePath = active;
        m_themeName = tr("Active theme");
    } else {
        m_themePath = tools::bundledTemplateDir();
        m_themeName = tr("Bundled template");
    }
}

void UlauncherController::loadColors()
{
    m_cssContent.clear();
    m_manifestJson.clear();

    QDir dir(m_themePath);
    QFile cssFile(dir.filePath(QStringLiteral("theme.css")));
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        m_cssContent = QString::fromUtf8(cssFile.readAll());
    QFile manifestFile(dir.filePath(QStringLiteral("manifest.json")));
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
        m_manifestJson = QString::fromUtf8(manifestFile.readAll());

    m_themeColors = tools::extractCssColors(m_cssContent);
    m_manifestColors = tools::parseManifestColors(m_manifestJson);

    m_colorNames = kCanonicalColors;
    m_colorValues.clear();
    for (const QString &name : m_colorNames) {
        QString value;
        if (m_themeColors.contains(name))
            value = m_themeColors[name];
        else if (m_manifestColors.contains(name))
            value = m_manifestColors[name];
        m_colorValues.append(value);
    }
    emit colorListChanged();
    emit colorValuesChanged();

    if (m_colorIndex >= m_colorNames.size()) {
        m_colorIndex = m_colorNames.isEmpty() ? -1 : 0;
        emit colorIndexChanged();
    }
    if (m_colorIndex >= 0 && m_colorIndex < m_colorNames.size())
        m_draftColor = displayColorFor(m_colorNames.at(m_colorIndex));
}

void UlauncherController::regeneratePreview()
{
    std::map<QString, QString> live;
    for (auto it = m_themeColors.constBegin(); it != m_themeColors.constEnd(); ++it)
        live[it.key()] = it.value();
    for (auto it = m_manifestColors.constBegin(); it != m_manifestColors.constEnd(); ++it)
        live[it.key()] = it.value();

    m_previewHtml =
        tools::ulauncherPreviewHtml(m_themePath, live, 0.75, 0, m_manifestJson, m_cssContent);
    emit previewHtmlChanged();
}

void UlauncherController::updateHasBackup()
{
    const bool backup = tools::hasThemeBackup(m_themePath);
    if (m_hasBackup != backup) {
        m_hasBackup = backup;
        emit hasBackupChanged();
    }
}

QString UlauncherController::displayColorFor(const QString &name) const
{
    QString raw;
    if (m_themeColors.contains(name))
        raw = m_themeColors[name];
    else if (m_manifestColors.contains(name))
        raw = m_manifestColors[name];
    raw = raw.trimmed();

    if (raw.startsWith('@') && raw.size() > 1) {
        const QString other = raw.mid(1);
        if (m_themeColors.contains(other))
            raw = m_themeColors[other].trimmed();
        else if (m_manifestColors.contains(other))
            raw = m_manifestColors[other].trimmed();
    }

    if (raw.isEmpty() || raw.startsWith("provisional_") || raw == "transparent")
        return QStringLiteral("transparent");
    const QColor c(raw);
    if (!c.isValid())
        return QStringLiteral("transparent");
    return c.name(QColor::HexRgb);
}
