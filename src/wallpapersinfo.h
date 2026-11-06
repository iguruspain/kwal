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
    // The optional video-resize generation batch: how many videos the
    // current "Generate resizes" run covers and how many have finished.
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizeProgressChanged)
    Q_PROPERTY(int resizeTotal READ resizeTotal NOTIFY resizeProgressChanged)
    Q_PROPERTY(int resizeCompleted READ resizeCompleted NOTIFY resizeProgressChanged)
public:
    struct Entry {
        QString file;
        QString type; // "image" or "video"
        // Cache copy created by the optional P5 resize; empty when the
        // original is applied as-is.
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
    // Enqueues one video in a resize batch (explicit "Generate resizes").
    Q_INVOKABLE void planResize(const QString &path);
    // Starts a resize batch of <total> videos; resets the progress counters.
    Q_INVOKABLE void beginResizeBatch(int total);
    // True while a "Generate resizes" batch is running.
    bool resizing() const;
    int resizeTotal() const;
    int resizeCompleted() const;
    // Removes entries whose file no longer exists on disk (and their
    // frames), plus any orphan PNGs in video_frames/ and copies in
    // video_resizes/ that no entry references. Called at startup and after
    // every folder scan; emits entryChanged for every removed path and
    // purged() when anything was removed.
    Q_INVOKABLE void sweep();
    Q_INVOKABLE QString firstFrameFor(const QString &path) const;
    Q_INVOKABLE QStringList sourceColorsFor(const QString &path) const;
    Q_INVOKABLE QStringList colorCategoriesFor(const QString &path) const;
    Q_INVOKABLE QStringList tagsFor(const QString &path) const;
    // Manual overrides from the UI; preserved across folder rescans.
    Q_INVOKABLE void setCategories(const QString &path, const QStringList &categories);
    Q_INVOKABLE void setTags(const QString &path, const QStringList &tags);
    // Adds the given tags to the entry when not already present (case
    // insensitive), keeping the existing order; preserved across rescans.
    Q_INVOKABLE void mergeTags(const QString &path, const QStringList &additions);
    // Replaces the entry's categories with the automatic ones (computed from
    // the image or first frame) and clears the manual flag.
    Q_INVOKABLE void recalculateCategories(const QString &path);
    // Cache cleanup from the Settings page.
    // Removes every cached resize copy on disk and the "resized" reference
    // of every entry; a no-op while a "Generate resizes" batch runs.
    // Returns the number of copies removed; emits entryChanged per affected
    // file and purged() so dead playlist URLs leave the desktop appletsrc.
    Q_INVOKABLE int clearAllResizes();
    // Drops the tags of every entry and clears the manual flag, so a later
    // AI run or rescan starts from an empty tag set. Returns the number of
    // entries changed; emits entryChanged per affected file.
    Q_INVOKABLE int clearAllTags();

    bool ffmpegAvailable() const;
    bool matugenAvailable() const;
    bool matugenEnabled() const;
    void setMatugenEnabled(bool enabled);
    // Sets the resize target read by pool jobs; called just before a
    // "Generate resizes" batch starts or a pre-apply resize runs.
    void setResizeParameters(int height, int fps);

    Entry entryFor(const QString &path) const;
    void load();

    // Runs one refresh pass for <path>; called from the thread pool.
    void processFile(const QString &path, const QString &type);
    // Applies the categories computed by RecalcJob on the object's thread.
    void applyCalculatedCategories(const QString &path, const QStringList &categories, bool ok);
    // Runs one P5 resize pass for <path>; called from the thread pool.
    void processResize(const QString &path);
    // Blocking probe + encode ensuring the resized cache copy for <path>
    // exists (reusing a non-empty copy). Must run off the UI thread (the
    // apply job generates the copy on demand, before applying). Returns
    // the cache copy's path, or "" when the settings call for no resize
    // or the copy could not be produced (the original is applied then).
    // Emits resizeCopyStarted/resizeCopyFinished; only <path> is touched,
    // never the rest of a folder.
    QString ensureResizedCopy(const QString &path);

signals:
    // Emitted after a file's entry was updated.
    void entryChanged(const QString &path);
    // Emitted after a sweep removed entries or cache files.
    void purged();
    // Emitted after all refreshFile jobs queued so far have finished.
    void scanFinished();
    // Emitted as a "Generate resizes" batch progresses.
    void resizeProgressChanged();
    // Emitted (from the apply job's thread) when the on-demand resize of
    // <path> starts and when it ends; ok is false when the copy could
    // not be produced (the original is applied instead).
    void resizeCopyStarted(const QString &path);
    void resizeCopyFinished(const QString &path, bool ok);
    // Emitted when the matugen color-extraction toggle changed.
    void matugenEnabledChanged();

private:
    explicit WallpapersInfo(QObject *parent = nullptr);

    void saveLocked();
    void upsert(const Entry &entry);
    void jobFinished();
    // Finishes one resize job; updates the batch progress on the object's thread.
    void resizeJobFinished();

    QString jsonPath() const;
    QString framesDir() const;
    QString resizesDir() const;

    QString m_root;
    QHash<QString, Entry> m_entries;
    mutable QMutex m_mutex;
    int m_pending = 0; // refresh jobs in flight, guarded by m_mutex
    QThreadPool m_pool;
    std::atomic<bool> m_matugenEnabled{false};
    // P5 resize target, read by pool jobs; set just before a batch starts.
    std::atomic<int> m_resizeHeight{1080};
    std::atomic<int> m_resizeFps{0};
    // "Generate resizes" batch progress; touched only on the object's thread.
    int m_resizeTotal = 0;
    int m_resizeCompleted = 0;
    int m_resizePending = 0;
    // One-time "ffmpeg/ffprobe not found" warning.
    std::atomic<bool> m_ffmpegWarned{false};
    void warnFfmpegOnce();

    static WallpapersInfo *s_instance;
};
