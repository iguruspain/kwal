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

FastfetchController::FastfetchController(QObject *parent)
    : QObject(parent)
{
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
    if (m_selectedFile.isEmpty()) {
        setTintedPreviewPath(QString());
        return;
    }

    QColor tint;
    if (!parseTint(m_draftColor, tint)) {
        setTintedPreviewPath(QString());
        return;
    }
    const QString previewDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/kwal");
    QDir().mkpath(previewDir);
    const QString previewPath = previewDir + QStringLiteral("/fastfetch-preview.png");
    // Run the ImageMagick pipeline on a shrunk copy so previewing stays fast.
    if (!tools::tintImageTo(m_selectedFile, previewPath, tint, 0.8, 512)) {
        setTintedPreviewPath(QString());
        return;
    }
    // Same cache path, new content: bump the cache-buster so the QML
    // Image source changes and reloads.
    m_tintedPreviewRevision++;
    setTintedPreviewPath(previewPath);
    emit tintedPreviewChanged();
}

void FastfetchController::applyOriginalImage()
{
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
    if (m_selectedFile.isEmpty()) {
        emit operationResult(false, tr("No image selected."));
        return;
    }

    QColor tint;
    if (!parseTint(m_draftColor, tint)) {
        emit operationResult(false, tr("No valid tint color selected."));
        return;
    }

    const QString tintedPath = tools::tintedImagePath(m_selectedFile);
    // Full resolution: this file becomes the actual fastfetch logo.
    if (!tools::tintImageTo(m_selectedFile, tintedPath, tint)) {
        emit operationResult(false, tr("Failed to generate the tinted image."));
        return;
    }

    tools::backupFile(tools::fastfetchConfigPath());
    setHasBackup(tools::hasConfigBackup());

    if (!tools::setImageSource(tintedPath)) {
        emit operationResult(false, tr("Tinted image saved, but the fastfetch config could not be updated."));
        return;
    }
    finishApply(tintedPath);
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
