#include "ulaunchercontroller.h"

#include "templateinstaller.h"
#include "tools/ulauncherpreview.h"
#include "tools/ulaunchertheme.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

#include <map>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("UlauncherController", text);
}

std::map<QString, QString> liveColors(const tools::ColorMap &themeColors,
                                      const tools::ColorMap &manifestColors)
{
    std::map<QString, QString> live;
    for (auto it = themeColors.constBegin(); it != themeColors.constEnd(); ++it)
        live[it.key()] = it.value();
    for (auto it = manifestColors.constBegin(); it != manifestColors.constEnd(); ++it)
        if (!it.value().contains(QStringLiteral("provisional")))
            live[it.key()] = it.value();

    // The preview resolves highlight colors from the matched_text_hl_colors_*
    // variables; the manifest when_selected / when_not_selected map onto them.
    const std::pair<QString, QString> hlPairs[] = {
        {QStringLiteral("when_selected"), QStringLiteral("matched_text_hl_colors_when_selected")},
        {QStringLiteral("when_not_selected"), QStringLiteral("matched_text_hl_colors_when_not_selected")},
    };
    for (const auto &pair : hlPairs) {
        const auto it = manifestColors.find(pair.first);
        if (it != manifestColors.constEnd()
            && !it.value().trimmed().isEmpty()
            && !it.value().contains(QStringLiteral("provisional")))
            live[pair.second] = it.value();
    }
    return live;
}

} // namespace

UlauncherController::UlauncherController(QObject *parent)
    : QObject(parent)
{
    m_previewWindowWidth = tools::ulauncherBaseWidth();
    refresh();
}

QVariantList UlauncherController::themeEntries() const
{
    QVariantList list;
    for (const ThemeEntry &entry : m_entries) {
        QVariantMap map;
        map[QStringLiteral("name")] = entry.name;
        map[QStringLiteral("path")] = entry.path;
        map[QStringLiteral("isTemplate")] = entry.isTemplate;
        map[QStringLiteral("isCurrent")] = entry.isCurrent;
        list.append(map);
    }
    return list;
}

int UlauncherController::selectedThemeIndex() const
{
    return m_selectedIndex;
}

QString UlauncherController::currentThemePath() const
{
    return m_currentPath;
}

QVariantList UlauncherController::paletteSections() const
{
    return m_sections;
}

QString UlauncherController::previewHtml() const
{
    return m_previewHtml;
}

QString UlauncherController::currentPreviewHtml() const
{
    return m_currentPreviewHtml;
}

bool UlauncherController::hasBackup() const
{
    return m_hasBackup;
}

void UlauncherController::setThemePath(const QString &path)
{
    m_selectedPath = path;
    m_selectedIndex = -1;
    emit selectedThemeIndexChanged();
    loadTheme();
    rebuildSections();
    regeneratePreview();
    updateHasBackup();
}

void UlauncherController::refresh()
{
    const QString keep = m_selectedPath;
    rebuildThemeEntries();

    int index = -1;
    if (!keep.isEmpty())
        for (int i = 0; i < m_entries.size(); ++i)
            if (m_entries[i].path == keep) {
                index = i;
                break;
            }
    if (index < 0)
        for (int i = 0; i < m_entries.size(); ++i)
            if (m_entries[i].isCurrent) {
                index = i;
                break;
            }
    if (index < 0)
        index = 0;

    selectEntry(index);
    regenerateCurrentPreview();
}

void UlauncherController::selectTheme(int index)
{
    selectIndex(index);
}

void UlauncherController::clearSelection()
{
    rebuildThemeEntries();
    int index = -1;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].isCurrent) {
            index = i;
            break;
        }
    if (index < 0)
        index = 0;
    selectEntry(index);
    regenerateCurrentPreview();
}

void UlauncherController::setColorValue(const QString &key, const QString &color)
{
    bool changed = false;
    if (m_manifestRowKey.contains(key)) {
        m_manifestColors[m_manifestRowKey[key]] = color;
        changed = true;
    } else if (m_themeColors.contains(key)) {
        m_themeColors[key] = color;
        changed = true;
    }
    if (!changed)
        return;
    rebuildSections();
    regeneratePreview();
}

