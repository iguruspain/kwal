#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Fastfetch integration: tint a selected image and point the fastfetch
// config at the result (or at the original image).
class FastfetchController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString configImage READ configImage NOTIFY configImageChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QString draftColor READ draftColor WRITE setDraftColor NOTIFY draftColorChanged)
    Q_PROPERTY(QString tintedPreviewPath READ tintedPreviewPath NOTIFY tintedPreviewChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)

public:
    explicit FastfetchController(QObject *parent = nullptr);

    QString configImage() const;
    QString selectedFile() const;
    QString draftColor() const;
    void setDraftColor(const QString &color);
    QString tintedPreviewPath() const;
    bool hasBackup() const;
    // file:// URL (percent-encoded) for a local path; for QML Image sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;

public slots:
    // Re-read the image referenced by the fastfetch config.
    void refreshConfigImage();
    void selectFile(const QString &path);
    void clearSelection();
    // Tints a scaled copy of the selected image with draftColor.
    void generateTintedPreview();
    // Points the fastfetch config at the selected image, without tinting.
    void applyOriginalImage();
    // Tints the selected image next to the original and points the config at it.
    void applyTintedImage();
    void restoreBackup();

signals:
    void configImageChanged();
    void selectedFileChanged();
    void draftColorChanged();
    void tintedPreviewChanged();
    void hasBackupChanged();
    void operationResult(bool success, const QString &message);

private:
    void setConfigImage(const QString &path);
    void setSelectedFile(const QString &path);
    void setTintedPreviewPath(const QString &path);
    void setHasBackup(bool has);
    void finishApply(const QString &appliedPath);
    // Adopt the image the config points at as the selection unless the
    // user picked one manually.
    void adoptConfigImage();
    QString m_configImage;
    QString m_selectedFile;
    bool m_autoSelected = false;
    QString m_draftColor = QStringLiteral("transparent");
    QString m_tintedPreviewPath;
    bool m_hasBackup = false;
};
