#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>
#include <atomic>

// Metadata index persisted at ~/.cache/kwal/wallpapers_info.json.
// Signals are always emitted on the object's thread.
class WallpapersInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    QML_UNCREATABLE("Provided by C++")
    Q_PROPERTY(bool matugenEnabled READ matugenEnabled WRITE setMatugenEnabled NOTIFY matugenEnabledChanged)
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizeProgressChanged)
    Q_PROPERTY(int resizeTotal READ resizeTotal NOTIFY resizeProgressChanged)
    Q_PROPERTY(int resizeCompleted READ resizeCompleted NOTIFY resizeProgressChanged)
public:
    struct Entry {
        QString file;
        QString type; // "image" or "video"
        // Cache resize copy; empty = original applied as-is.
        QString resized;
        QString firstFrame;
        QStringList sourceColorIndex;
        QStringList colorCategories;
        QStringList tags;
        // true once matugen ran; failed/missing runs retry on the next scan.
        bool matugenDone = false;
        // Manual UI edits; rescans preserve them.
        bool categoriesManual = false;
        bool tagsManual = false;
        // 1/0 animation; -1 = not probed yet (older index).
        int animated = -1;
        double lastModified = 0.0;
    };

    static void initialize(QObject *parent);
    static WallpapersInfo *instance();

    Q_INVOKABLE void refreshFile(const QString &path, const QString &type);
    Q_INVOKABLE void planResize(const QString &path);
    Q_INVOKABLE void beginResizeBatch(int total);
    bool resizing() const;
    int resizeTotal() const;
    int resizeCompleted() const;
    Q_INVOKABLE void sweep();
    Q_INVOKABLE QString firstFrameFor(const QString &path) const;
    Q_INVOKABLE QStringList sourceColorsFor(const QString &path) const;
    Q_INVOKABLE QStringList colorCategoriesFor(const QString &path) const;
    Q_INVOKABLE QStringList tagsFor(const QString &path) const;
    // Falls back to the extension rule while the entry is unprobed.
    Q_INVOKABLE bool isAnimatedFor(const QString &path) const;
    Q_INVOKABLE void setCategories(const QString &path, const QStringList &categories);
    Q_INVOKABLE void setTags(const QString &path, const QStringList &tags);
    Q_INVOKABLE void mergeTags(const QString &path, const QStringList &additions);
    Q_INVOKABLE void recalculateCategories(const QString &path);
    // No-op during a resize batch; purged() prunes dead video URLs from the
    // desktop appletsrc playlist.
    Q_INVOKABLE int clearAllResizes();
    Q_INVOKABLE int clearAllTags();

    bool ffmpegAvailable() const;
    bool matugenAvailable() const;
    bool matugenEnabled() const;
    void setMatugenEnabled(bool enabled);
    // Read by pool jobs; set just before a batch or a pre-apply resize.
    void setResizeParameters(int height, int fps);

    Entry entryFor(const QString &path) const;
    void load();

    // Runs on a pool thread.
    void processFile(const QString &path, const QString &type);
    // Runs on the object's thread (queued from RecalcJob).
    void applyCalculatedCategories(const QString &path, const QStringList &categories, bool ok);
    // Runs on a pool thread.
    void processResize(const QString &path);
    // Blocking probe + encode; must run off the UI thread. Returns the cache
    // copy path, or "" when no resize applies. Emits resizeCopyStarted/Finished.
    QString ensureResizedCopy(const QString &path);

signals:
    void entryChanged(const QString &path);
    void purged();
    void scanFinished();
    void resizeProgressChanged();
    // Emitted from the apply job's thread; ok=false = original applied.
    void resizeCopyStarted(const QString &path);
    void resizeCopyFinished(const QString &path, bool ok);
    void matugenEnabledChanged();

private:
    explicit WallpapersInfo(QObject *parent = nullptr);

    void saveLocked();
    void upsert(const Entry &entry);
    void notifyEntryChanged(const QString &path);
    // Cheap frame-count probe for up-to-date but unprobed entries.
    void fillMissingAnimatedProbe(const QString &path);
    void jobFinished();
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
    // Resize target, read by pool jobs.
    std::atomic<int> m_resizeHeight{1080};
    std::atomic<int> m_resizeFps{0};
    // Resize-batch progress; guarded by m_mutex.
    int m_resizeTotal = 0;
    int m_resizeCompleted = 0;
    int m_resizePending = 0;
    std::atomic<bool> m_ffmpegWarned{false};
    void warnFfmpegOnce();

    static WallpapersInfo *s_instance;
};