void UlauncherController::saveTheme()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_entries.size()) {
        emit operationResult(false, tr("No theme selected."));
        return;
    }
    const ThemeEntry entry = m_entries[m_selectedIndex];
    if (entry.isTemplate) {
        emit operationResult(false,
                            tr("Cannot save changes to a template. Create a new theme instead."));
        return;
    }
    for (auto it = m_themeColors.constBegin(); it != m_themeColors.constEnd(); ++it)
        if (it.value().contains(QStringLiteral("provisional"))) {
            emit operationResult(false,
                                tr("Theme contains provisional values. Set all colors first."));
            return;
        }
    for (auto it = m_manifestColors.constBegin(); it != m_manifestColors.constEnd(); ++it)
        if (it.value().contains(QStringLiteral("provisional"))) {
            emit operationResult(false,
                                tr("Theme contains provisional values. Set all colors first."));
            return;
        }

    const QString error = tools::applyTheme(m_selectedPath, m_manifestColors, m_themeColors);
    if (!error.isEmpty()) {
        emit operationResult(false, error);
        return;
    }

    updateHasBackup();
    loadTheme();
    rebuildSections();
    regeneratePreview();
    if (entry.isCurrent)
        regenerateCurrentPreview();
    emit operationResult(true, tr("Theme saved."));
}

void UlauncherController::applyToUlauncher()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_entries.size()) {
        emit operationResult(false, tr("No theme selected."));
        return;
    }
    const ThemeEntry entry = m_entries[m_selectedIndex];
    if (entry.isTemplate) {
        emit operationResult(false,
                            tr("Cannot apply a template. Create a new theme from it first."));
        return;
    }
    if (entry.isCurrent) {
        emit operationResult(true, tr("Theme is already the active theme."));
        return;
    }

    const QString error = tools::setActiveThemePath(m_selectedPath);
    if (!error.isEmpty()) {
        emit operationResult(false, error);
        return;
    }

    rebuildThemeEntries();
    regenerateCurrentPreview();
    emit operationResult(true, tr("Theme applied to Ulauncher."));
}

void UlauncherController::createTheme(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        emit operationResult(false, tr("Please provide a name for the new theme."));
        return;
    }
    if (m_selectedPath.isEmpty()) {
        emit operationResult(false, tr("No source theme selected."));
        return;
    }

    const QString newPath = tools::createTheme(m_selectedPath, trimmed);
    if (newPath.isEmpty()) {
        emit operationResult(false, tr("Failed to create theme. The name may already be in use."));
        return;
    }

    rebuildThemeEntries();
    int index = -1;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].path == newPath) {
            index = i;
            break;
        }
    if (index < 0) {
        emit operationResult(false, tr("Failed to select the new theme."));
        return;
    }
    selectIndex(index);
    regenerateCurrentPreview();
    emit operationResult(true, tr("Theme '%1' created.").arg(trimmed));
}

void UlauncherController::deleteTheme()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_entries.size()) {
        emit operationResult(false, tr("No theme selected."));
        return;
    }
    const ThemeEntry entry = m_entries[m_selectedIndex];
    if (entry.isTemplate) {
        emit operationResult(false, tr("Cannot delete a template."));
        return;
    }
    if (entry.isCurrent) {
        emit operationResult(false, tr("Cannot delete the active theme."));
        return;
    }

    if (!tools::deleteTheme(m_selectedPath, m_currentPath)) {
        emit operationResult(false, tr("Failed to delete theme."));
        return;
    }

    rebuildThemeEntries();
    int index = -1;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].isCurrent) {
            index = i;
            break;
        }
    if (index < 0)
        index = 0;
    selectEntry(index);
    regenerateCurrentPreview();
    emit operationResult(true, tr("Theme deleted."));
}

void UlauncherController::restoreBackup()
{
    if (m_selectedPath.isEmpty()) {
        emit operationResult(false, tr("No theme selected."));
        return;
    }
    if (tools::restoreThemeBackup(m_selectedPath)) {
        bool isCurrent = false;
        if (m_selectedIndex >= 0 && m_selectedIndex < m_entries.size())
            isCurrent = m_entries[m_selectedIndex].isCurrent;
        loadTheme();
        rebuildSections();
        regeneratePreview();
        updateHasBackup();
        if (isCurrent)
            regenerateCurrentPreview();
        emit operationResult(true, tr("Theme restored from backup."));
    } else {
        emit operationResult(false, tr("No backup found to restore."));
    }
}

