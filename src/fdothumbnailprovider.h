#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>

#include <atomic>

class QImage;

// FreeDesktop-standard thumbnails: cache hit -> cached PNG; miss -> downscale the
// original to max 256 px and store. Loading runs on the global thread pool.
class FdoThumbnailProvider : public QQuickAsyncImageProvider
{
    Q_OBJECT
public:
    FdoThumbnailProvider();

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    static QImage thumbnailImage(const QString &path);
    static QString cacheFilePath(const QString &path);
    static QString localPath(const QString &id);
};

class FdoThumbnailResponse : public QQuickImageResponse
{
    Q_OBJECT
public:
    explicit FdoThumbnailResponse(const QString &id);

    QQuickTextureFactory *textureFactory() const override { return m_textureFactory; }
    QString errorString() const override { return m_error; }

    void cancel() override;

    // Runs on the thread pool; emits finished() when done (possibly with a null factory).
    void loadAndFinish();

private:
    QString m_id;
    QQuickTextureFactory *m_textureFactory = nullptr;
    QString m_error;
    // cancel() runs on the GUI thread while the pool reads: a plain
    // (even volatile) bool would be a data race.
    std::atomic<bool> m_canceled{false};
};
