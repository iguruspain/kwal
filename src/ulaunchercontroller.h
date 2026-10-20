#pragma once

#include <QMap>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <QtQml/qqmlregistration.h>

// Manages Ulauncher themes: lists templates and user themes, exposes the
// selected theme's colors as editable sections, renders live previews, and
// saves/applies/creates/deletes themes.
class UlauncherController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Theme list for the combobox: list of {name, path, isTemplate, isCurrent} maps.
    Q_PROPERTY(QVariantList themeEntries READ themeEntries NOTIFY themeEntriesChanged)
    Q_PROPERTY(int selectedThemeIndex READ selectedThemeIndex NOTIFY selectedThemeIndexChanged)
    Q_PROPERTY(QString currentThemePath READ currentThemePath NOTIFY currentThemePathChanged)
    // Editable color sections: list of {title, colors: [{key, value, display}]} maps.
    Q_PROPERTY(QVariantList paletteSections READ paletteSections NOTIFY paletteSectionsChanged)
    // Launcher mockup of the selected theme with unapplied edits.
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewHtmlChanged)
    // Launcher mockup of the applied (active) theme.
    Q_PROPERTY(QString currentPreviewHtml READ currentPreviewHtml NOTIFY currentPreviewHtmlChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)

public:
    explicit UlauncherController(QObject *parent = nullptr);

    QVariantList themeEntries() const;
    int selectedThemeIndex() const;
    QString currentThemePath() const;
    QVariantList paletteSections() const;
    QString previewHtml() const;
    QString currentPreviewHtml() const;
    bool hasBackup() const;

    // Point the controller at a specific theme directory (used by tests);
    // reloads colors and regenerates the preview.
    void setThemePath(const QString &path);

public slots:
    // Re-resolves the active theme, rebuilds the theme list, and keeps the
    // selection (falling back to the current theme, then the first entry).
    void refresh();
    // Selects the theme at the given list index for editing.
    void selectTheme(int index);
    // Re-selects the currently applied theme (or the first entry).
    void clearSelection();
    // Edits one color by key (a manifest row key or a theme.css variable name).
    void setColorValue(const QString &key, const QString &color);
    // Writes the edited colors to the selected theme's files.
    void saveTheme();
    // Makes the selected theme active in Ulauncher's settings.json.
    void applyToUlauncher();
    // Creates a new user theme from the selected one.
    void createTheme(const QString &name);
    // Deletes the selected user theme.
    void deleteTheme();
    // Restores the selected theme from its .bak backups.
    void restoreBackup();

signals:
    void themeEntriesChanged();
    void selectedThemeIndexChanged();
    void currentThemePathChanged();
    void paletteSectionsChanged();
    void previewHtmlChanged();
    void currentPreviewHtmlChanged();
    void hasBackupChanged();
    void operationResult(bool success, QString message);

private:
    struct ThemeEntry {
        QString name;
        QString path;
        bool isTemplate = false;
        bool isCurrent = false;
    };

    void rebuildThemeEntries();
    void selectEntry(int index);
    void selectIndex(int index);
    void loadTheme();
    void rebuildSections();
    void regeneratePreview();
    void regenerateCurrentPreview();
    void updateHasBackup();
    QString displayColorFor(const QString &key) const;

    QList<ThemeEntry> m_entries;
    int m_selectedIndex = -1;
    QString m_selectedPath;
    QString m_currentPath;
    QString m_cssContent;
    QString m_manifestJson;
    QMap<QString, QString> m_themeColors;
    QMap<QString, QString> m_manifestColors;
    // Manifest row key ("when_selected" / "when_not_selected") -> the actual
    // manifest key (root key or its matched_text_hl_colors entry).
    QMap<QString, QString> m_manifestRowKey;
    QStringList m_themeColorOrder;
    QVariantList m_sections;
    QString m_previewHtml;
    QString m_currentPreviewHtml;
    bool m_hasBackup = false;
};
