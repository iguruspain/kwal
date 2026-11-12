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

}

// Runs the full-resolution ImageMagick tint on a pool thread: the pass
// takes seconds to minutes for a multi-hundred-frame animation and must
// never block the UI thread. The result is posted back to the owning
// thread, where the state is updated. One pass serves both purposes: the
// preview is the file the apply later copies into place, so the tint is
// never computed twice.
class FastfetchController::TintJob : public QRunnable
{
public:
    TintJob(QPointer<FastfetchController> controller,
            QString src,
            QString dst,
            QColor tint)
        : m_controller(std::move(controller))
        , m_src(std::move(src))
        , m_dst(std::move(dst))
        , m_tint(tint)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (!m_controller) {
            return;
        }
        const bool ok = tools::tintImageTo(m_src, m_dst, m_tint);
        QMetaObject::invokeMethod(
            m_controller.data(),
            [ctl = m_controller, ok, src = m_src, dst = m_dst]() {
                if (!ctl) {
                    return;
                }
                ctl->finishPreviewTint(ok, src, dst);
            },
            Qt::QueuedConnection);
    }

private:
    QPointer<FastfetchController> m_controller;
    QString m_src;
    QString m_dst;
    QColor m_tint;
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

bool FastfetchController::previewBusy() const
{
    return m_previewRunning;
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
        m_previewValid = false;
        setTintedPreviewPath(QString());
        return;
    }
    if (m_previewRunning) {
        // Coalesce: only the latest color matters; the running job
        // re-runs with it when it finishes.
        m_previewPending = true;
        return;
    }
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/kwal");
    const QString previewDir = cacheRoot + QStringLiteral("/fastfetch_preview");
    QDir().mkpath(previewDir);
    // The folder holds exactly one preview: empty it before every pass so a
    // failed or interrupted run (and ImageMagick's numbered per-frame
    // fallback outputs) never leaves stale files behind.
    QDir dir(previewDir);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Hidden);
    for (const QFileInfo &entry : entries)
        QFile::remove(entry.absoluteFilePath());
    // Older builds wrote the preview loose in the cache root, where an
    // animated source exploded into one PNG per frame: drop those leftovers.
    const QFileInfoList legacy = QDir(cacheRoot).entryInfoList(
        {QStringLiteral("fastfetch-preview*")}, QDir::Files | QDir::Hidden);
    for (const QFileInfo &entry : legacy)
        QFile::remove(entry.absoluteFilePath());
    // The preview keeps the source's animated family (GIF/WebP): the tinted
    // preview itself is an animated GIF/WebP, not a per-frame PNG dump; a
    // still source previews as PNG.
    const QString suffix = QFileInfo(m_selectedFile).suffix().toLower();
    const QString previewSuffix =
        (suffix == QLatin1String("gif") || suffix == QLatin1String("webp"))
            ? suffix
            : QStringLiteral("png");
    m_previewPath = previewDir + QStringLiteral("/fastfetch-preview.") + previewSuffix;
    m_previewValid = false;
    setPreviewBusy(true);
    m_pool.start(new TintJob(QPointer<FastfetchController>{this}, m_selectedFile, m_previewPath, tint));
}

void FastfetchController::finishPreviewTint(bool ok, const QString &src, const QString &dst)
{
    setPreviewBusy(false);
    if (src == m_selectedFile) {
        if (ok) {
            m_previewValid = true;
            // Same cache path, new content: bump the cache-buster so the QML
            // Image source changes and reloads (the path itself does not).
            m_tintedPreviewRevision++;
            setTintedPreviewPath(dst);
            emit tintedPreviewChanged();
        } else {
            m_previewValid = false;
            setTintedPreviewPath(QString());
        }
    }
    if (m_previewPending) {
        m_previewPending = false;
        generateTintedPreview();
        return;
    }
    // An apply clicked while this pass ran (or while no usable preview
    // existed) rides on the fresh result instead of re-tinting.
    if (m_applyAfterPreview) {
        m_applyAfterPreview = false;
        if (m_previewValid && src == m_selectedFile)
            copyPreviewToTinted();
        else
            emit operationResult(false, tr("Failed to generate the tinted image."));
    }
}

void FastfetchController::setBusy(bool value)
{
    if (m_applyRunning != value) {
        m_applyRunning = value;
        emit busyChanged();
    }
}

void FastfetchController::setPreviewBusy(bool value)
{
    if (m_previewRunning != value) {
        m_previewRunning = value;
        emit previewBusyChanged();
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

    // Never re-tint: the apply is a plain copy of the preview the same
    // selection+color pass already produced. If that pass is not up to
    // date (never run, failed, or superseded by a newer color), run it
    // and copy when it lands.
    if (!m_previewValid) {
        m_applyAfterPreview = true;
        if (!m_previewRunning)
            generateTintedPreview();
        return;
    }
    copyPreviewToTinted();
}

// Replaces <dir>/tinted/<stem>-tinted.<ext> with the preview file and
// points the fastfetch config at it. The file stays put afterwards: a
// later preview regeneration clears the cache folder, never the applied
// copy (fastfetch may be reading it at this very moment).
void FastfetchController::copyPreviewToTinted()
{
    setBusy(true);
    const QString dst = tools::tintedImagePath(m_selectedFile);
    QDir().mkpath(QFileInfo(dst).absolutePath());
    QFile::remove(dst);
    const bool ok = QFile::copy(m_previewPath, dst);
    if (!ok) {
        // The cache file vanished or is unreadable: the preview is no
        // longer usable, so a repeated apply regenerates instead of
        // copying the missing file again.
        m_previewValid = false;
        setBusy(false);
        emit operationResult(false, tr("Failed to write the tinted copy."));
        return;
    }

    tools::backupFile(tools::fastfetchConfigPath());
    setHasBackup(tools::hasConfigBackup());

    if (!tools::setImageSource(dst)) {
        setBusy(false);
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
    // The apply step is over: the busy flag it set (via
    // copyPreviewToTinted) ends here, before the result is announced.
    setBusy(false);
    tools::clearCache();
    setConfigImage(appliedPath);
    setHasBackup(true);
    emit operationResult(true, tr("Applied image %1").arg(appliedPath));
}
