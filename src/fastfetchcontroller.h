#pragma once

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Tints a selected image and points the fastfetch config at the result.
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
    // Manual pick, or a tint pass running/landed on the auto-adopted config image.
    Q_PROPERTY(bool previewsVisible READ previewsVisible NOTIFY previewsVisibilityChanged)
    // Apply step (tinted copy / backup restore) in flight.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Tinted-preview generation pass in flight.
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
    // file:// URL of tintedPreviewPath with a cache-buster; empty without a preview.
    QString tintedPreviewUrl() const;
    bool hasBackup() const;
    bool previewsVisible() const;
    bool busy() const;
    bool previewBusy() const;
    // Percent-encoded file:// URL, for QML Image sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;
    Q_INVOKABLE QString tintedImagePathFor(const QString &srcPath) const;

public slots:
    void refreshConfigImage();
    void selectFile(const QString &path);
    void clearSelection();
    // Full-res tint with draftColor into the preview cache; apply copies this exact file.
    void generateTintedPreview();
    // Points the config at the selected image, untinted.
    void applyOriginalImage();
    // Copies the preview into a "tinted" subfolder next to the original
    // (regenerating it first if stale); the tint pass is never repeated for apply.
    void applyTintedImage();
    void restoreBackup();

signals:
    void configImageChanged();
    void selectedFileChanged();
    void draftColorChanged();
    void tintedPreviewChanged();
    void hasBackupChanged();
    void previewsVisibilityChanged();
    void busyChanged();
    void previewBusyChanged();
    void operationResult(bool success, const QString &message);

private:
    // ImageMagick pass off the UI thread; result posted back to the owning thread.
    class TintJob;
    void setBusy(bool value);
    void setPreviewBusy(bool value);
    void finishPreviewTint(bool ok, const QString &src, const QString &dst);
    void copyPreviewToTinted();
    void setConfigImage(const QString &path);
    void setSelectedFile(const QString &path);
    void setTintedPreviewPath(const QString &path);
    void setHasBackup(bool has);
    // User picked the file (vs. the config image being adopted).
    void setManuallySelected(bool manual);
    void notifyPreviewsVisibility();
    void finishApply(const QString &appliedPath);
    // Adopts the config image as the selection unless the user picked one manually.
    void adoptConfigImage();
    QString m_configImage;
    int m_configRevision = 0;
    QString m_selectedFile;
    bool m_autoSelected = false;
    bool m_manuallySelected = false;
    // Last value announced through previewsVisibilityChanged().
    bool m_previewsVisible = false;
    QString m_draftColor = QStringLiteral("transparent");
    QString m_tintedPreviewPath;
    int m_tintedPreviewRevision = 0;
    bool m_hasBackup = false;
    // Tint jobs; main thread only — pool threads touch nothing else.
    QThreadPool m_pool;
    QString m_previewPath;
    bool m_previewRunning = false;
    // m_previewPath is the current selection+color tint; false on pass start or failure.
    bool m_previewValid = false;
    // Apply requested on a stale preview: copy the fresh result when it lands.
    bool m_applyAfterPreview = false;
    // Color changed mid-job: re-run with the latest color when it lands.
    bool m_previewPending = false;
    bool m_applyRunning = false;
};