void UlauncherController::setPreviewScale(double scale)
{
    const double clamped = qBound(0.2, scale, 2.0);
    if (qFuzzyCompare(clamped, m_previewScale))
        return;
    m_previewScale = clamped;
    regeneratePreview();
    regenerateCurrentPreview();
}

int UlauncherController::previewWindowWidth() const
{
    return m_previewWindowWidth;
}

void UlauncherController::setPreviewWindowWidth(int width)
{
    const int clamped = qBound(400, width, 1200);
    if (clamped == m_previewWindowWidth)
        return;
    m_previewWindowWidth = clamped;
    emit previewWindowWidthChanged();
    regeneratePreview();
    regenerateCurrentPreview();
}

void UlauncherController::rebuildThemeEntries()
{
    m_entries.clear();

    const QStringList templateDirs =
        tools::listThemeDirs(tools::templateDir() + QStringLiteral("/ulauncher"));
    for (const QString &path : templateDirs)
        m_entries.append({QString(), path, true, false});
    const QStringList userDirs = tools::listThemeDirs(tools::userThemesDir());
    for (const QString &path : userDirs)
        m_entries.append({QString(), path, false, false});

    m_currentPath = tools::resolveActiveThemePath();
    const QString current = QFileInfo(m_currentPath).absoluteFilePath();
    for (ThemeEntry &entry : m_entries)
        entry.isCurrent = !current.isEmpty() && entry.path == current;

    // The applied theme may live outside the listed roots; make sure it is
    // always selectable.
    if (!m_currentPath.isEmpty()) {
        bool found = false;
        for (const ThemeEntry &entry : m_entries)
            if (entry.isCurrent) {
                found = true;
                break;
            }
        if (!found)
            m_entries.prepend({QString(), m_currentPath, false, true});
    }

    for (ThemeEntry &entry : m_entries) {
        QString name = QFileInfo(entry.path).fileName();
        if (entry.isTemplate)
            name += tr(" (Template)");
        if (entry.isCurrent)
            name += tr(" (Current)");
        entry.name = name;
    }

    emit themeEntriesChanged();
    emit currentThemePathChanged();
}

void UlauncherController::selectEntry(int index)
{
    if (m_entries.isEmpty()) {
        m_selectedIndex = -1;
        m_selectedPath.clear();
        emit selectedThemeIndexChanged();
        loadTheme();
        rebuildSections();
        regeneratePreview();
        updateHasBackup();
        return;
    }
    if (index < 0)
        index = 0;
    selectIndex(index);
}

void UlauncherController::selectIndex(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_selectedIndex = index;
    m_selectedPath = m_entries[index].path;
    emit selectedThemeIndexChanged();
    loadTheme();
    rebuildSections();
    regeneratePreview();
    updateHasBackup();
}

void UlauncherController::loadTheme()
{
    m_cssContent.clear();
    m_manifestJson.clear();
    m_themeColors.clear();
    m_manifestColors.clear();
    m_manifestRowKey.clear();
    m_themeColorOrder.clear();

    if (m_selectedPath.isEmpty())
        return;

    const QDir dir(m_selectedPath);
    QFile cssFile(dir.filePath(QStringLiteral("theme.css")));
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        m_cssContent = QString::fromUtf8(cssFile.readAll());
    QFile manifestFile(dir.filePath(QStringLiteral("manifest.json")));
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
        m_manifestJson = QString::fromUtf8(manifestFile.readAll());

    m_themeColors = tools::extractCssColors(m_cssContent);
    m_manifestColors = tools::parseManifestColors(m_manifestJson);

    // Map each manifest row to the key that actually holds its value: a root
    // key when present, otherwise its matched_text_hl_colors entry.
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(m_manifestJson.toUtf8(), &error);
    const QJsonObject obj = !error.error && doc.isObject() ? doc.object() : QJsonObject();
    const QStringList rows = {QStringLiteral("when_selected"), QStringLiteral("when_not_selected")};
    for (const QString &row : rows)
        m_manifestRowKey[row] = obj.contains(row)
            ? row
            : QStringLiteral("matched_text_hl_colors_") + row;

    m_themeColorOrder = tools::cssColorOrder(m_cssContent);
}

