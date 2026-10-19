#include "fastfetchcontroller.h"

#include "tools/fastfetchconfig.h"
#include "tools/fileutil.h"
#include "tools/tintimage.h"

#include <QColor>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QImageReader>
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
    if (m_draftColor != color) {
        m_draftColor = color;
        emit draftColorChanged();
    }
}

QString FastfetchController::tintedPreviewPath() const
{
    return m_tintedPreviewPath;
}

bool FastfetchController::hasBackup() const
{
    return m_hasBackup;
}

QString FastfetchController::fileUrl(const QString &path) const
{
    if (path.isEmpty())
        return QString();
    return QUrl::fromLocalFile(path).toString();
}

void FastfetchController::refreshConfigImage()
{
    const QString detected0 = tools::detectConfigImage();
    setConfigImage(detected0);
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
        generateTintedPreview();
    }
}

void FastfetchController::selectFile(const QString &path)
{
    m_autoSelected = false;
    setSelectedFile(path);
    generateTintedPreview();
}

void FastfetchController::clearSelection()
{
    m_autoSelected = false;
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

    QImageReader reader(m_selectedFile);
    reader.setScaledSize(QSize(512, 512));
    QImage image = reader.read();
    if (image.isNull()) {
        setTintedPreviewPath(QString());
        return;
    }

    const QImage tinted = tools::tintImage(image, tint);
    const QString previewDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/kwal");
    QDir().mkpath(previewDir);
    const QString previewPath = previewDir + QStringLiteral("/fastfetch-preview.png");
    if (tinted.save(previewPath))
        setTintedPreviewPath(previewPath);
    else
        setTintedPreviewPath(QString());
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

    QImage image(m_selectedFile);
    if (image.isNull()) {
        emit operationResult(false, tr("Failed to read the selected image."));
        return;
    }

    const QString tintedPath = tools::tintedImagePath(m_selectedFile);
    if (!tools::tintImage(image, tint).save(tintedPath)) {
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

void FastfetchController::finishApply(const QString &appliedPath)
{
    tools::clearCache();
    setConfigImage(appliedPath);
    setHasBackup(true);
    emit operationResult(true, tr("Applied image %1").arg(appliedPath));
}
