#include "fdothumbnailprovider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QRunnable>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

namespace {

constexpr int kMaxThumbnailSize = 256; // FreeDesktop "large" category.

class FdoThumbnailWorker : public QRunnable
{
public:
    explicit FdoThumbnailWorker(FdoThumbnailResponse *response)
        : m_response(response)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        m_response->loadAndFinish();
    }

private:
    FdoThumbnailResponse *m_response;
};

} // namespace

FdoThumbnailProvider::FdoThumbnailProvider()
    : QQuickAsyncImageProvider()
{
}

QQuickImageResponse *FdoThumbnailProvider::requestImageResponse(const QString &id, const QSize &)
{
    auto *response = new FdoThumbnailResponse(id);
    QThreadPool::globalInstance()->start(new FdoThumbnailWorker(response));
    return response;
}

QImage FdoThumbnailProvider::thumbnailImage(const QString &path)
{
    const QString thumbnailPath = cacheFilePath(path);
    QImage image;
    if (QFile::exists(thumbnailPath) && image.load(thumbnailPath)) {
        return image;
    }

    // Cache miss: downscale the original (bounded memory) and populate the cache.
    QImageReader reader(path);
    reader.setScaledSize(QSize(kMaxThumbnailSize, kMaxThumbnailSize));
    const QImage original = reader.read();
    if (original.isNull()) {
        return {};
    }

    const QDir cacheDir = QFileInfo(thumbnailPath).absoluteDir();
    if (cacheDir.mkpath(QStringLiteral("."))) {
        original.save(thumbnailPath, "PNG");
    }
    return original;
}

QString FdoThumbnailProvider::cacheFilePath(const QString &path)
{
    // FreeDesktop standard: md5 of the percent-encoded file URI.
    const QString fileUri = QStringLiteral("file://") + QUrl::toPercentEncoding(path, QByteArrayLiteral("/"));
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(fileUri.toUtf8(), QCryptographicHash::Md5).toHex());
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
         + QStringLiteral("/thumbnails/large/") + hash + QStringLiteral(".png");
}

QString FdoThumbnailProvider::localPath(const QString &id)
{
    if (id.startsWith(QLatin1String("file://"))) {
        return QUrl(id).toLocalFile();
    }
    return id;
}

FdoThumbnailResponse::FdoThumbnailResponse(const QString &id)
    : m_id(id)
{
}

void FdoThumbnailResponse::cancel()
{
    m_canceled = true;
}

void FdoThumbnailResponse::loadAndFinish()
{
    if (!m_canceled) {
        const QString path = FdoThumbnailProvider::localPath(m_id);
        const QImage image = FdoThumbnailProvider::thumbnailImage(path);
        if (image.isNull()) {
            m_error = QStringLiteral("Could not load thumbnail for %1").arg(path);
        } else {
            m_textureFactory = QQuickTextureFactory::textureFactoryForImage(image);
        }
    }
    emit finished();
}