void UlauncherController::rebuildSections()
{
    m_sections.clear();

    QVariantList manifestColors;
    const QStringList rows = {QStringLiteral("when_selected"), QStringLiteral("when_not_selected")};
    for (const QString &row : rows) {
        const QString rowKey = m_manifestRowKey.value(row);
        if (rowKey.isEmpty())
            continue;
        QVariantMap entry;
        entry[QStringLiteral("key")] = row;
        entry[QStringLiteral("value")] = m_manifestColors.value(rowKey);
        entry[QStringLiteral("display")] = displayColorFor(rowKey);
        manifestColors.append(entry);
    }
    QVariantMap manifestSection;
    manifestSection[QStringLiteral("title")] = tr("Manifest Colors");
    manifestSection[QStringLiteral("colors")] = manifestColors;
    m_sections.append(manifestSection);

    QVariantList themeColors;
    for (const QString &name : m_themeColorOrder) {
        const QString raw = m_themeColors.value(name);
        QVariantMap entry;
        entry[QStringLiteral("key")] = name;
        entry[QStringLiteral("value")] = raw;
        entry[QStringLiteral("display")] = displayColorFor(name);
        themeColors.append(entry);
    }
    QVariantMap themeSection;
    themeSection[QStringLiteral("title")] = tr("Theme Colors");
    themeSection[QStringLiteral("colors")] = themeColors;
    m_sections.append(themeSection);

    emit paletteSectionsChanged();
}

void UlauncherController::regeneratePreview()
{
    m_previewHtml = tools::ulauncherPreviewHtml(m_selectedPath,
                                                liveColors(m_themeColors, m_manifestColors),
                                                m_previewScale,
                                                m_previewWindowWidth,
                                                m_manifestJson,
                                                m_cssContent);
    emit previewHtmlChanged();
}

void UlauncherController::regenerateCurrentPreview()
{
    QString css, manifest;
    if (!m_currentPath.isEmpty()) {
        const QDir dir(m_currentPath);
        QFile cssFile(dir.filePath(QStringLiteral("theme.css")));
        if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text))
            css = QString::fromUtf8(cssFile.readAll());
        QFile manifestFile(dir.filePath(QStringLiteral("manifest.json")));
        if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
            manifest = QString::fromUtf8(manifestFile.readAll());
    }

    m_currentPreviewHtml = m_currentPath.isEmpty()
        ? QString()
        : tools::ulauncherPreviewHtml(m_currentPath,
                                      liveColors(tools::extractCssColors(css),
                                                 tools::parseManifestColors(manifest)),
                                      m_previewScale,
                                      m_previewWindowWidth,
                                      manifest,
                                      css);
    emit currentPreviewHtmlChanged();
}

void UlauncherController::updateHasBackup()
{
    const bool backup = !m_selectedPath.isEmpty() && tools::hasThemeBackup(m_selectedPath);
    if (m_hasBackup != backup) {
        m_hasBackup = backup;
        emit hasBackupChanged();
    }
}

QString UlauncherController::displayColorFor(const QString &key) const
{
    QString raw;
    if (m_themeColors.contains(key))
        raw = m_themeColors[key];
    else if (m_manifestColors.contains(key))
        raw = m_manifestColors[key];
    raw = raw.trimmed();

    if (raw.startsWith(QLatin1Char('@')) && raw.size() > 1) {
        const QString other = raw.mid(1);
        if (m_themeColors.contains(other))
            raw = m_themeColors[other].trimmed();
        else if (m_manifestColors.contains(other))
            raw = m_manifestColors[other].trimmed();
    }

    if (raw.isEmpty() || raw.startsWith(QStringLiteral("provisional_"))
        || raw == QStringLiteral("transparent"))
        return QStringLiteral("transparent");
    const QColor c(raw);
    if (!c.isValid())
        return QStringLiteral("transparent");
    return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}
