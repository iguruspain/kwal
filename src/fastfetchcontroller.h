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
    // True while the apply step (writing the tinted copy / restoring the
    // backup) runs; the QML buttons show a spinner and stay disabled.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // True while the tinted-preview generation pass runs; the Tinted
    // preview area shows a spinner over the last preview while it does.
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY previewBusyChanged)


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
    // True while the apply step is running.
    bool busy() const;
    // True while a tinted-preview generation pass runs.
    bool previewBusy() const;
    // file:// URL (percent-encoded) for a local path; for QML Image sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;
    // Path the applied tinted copy of srcPath will be written to.
    Q_INVOKABLE QString tintedImagePathFor(const QString &srcPath) const;

public slots:
    // Re-read the image referenced by the fastfetch config.
    void refreshConfigImage();
    void selectFile(const QString &path);
    void clearSelection();
    // Full-resolution tint of the selected image with draftColor, written to
    // the preview cache; this same file is what applyTintedImage copies.
    void generateTintedPreview();
    // Points the fastfetch config at the selected image, without tinting.
    void applyOriginalImage();
    // Copies the generated preview to a "tinted" subfolder next to the
    // original (regenerating it first if needed) and points the config at
    // the copy — the tint pass is never repeated for the apply.
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
    void previewBusyChanged();
    void operationResult(bool success, const QString &message);

private:
    // Pools the ImageMagick passes off the UI thread; the result is posted
    // back to the owning thread, where the state is updated.
    class TintJob;
    void setBusy(bool value);
    void setPreviewBusy(bool value);
    void finishPreviewTint(bool ok, const QString &src, const QString &dst);
    void copyPreviewToTinted();
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
    // True while the file at m_previewPath is the tint of the *current*
    // selection with the *current* color (false from the moment a newer
    // pass starts or a pass fails).
    bool m_previewValid = false;
    // An apply requested while the preview pass was not up to date: copy
    // the fresh result when that pass lands instead of re-tinting.
    bool m_applyAfterPreview = false;
    // A color change arrived while a preview job ran: re-run with the
    // latest color once the running job finishes.
    bool m_previewPending = false;
    bool m_applyRunning = false;
};
