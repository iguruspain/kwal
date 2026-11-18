#pragma once

#include "tools/ulauncherpreview.h"

#include <KConfigGroup>

#include <QMap>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <QtQml/qqmlregistration.h>


// Ulauncher themes: list templates/user themes, edit their colors, preview them,
// save/apply/create/delete. Preview mockup settings persist via a KConfig group.
class UlauncherController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Combobox list of {name, path, isTemplate, isCurrent} maps.
    Q_PROPERTY(QVariantList themeEntries READ themeEntries NOTIFY themeEntriesChanged)
    Q_PROPERTY(int selectedThemeIndex READ selectedThemeIndex NOTIFY selectedThemeIndexChanged)
    Q_PROPERTY(QString currentThemePath READ currentThemePath NOTIFY currentThemePathChanged)
    // Editable color sections: list of {title, colors: [{key, value, display}]} maps.
    Q_PROPERTY(QVariantList paletteSections READ paletteSections NOTIFY paletteSectionsChanged)
    // Launcher mockup of the selected theme, unapplied edits included.
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewHtmlChanged)
    // Launcher mockup of the applied (active) theme.
    Q_PROPERTY(QString currentPreviewHtml READ currentPreviewHtml NOTIFY currentPreviewHtmlChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)
    // Launcher window width in the mockups, clamped to 400..1200.
    Q_PROPERTY(int previewWindowWidth READ previewWindowWidth NOTIFY previewWindowWidthChanged)
    // Mockup input text; its characters are what gets highlighted in the row names.
    Q_PROPERTY(QString previewQuery READ previewQuery NOTIFY previewQueryChanged)
    // Mockup sample rows: list of {icon, name, shortcut, selected} maps.
    Q_PROPERTY(QVariantList sampleRows READ sampleRows NOTIFY sampleRowsChanged)

public:
    explicit UlauncherController(QObject *parent = nullptr);
    UlauncherController(const KConfigGroup &config, QObject *parent = nullptr);

    QVariantList themeEntries() const;
    int selectedThemeIndex() const;
    QString currentThemePath() const;
    QVariantList paletteSections() const;
    QString previewHtml() const;
    QString currentPreviewHtml() const;
    bool hasBackup() const;
    int previewWindowWidth() const;
    QString previewQuery() const;
    QVariantList sampleRows() const;

    // Points the controller at a specific theme directory (used by tests).
    void setThemePath(const QString &path);

public slots:
    // Rebuilds the list, keeping the selection (fallback: current theme, then first).
    void refresh();
    // Reload only when the watched files changed on disk (matugen or an editor
    // rewrites them); drafts in the selected theme survive unless its own files changed.
    void refreshIfChanged();
    void selectTheme(int index);
    // Re-selects the currently applied theme (or the first entry).
    void clearSelection();
    // Edits one color by key (a manifest row key or a theme.css variable name).
    void setColorValue(const QString &key, const QString &color);
    void saveTheme();
    // Makes the selected theme active in Ulauncher's settings.json.
    void applyToUlauncher();
    // Creates a new user theme from the selected one.
    void createTheme(const QString &name);
    void deleteTheme();
    // Restores the selected theme from its .bak backups.
    void restoreBackup();
    // Render scale for both mockups, clamped to 0.2..2.0; supplied by the QML mockups.
    void setPreviewScale(double scale);
    // Mockup launcher width, clamped to 400..1200; regenerates and persists.
    void setPreviewWindowWidth(int width);
    // Mockup input text; regenerates and persists.
    void setPreviewQuery(const QString &query);
    // Edits one sample row in place. No signal: the settings list shows the values already.
    void setSampleRow(int index, const QVariantMap &row);
    void addSampleRow();
    void removeSampleRow(int index);
signals:
    void themeEntriesChanged();
    void selectedThemeIndexChanged();
    void currentThemePathChanged();
    void paletteSectionsChanged();
    void previewHtmlChanged();
    void currentPreviewHtmlChanged();
    void hasBackupChanged();
    void previewWindowWidthChanged();
    void previewQueryChanged();
    void sampleRowsChanged();
    void operationResult(bool success, QString message);

private:
    struct ThemeEntry {
        QString name;
        QString path;
        bool isTemplate = false;
        bool isCurrent = false;
    };

    void rebuildThemeEntries();
    // The theme dir's manifest.json + theme.css pair (watchedFiles() adds settings.json).
    static QStringList themeFiles(const QString &themeDir);
    QStringList watchedFiles() const;
    // Re-take fingerprints after every read/self-write: only external changes count.
    void armFingerprints();
    // Index of the listed entry whose directory is path, or -1.
    int entryIndexForPath(const QString &path) const;
    void selectEntry(int index);
    void selectIndex(int index);
    void loadTheme();
    void rebuildSections();
    void regeneratePreview();
    void regenerateCurrentPreview();
    void updateHasBackup();
    QString displayColorFor(const QString &key) const;
    void loadPreviewConfig();
    void savePreviewConfig();
    void regeneratePreviews();

    QList<ThemeEntry> m_entries;
    int m_selectedIndex = -1;
    QString m_selectedPath;
    QString m_currentPath;
    // Fingerprints of watchedFiles()/themeFiles(m_selectedPath) as of the last read.
    QString m_watchFingerprint;
    QString m_selectedFingerprint;
    QString m_cssContent;
    QString m_manifestJson;
    QMap<QString, QString> m_themeColors;
    QMap<QString, QString> m_manifestColors;
    // Row key -> real manifest key: root key, or nested under matched_text_hl_colors.
    QMap<QString, QString> m_manifestRowKey;
    QStringList m_themeColorOrder;
    double m_previewScale = 0.75;
    int m_previewWindowWidth = kDefaultPreviewWidth;
    QString m_query;
    QList<tools::UlauncherRow> m_rows;
    // Invalid unless built with a config group; then preview settings persist to it.
    KConfigGroup m_config;
    static constexpr int kDefaultPreviewWidth = 900;
    static constexpr int kMinPreviewWidth = 400;
    static constexpr int kMaxPreviewWidth = 1200;
    QVariantList m_sections;
    QString m_previewHtml;
    QString m_currentPreviewHtml;
    bool m_hasBackup = false;
};
