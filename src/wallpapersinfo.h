#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>
#include <atomic>

// Per-wallpaper metadata, persisted as JSON in ~/.cache/kwal/wallpapers_info.json:
// media type, first video frame (PNG named by content MD5 in
// ~/.cache/kwal/video_frames/), color categories (derived from the image via
// material-color-utilities), optional matugen source colors and tags.
// Heavy per-file work (hashing, ffmpeg frame extraction, color analysis) runs
// on a 2-thread pool; signals are always emitted on the thread the object lives on.
class WallpapersInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    QML_UNCREATABLE("Provided by C++")
    Q_PROPERTY(bool matugenEnabled READ matugenEnabled WRITE setMatugenEnabled NOTIFY matugenEnabledChanged)
public:
    struct Entry {
        QString file;
        QString type; // "image" or "video"
        QString resized;
        QString firstFrame;
        QStringList sourceColorIndex;
        QStringList colorCategories;
        QStringList tags;
        // matugen extraction succeeded for this file; a failed or missing run
        // is retried on the next scan.
        bool matugenDone = false;
        // Categories/tags were edited manually in the UI; rescans preserve them.
        bool categoriesManual = false;
        bool tagsManual = false;
        double lastModified = 0.0;
    };

    // Creates (or replaces) the singleton; loads the persisted index.
    static void initialize(QObject *parent);
    static WallpapersInfo *instance();

    Q_INVOKABLE void refreshFile(const QString &path, const QString &type);
    // Removes index entries whose file no longer exists (and their extracted
    // frames), plus any orphan frame PNGs in the cache. Call after a folder
    // scan; emits entryChanged for every removed path.
    Q_INVOKABLE void sweep();
    Q_INVOKABLE QString firstFrameFor(const QString &path) const;
    Q_INVOKABLE QStringList sourceColorsFor(const QString &path) const;
    Q_INVOKABLE QStringList colorCategoriesFor(const QString &path) const;
    Q_INVOKABLE QStringList tagsFor(const QString &path) const;
    // Manual overrides from the UI; preserved across folder rescans.
    Q_INVOKABLE void setCategories(const QString &path, const QStringList &categories);
    Q_INVOKABLE void setTags(const QString &path, const QStringList &tags);
    // Replaces the entry's categories with the automatic ones (computed from
    // the image or first frame) and clears the manual flag.
    Q_INVOKABLE void recalculateCategories(const QString &path);

    bool ffmpegAvailable() const;
    bool matugenAvailable() const;
    bool matugenEnabled() const;
    void setMatugenEnabled(bool enabled);

    Entry entryFor(const QString &path) const;
    void load();

    // Runs one refresh pass for <path>; called from the thread pool.
    void processFile(const QString &path, const QString &type);
    // Applies the categories computed by RecalcJob on the object's thread.
    void applyCalculatedCategories(const QString &path, const QStringList &categories, bool ok);

signals:
    // Emitted after a file's entry was updated.
    void entryChanged(const QString &path);
    // Emitted after all refreshFile jobs queued so far have finished.
    void scanFinished();
    // Emitted when the matugen color-extraction toggle changed.
    void matugenEnabledChanged();

private:
    explicit WallpapersInfo(QObject *parent = nullptr);

    void saveLocked();
    void upsert(const Entry &entry);
    void jobFinished();

    QString jsonPath() const;
    QString framesDir() const;

    QString m_root;
    QHash<QString, Entry> m_entries;
    mutable QMutex m_mutex;
    int m_pending = 0; // refresh jobs in flight, guarded by m_mutex
    QThreadPool m_pool;
    std::atomic<bool> m_matugenEnabled{false};

    static WallpapersInfo *s_instance;
};
