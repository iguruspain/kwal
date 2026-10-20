#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include "tools/starshipconfig.h"
#include <QtQml/qqmlregistration.h>
#include <toml++/toml.hpp>

// Starship prompt theme controller: manages template selection, palette editing,
// live preview, and applying/restoring the starship.toml configuration.
class StarshipController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString configPath READ configPath NOTIFY configPathChanged)
    Q_PROPERTY(QString selectedTemplate READ selectedTemplate NOTIFY selectedTemplateChanged)
    Q_PROPERTY(int templateIndex READ templateIndex WRITE setTemplateIndex NOTIFY templateIndexChanged)
    Q_PROPERTY(QString draftColor READ draftColor WRITE setDraftColor NOTIFY draftColorChanged)
    Q_PROPERTY(int draftPaletteIndex READ draftPaletteIndex WRITE setDraftPaletteIndex NOTIFY draftPaletteIndexChanged)
    Q_PROPERTY(int draftColorIndex READ draftColorIndex WRITE setDraftColorIndex NOTIFY draftColorIndexChanged)
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewHtmlChanged)
    Q_PROPERTY(QString currentPreviewHtml READ currentPreviewHtml NOTIFY currentPreviewHtmlChanged)
    Q_PROPERTY(double previewScale READ previewScale NOTIFY previewScaleChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)
    Q_PROPERTY(bool hasConfig READ hasConfig NOTIFY hasConfigChanged)
    Q_PROPERTY(QStringList templateNames READ templateNames NOTIFY templateNamesChanged)
    Q_PROPERTY(QVariantList paletteNames READ paletteNames NOTIFY paletteNamesChanged)
    Q_PROPERTY(QVariantList paletteValues READ paletteValues NOTIFY paletteValuesChanged)
    Q_PROPERTY(QVariantList paletteKeys READ paletteKeys NOTIFY paletteKeysChanged)

public:
    explicit StarshipController(QObject *parent = nullptr);

    QString configPath() const;
    QString selectedTemplate() const;
    int templateIndex() const;
    void setTemplateIndex(int index);
    QString draftColor() const;
    void setDraftColor(const QString &color);
    int draftPaletteIndex() const;
    void setDraftPaletteIndex(int index);
    int draftColorIndex() const;
    void setDraftColorIndex(int index);
    QString previewHtml() const;
    QString currentPreviewHtml() const;
    double previewScale() const;
    bool hasBackup() const;
    bool hasConfig() const;
    QStringList templateNames() const;
    QVariantList paletteNames() const;
    QVariantList paletteValues() const;
    QVariantList paletteKeys() const;

public slots:
    // Re-read the current starship.toml and update palettes/preview.
    void refresh();
    // Load a template by name (from ~/.config/kwal/templates/starship).
    void loadTemplate(const QString &name);
    // Clear template selection, go back to editing the current config.
    void clearSelection();
    // Apply the current draft palette to ~/.config/starship.toml (creates .bak on first write).
    void applyConfig();
    // Restore starship.toml from its .bak backup.
    void restoreBackup();
    // Edits one color row of the draft palette by key (mirrors the QML rows).
    void setColorValue(const QString &key, const QString &color);
    // Updates the preview scaling factor (drives both previews' HTML).
    void setPreviewScale(double scale);

signals:
    void configPathChanged();
    void selectedTemplateChanged();
    void templateIndexChanged();
    void draftColorChanged();
    void draftPaletteIndexChanged();
    void draftColorIndexChanged();
    void previewHtmlChanged();
    void currentPreviewHtmlChanged();
    void previewScaleChanged();
    void hasBackupChanged();
    void hasConfigChanged();
    void templateNamesChanged();
    void paletteNamesChanged();
    void paletteValuesChanged();
    void paletteKeysChanged();
    // User-facing result of apply/restore/clear operations.
    void operationResult(bool success, const QString &message);

private:
    void regeneratePreview();
    void regenerateCurrentPreview();
    void updateHasBackup();
    void updateHasConfig();
    void loadTemplateList();
    tools::PaletteMap draftPalettes() const;

    QString m_configPath;
    QString m_selectedTemplate;
    int m_templateIndex = -1;           // -1 = current config, >=0 = template index
    QString m_draftColor = QStringLiteral("transparent");
    int m_draftPaletteIndex = 0;
    int m_draftColorIndex = 0;
    QString m_previewHtml;
    QString m_currentPreviewHtml;
    bool m_hasBackup = false;
    bool m_hasConfig = false;
    QStringList m_templateNames;
    QStringList m_paletteNames;
    QList<QVariantList> m_paletteValues;  // list of string lists
    QList<QVariantList> m_paletteKeys;    // list of string lists
    toml::table m_doc;                    // parsed source doc (config or template)
    double m_previewScale = 1.0;
    bool m_configValid = false;
};