#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include "tools/starshipconfig.h"
#include <QtQml/qqmlregistration.h>
#include <toml++/toml.hpp>

// Starship prompt theme: templates, palette editing, live preview, apply/restore.
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
    void refresh();
    // Reload only if starship.toml changed on disk (matugen and friends rewrite it
    // while the app runs); an edited draft stays, only the applied side follows.
    void refreshIfChanged();
    // Loads a template by name from ~/.config/kwal/templates/starship.
    void loadTemplate(const QString &name);
    // Drops the template selection; editing goes back to the live config.
    void clearSelection();
    // Writes the draft palette to ~/.config/starship.toml, backing it up to .bak first.
    void applyConfig();
    void restoreBackup();
    void setColorValue(const QString &key, const QString &color);
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
    // Result of apply/restore/clear, for the user.
    void operationResult(bool success, const QString &message);

private:
    void regeneratePreview();
    void regenerateCurrentPreview();
    void updateHasBackup();
    void updateHasConfig();
    void loadTemplateList();
    // Fingerprint of starship.toml as last read (refresh() re-arms it), so
    // refreshIfChanged() can spot an external rewrite.
    void armConfigFingerprint();
    tools::PaletteMap draftPalettes() const;

    QString m_configPath;
    QString m_configFingerprint;
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
    QList<QVariantList> m_paletteValues;
    QList<QVariantList> m_paletteKeys;
    toml::table m_doc;                    // parsed config or template doc
    double m_previewScale = 1.0;
    bool m_configValid = false;
};