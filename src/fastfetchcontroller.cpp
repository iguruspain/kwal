#include "fastfetchcontroller.h"

#include "tools/fastfetchconfig.h"
#include "tools/fileutil.h"
#include "tools/tintimage.h"

#include <QColor>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("FastfetchController", text);
}

bool parseTint(const QString &text, QColor &out)
{
    const QString hex = text.trimmed();
    if (hex.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0)
        return false;
    if (hex.size() == 7)
        out = QColor(hex);
    else if (hex.size() == 9)
        out = QColor::fromString(hex);
    else
        return false;
    return out.isValid();
}

// Max dimension of the tinted preview; the full-resolution pass is only
// run by the apply.
constexpr int kPreviewMaxDim = 512;

}

// Runs the ImageMagick tint on a pool thread: the pass takes minutes for a
// multi-hundred-frame GIF and must never block the UI thread. The result is
// posted back to the owning thread, where the state is updated.
class FastfetchController::TintJob : public QRunnable
{
public:
    TintJob(QPointer<FastfetchController> controller,
            QString src,
            QString dst,
            QColor tint,
            bool preview)
        : m_controller(std::move(controller))
        , m_src(std::move(src))
        , m_dst(std::move(dst))
        , m_tint(tint)
        , m_preview(preview)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (!m_controller) {
            return;
        }
        const bool ok = m_preview
            ? tools::tintImageTo(m_src, m_dst, m_tint, 0.8, kPreviewMaxDim)
            : tools::tintImageTo(m_src, m_dst, m_tint);
        QMetaObject::invokeMethod(
            m_controller.data(),
            [ctl = m_controller, ok, src = m_src, dst = m_dst, preview = m_preview]() {
                if (!ctl) {
                    return;
                }
                if (preview)
                    ctl->finishPreviewTint(ok, src, dst);
                else
                    ctl->finishApplyTint(ok, src, dst);
            },
            Qt::QueuedConnection);
    }

private:
    QPointer<FastfetchController> m_controller;
    QString m_src;
    QString m_dst;
    QColor m_tint;
    bool m_preview;
};

FastfetchController::FastfetchController(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
    m_configImage = tools::detectConfigImage();
    m_hasBackup = tools::hasConfigBackup();
    adoptConfigImage();
}

QString FastfetchController::configImage() const
{
    return m_configImage;
}

QString FastfetchController::configImageUrl() const
{
    if (m_configImage.isEmpty())
        return QString();
    return fileUrl(m_configImage) + QStringLiteral("?t=%1").arg(m_configRevision);
}

QString FastfetchController::selectedFile() const
{
    return m_selectedFile;
}

QString FastfetchController::draftColor() const
{
    return m_draftColor;
}

void FastfetchController::setDraftColor(const QString &color)
{
    if (m_draftColor == color)
        return;
    m_draftColor = color;
    emit draftColorChanged();
    // Keep the tinted preview in sync with the color, not just the QML dialog.
    if (!m_selectedFile.isEmpty())
        generateTintedPreview();
}

QString FastfetchController::tintedPreviewPath() const
{
    return m_tintedPreviewPath;
}

QString FastfetchController::tintedPreviewUrl() const
{
    if (m_tintedPreviewPath.isEmpty())
        return QString();
    return fileUrl(m_tintedPreviewPath) + QStringLiteral("?t=%1").arg(m_tintedPreviewRevision);
}

bool FastfetchController::hasBackup() const
{
    return m_hasBackup;
}

bool FastfetchController::busy() const
{
    return m_applyRunning;
}

bool FastfetchController::manuallySelected() const
{
    return m_manuallySelected;
}

QString FastfetchController::fileUrl(const QString &path) const
{
    if (path.isEmpty())
        return QString();
    return QUrl::fromLocalFile(path).toString();
}

QString FastfetchController::tintedImagePathFor(const QString &srcPath) const
{
    return tools::tintedImagePath(srcPath);
}

void FastfetchController::refreshConfigImage()
{
    // Bump the cache-buster so the same-path image reloads after external
    // changes, even when detection returns the previous path.
    m_configRevision++;
    setConfigImage(tools::detectConfigImage());
    emit configImageChanged();
    adoptConfigImage();
}

void FastfetchController::adoptConfigImage()
{
    const QString detected = tools::detectConfigImage();
    if (detected.isEmpty()) {
        if (m_autoSelected)
            setSelectedFile(QString());
        return;
    }
    if (m_autoSelected || m_selectedFile.isEmpty()) {
        m_autoSelected = true;
        setSelectedFile(detected);
        setManuallySelected(false);
        generateTintedPreview();
    }
}

void FastfetchController::selectFile(const QString &path)
{
    // Callers may hand over a file:// URL (e.g. QML FileDialog); normalize.
    QString normalized = path;
    if (normalized.startsWith(QLatin1String("file://")))
        normalized = QUrl(normalized).toLocalFile();
    m_autoSelected = false;
    setManuallySelected(!normalized.isEmpty());
    setSelectedFile(normalized);
    generateTintedPreview();
}

void FastfetchController::clearSelection()
{
    m_autoSelected = false;
    setManuallySelected(false);
    setSelectedFile(QString());
    setDraftColor(QStringLiteral("transparent"));
    setTintedPreviewPath(QString());
}

