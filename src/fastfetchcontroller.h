#pragma once

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Fastfetch integration: tint a selected image and point the fastfetch
// config at the result (or at the original image).
class FastfetchController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString configImage READ configImage NOTIFY configImageChanged)
    Q_PROPERTY(QString configImageUrl READ configImageUrl NOTIFY configImageChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QString draftColor READ draftColor WRITE setDraftColor NOTIFY draftColorChanged)
    Q_PROPERTY(QString tintedPreviewPath READ tintedPreviewPath NOTIFY tintedPreviewChanged)
    Q_PROPERTY(QString tintedPreviewUrl READ tintedPreviewUrl NOTIFY tintedPreviewChanged)
    Q_PROPERTY(bool hasBackup READ hasBackup NOTIFY hasBackupChanged)
    // True only after the user picked an image; false for the auto-adopted
    // config image. Drives the selected/tinted preview visibility.
    Q_PROPERTY(bool manuallySelected READ manuallySelected NOTIFY manuallySelectedChanged)
    // True while the full-resolution tint of applyTintedImage is running
    // on the thread pool; the QML buttons show a spinner and stay disabled.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)


public:
    explicit FastfetchController(QObject *parent = nullptr);
    QString configImage() const;
    // file:// URL of configImage with a cache-buster, for QML Image sources.
    QString configImageUrl() const;
    QString selectedFile() const;
    QString draftColor() const;
    void setDraftColor(const QString &color);
    QString tintedPreviewPath() const;
    // file:// URL of tintedPreviewPath with a cache-buster, for QML Image
    // sources; empty when no preview exists.
    QString tintedPreviewUrl() const;
    bool hasBackup() const;
    bool manuallySelected() const;
    // True while the full-resolution apply-tint job is running.
    bool busy() const;
    // file:// URL (percent-encoded) for a local path; for QML Image sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;
    // Path the applied tinted copy of srcPath will be written to.
    Q_INVOKABLE QString tintedImagePathFor(const QString &srcPath) const;

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
    void manuallySelectedChanged();
    void busyChanged();
    void operationResult(bool success, const QString &message);

private:
    // Pools the ImageMagick passes off the UI thread; the result is posted
    // back to the owning thread, where the state is updated.
    class TintJob;
    void setBusy(bool value);
    void finishPreviewTint(bool ok, const QString &src, const QString &dst);
    void finishApplyTint(bool ok, const QString &src, const QString &dst);
    void setConfigImage(const QString &path);
    void setSelectedFile(const QString &path);
    void setTintedPreviewPath(const QString &path);
    void setHasBackup(bool has);
    void setManuallySelected(bool manual);
    void finishApply(const QString &appliedPath);
    // Adopt the image the config points at as the selection unless the
    // user picked one manually.
    void adoptConfigImage();
    QString m_configImage;
    int m_configRevision = 0;
    QString m_selectedFile;
    bool m_autoSelected = false;
    bool m_manuallySelected = false;
    QString m_draftColor = QStringLiteral("transparent");
    QString m_tintedPreviewPath;
    int m_tintedPreviewRevision = 0;
    bool m_hasBackup = false;
    // Tint jobs (main thread only; the pool threads touch nothing else).
    QThreadPool m_pool;
    QString m_previewPath;
    bool m_previewRunning = false;
    // A color change arrived while a preview job ran: re-run with the
    // latest color once the running job finishes.
    bool m_previewPending = false;
    bool m_applyRunning = false;
};
