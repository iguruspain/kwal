#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <QtQml/qqmlregistration.h>

// Manages the active Ulauncher theme: exposes its color variables for editing,
// renders a live preview, and applies/restores changes to the theme files.
class UlauncherController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString themePath READ themePath NOTIFY themePathChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeNameChanged)
    Q_PROPERTY(QStringList colorNames READ colorNames NOTIFY colorListChanged)
    Q_PROPERTY(QVariantList colorValues READ colorValues NOTIFY colorValuesChanged)
    Q_PROPERTY(int colorIndex READ colorIndex WRITE setColorIndex NOTIFY colorIndexChanged)
    Q_PROPERTY(QString draftColor READ draftColor WRITE setDraftColor NOTIFY draftColorChanged)
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewHtmlChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)

public:
    explicit UlauncherController(QObject *parent = nullptr);

    QString themePath() const;
    QString themeName() const;
    QStringList colorNames() const;
    QVariantList colorValues() const;
    int colorIndex() const;
    void setColorIndex(int index);
    QString draftColor() const;
    void setDraftColor(const QString &color);
    QString previewHtml() const;
    bool hasBackup() const;

    // Point the controller at a specific theme directory (used by tests and for
    // explicit theme selection); reloads colors and regenerates the preview.
    void setThemePath(const QString &path);

public slots:
    void refresh();
    void applyConfig();
    void restoreBackup();

signals:
    void themePathChanged();
    void themeNameChanged();
    void colorListChanged();
    void colorValuesChanged();
    void colorIndexChanged();
    void draftColorChanged();
    void previewHtmlChanged();
    void hasBackupChanged();
    void operationResult(bool success, QString message);

private:
    void selectActiveTheme();
    void loadColors();
    void regeneratePreview();
    void updateHasBackup();
    QString displayColorFor(const QString &name) const;

    QString m_themePath;
    QString m_themeName;
    QString m_cssContent;
    QString m_manifestJson;
    QMap<QString, QString> m_themeColors;
    QMap<QString, QString> m_manifestColors;
    QStringList m_colorNames;
    QVariantList m_colorValues;
    int m_colorIndex = -1;
    QString m_draftColor = QStringLiteral("transparent");
    QString m_previewHtml;
    bool m_hasBackup = false;

    static const QStringList kCanonicalColors;
};