void FastfetchController::generateTintedPreview()
{
    QColor tint;
    if (m_selectedFile.isEmpty() || !parseTint(m_draftColor, tint)) {
        m_previewPending = false;
        setTintedPreviewPath(QString());
        return;
    }
    if (m_previewRunning) {
        // Coalesce: only the latest color matters; the running job
        // re-runs with it when it finishes.
        m_previewPending = true;
        return;
    }
    const QString previewDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/kwal");
    QDir().mkpath(previewDir);
    m_previewPath = previewDir + QStringLiteral("/fastfetch-preview.png");
    m_previewRunning = true;
    m_pool.start(new TintJob(QPointer<FastfetchController>{this}, m_selectedFile, m_previewPath, tint, true));
}

void FastfetchController::finishPreviewTint(bool ok, const QString &src, const QString &dst)
{
    m_previewRunning = false;
    if (src == m_selectedFile) {
        if (ok) {
            // Same cache path, new content: bump the cache-buster so the QML
            // Image source changes and reloads (the path itself does not).
            m_tintedPreviewRevision++;
            setTintedPreviewPath(dst);
            emit tintedPreviewChanged();
        } else {
            setTintedPreviewPath(QString());
        }
    }
    if (m_previewPending) {
        m_previewPending = false;
        generateTintedPreview();
    }
}

void FastfetchController::setBusy(bool value)
{
    if (m_applyRunning != value) {
        m_applyRunning = value;
        emit busyChanged();
    }
}

void FastfetchController::applyOriginalImage()
{
    if (m_applyRunning)
        return; // a tint job still has to write the config when it finishes
    if (m_selectedFile.isEmpty()) {
        emit operationResult(false, tr("No image selected."));
        return;
    }
    if (!QFileInfo::exists(m_selectedFile)) {
        emit operationResult(false, tr("The selected image no longer exists."));
        return;
    }

    tools::backupFile(tools::fastfetchConfigPath());
    setHasBackup(tools::hasConfigBackup());

    if (!tools::setImageSource(m_selectedFile)) {
        emit operationResult(false, tr("Failed to update the fastfetch config."));
        return;
    }
    finishApply(m_selectedFile);
}

void FastfetchController::applyTintedImage()
{
    if (m_applyRunning)
        return;
    if (m_selectedFile.isEmpty()) {
        emit operationResult(false, tr("No image selected."));
        return;
    }

    QColor tint;
    if (!parseTint(m_draftColor, tint)) {
        emit operationResult(false, tr("No valid tint color selected."));
        return;
    }

    setBusy(true);
    const QString tintedPath = tools::tintedImagePath(m_selectedFile);
    // Full resolution on the pool: this file becomes the actual fastfetch
    // logo, and the pass takes minutes for large animated GIFs.
    m_pool.start(new TintJob(QPointer<FastfetchController>{this}, m_selectedFile, tintedPath, tint, false));
}

void FastfetchController::finishApplyTint(bool ok, const QString &src, const QString &dst)
{
    setBusy(false);
    if (src != m_selectedFile) {
        emit operationResult(false, tr("The selection changed while the tint was running."));
        return;
    }
    if (!ok) {
        emit operationResult(false, tr("Failed to generate the tinted image."));
        return;
    }

    tools::backupFile(tools::fastfetchConfigPath());
    setHasBackup(tools::hasConfigBackup());

    if (!tools::setImageSource(dst)) {
        emit operationResult(false, tr("Tinted image saved, but the fastfetch config could not be updated."));
        return;
    }
    finishApply(dst);
}

void FastfetchController::restoreBackup()
{
    if (!tools::restoreConfigBackup()) {
        emit operationResult(false, tr("No backup found to restore."));
        return;
    }
    tools::clearCache();
    setConfigImage(tools::detectConfigImage());
    adoptConfigImage();
    setHasBackup(tools::hasConfigBackup());
    emit operationResult(true, tr("Restored fastfetch config from backup."));
}

void FastfetchController::setConfigImage(const QString &path)
{
    if (m_configImage != path) {
        m_configImage = path;
        emit configImageChanged();
    }
}

void FastfetchController::setSelectedFile(const QString &path)
{
    if (m_selectedFile != path) {
        m_selectedFile = path;
        emit selectedFileChanged();
    }
}

void FastfetchController::setTintedPreviewPath(const QString &path)
{
    if (m_tintedPreviewPath != path) {
        m_tintedPreviewPath = path;
        emit tintedPreviewChanged();
    }
}

void FastfetchController::setHasBackup(bool has)
{
    if (m_hasBackup != has) {
        m_hasBackup = has;
        emit hasBackupChanged();
    }
}

void FastfetchController::setManuallySelected(bool manual)
{
    if (m_manuallySelected != manual) {
        m_manuallySelected = manual;
        emit manuallySelectedChanged();
    }
}

void FastfetchController::finishApply(const QString &appliedPath)
{
    tools::clearCache();
    setConfigImage(appliedPath);
    setHasBackup(true);
    emit operationResult(true, tr("Applied image %1").arg(appliedPath));
}
