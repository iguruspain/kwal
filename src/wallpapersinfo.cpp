#include "wallpapersinfo.h"
#include "colorcategory.h"
#include "tools/fileutil.h"
#include <QByteArray>
#include <QImageReader>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QRunnable>
#include <QSet>
#include <QSize>
#include <QStandardPaths>
#include <QThread>

namespace {

constexpr int kRefreshThreadCount = 2;
constexpr int kMatugenTimeoutMs = 30000;
constexpr int kResizeTimeoutMs = 600000; // P5 encode cap (plan: max 10 min).
constexpr int kMaxAutoCategories = 5; // Automatic categories: top 5 by dominance (D13).

// Runs "matugen image <image> --show-source-colors" and returns the parsed
// "#rrggbb" colors: deduplicated, order preserved.
bool extractSourceColors(const QString &image, QStringList &colors)
{
    const QString binary = QStandardPaths::findExecutable(QStringLiteral("matugen"));
    if (binary.isEmpty()) {
        return false;
    }
    QProcess process;
    process.start(binary,
                 {QStringLiteral("image"),
                  image,
                  QStringLiteral("--show-source-colors")});
    if (!process.waitForStarted(5000) || !process.waitForFinished(kMatugenTimeoutMs)) {
        process.kill();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }
    static const QRegularExpression colorRe(QStringLiteral("#([0-9a-fA-F]{6})"));
    const QByteArray stdoutData = process.readAllStandardOutput();
    for (const QByteArray &line : stdoutData.split('\n')) {
        const QRegularExpressionMatch match = colorRe.match(QString::fromLatin1(line));
        if (!match.hasMatch()) {
            continue;
        }
        const QString hex = QStringLiteral("#") + match.captured(1).toLower();
        if (!colors.contains(hex)) {
            colors.append(hex);
        }
    }
    return !colors.isEmpty();
}

constexpr int kHashChunkBytes = 1024 * 1024;
constexpr int kFfmpegTimeoutMs = 120000;

// Converts a JSON array of strings to a QStringList.
QStringList jsonStringList(const QJsonValue &value)
{
    QStringList list;
    for (const QJsonValue &element : value.toArray()) {
        list.append(element.toString());
    }
    return list;
}

// Extracts the first video frame of <video> as a PNG to <target> (tmp + rename).
bool extractFirstFrame(const QString &video, const QString &target)
{
    const QString binary = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (binary.isEmpty()) {
        return false;
    }
    // Unique per extraction: two refresh passes of the same video can
    // overlap in the pool; a shared tmp name would interleave the two
    // ffmpeg writers in one file (same pattern as encodeResized).
    static std::atomic<quint64> tmpSeq{0};
    const QString tmp = target + QStringLiteral(".%1-%2.tmp")
                            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                            .arg(tmpSeq.fetch_add(1));
    QProcess process;
    process.start(binary,
                  {QStringLiteral("-v"),
                   QStringLiteral("error"),
                   QStringLiteral("-y"),
                   QStringLiteral("-i"),
                   video,
                   QStringLiteral("-map"),
                   QStringLiteral("0:v:0"),
                   QStringLiteral("-frames:v"),
                   QStringLiteral("1"),
                   QStringLiteral("-c:v"),
                   QStringLiteral("png"),
                   QStringLiteral("-f"),
                   QStringLiteral("image2"),
                   QStringLiteral("-update"),
                   QStringLiteral("1"),
                   QStringLiteral("-an"),
                   tmp});
    if (!process.waitForFinished(kFfmpegTimeoutMs)
        || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        QFile::remove(tmp);
        return false;
    }
    const QFileInfo info(tmp);
    if (info.size() <= 0) {
        QFile::remove(tmp);
        return false;
    }
    // Single atomic replace (rename(2)); no remove window.
    return tools::renameAtomic(tmp, target);
}

// MD5 of the file content, read in 1 MiB chunks.
QString contentMd5(const QString &path)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray buffer;
        buffer.resize(kHashChunkBytes);
        for (;;) {
            const qint64 read = file.read(buffer.data(), buffer.size());
            if (read <= 0) {
                break;
            }
            hash.addData(QByteArrayView(buffer.constData(), read));
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

class RefreshJob : public QRunnable
{
public:
    RefreshJob(QPointer<WallpapersInfo> info, QString path, QString type)
        : m_info(std::move(info))
        , m_path(std::move(path))
        , m_type(std::move(type))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (m_info) {
            m_info->processFile(m_path, m_type);
        }
    }

private:
    QPointer<WallpapersInfo> m_info;
    QString m_path;
    QString m_type;
};

// Automatic color categories of <image> (Celebi top colors + HCT
// classification), unique and in rank order, capped at kMaxAutoCategories.
// Returns false when the image cannot be decoded; the categories stay
// empty in that case.
bool computeImageCategories(const QString &image, QStringList &categories)
{
    categories.clear();
    if (image.isEmpty()) {
        return false;
    }
    QImageReader reader(image);
    reader.setScaledSize(QSize(128, 128));
    const QImage scaled = reader.read();
    if (scaled.isNull()) {
        return false;
    }
    for (const quint32 argb : colorcategory::topImageColors(scaled, 8)) {
        const QString category = colorcategory::colorCategory(argb);
        if (!categories.contains(category)) {
            categories.append(category);
            if (categories.size() >= kMaxAutoCategories) {
                break;
            }
        }
    }
    return true;
}

// Recomputes the automatic categories of one wallpaper (the drawer's reset
// button) and applies them on the object's thread.
class RecalcJob : public QRunnable
{
public:
    RecalcJob(QPointer<WallpapersInfo> info, QString path, QString image)
        : m_info(std::move(info))
        , m_path(std::move(path))
        , m_image(std::move(image))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QStringList categories;
        const bool ok = computeImageCategories(m_image, categories);
        if (m_info) {
            auto *info = m_info.data();
            QMetaObject::invokeMethod(info,
                                      [info, path = m_path, categories, ok] {
                                          info->applyCalculatedCategories(path, categories, ok);
                                      },
                                      Qt::QueuedConnection);
        }
    }

private:
    QPointer<WallpapersInfo> m_info;
    QString m_path;
    QString m_image;
};

// ffprobe height/fps of the first video stream.
struct VideoProbe {
    bool ok = false;
    int height = 0;
    double fps = 0.0;
};

VideoProbe probeVideo(const QString &probeBinary, const QString &video)
{
    VideoProbe result;
    QProcess process;
    process.start(probeBinary,
                  {QStringLiteral("-v"),
                   QStringLiteral("error"),
                   QStringLiteral("-select_streams"),
                   QStringLiteral("v:0"),
                   QStringLiteral("-show_entries"),
                   QStringLiteral("stream=width,height,r_frame_rate"),
                   QStringLiteral("-of"),
                   QStringLiteral("json"),
                   video});
    if (!process.waitForStarted(5000) || !process.waitForFinished(kMatugenTimeoutMs)) {
        process.kill();
        return result;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return result;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return result;
    }
    const QJsonObject stream = doc.object().value(QStringLiteral("streams")).toArray().first().toObject();
    const int height = stream.value(QStringLiteral("height")).toInt();
    result.height = height;
    const QStringList rates = stream.value(QStringLiteral("r_frame_rate")).toString().split(QLatin1Char('/'));
    if (rates.size() == 2) {
        const double num = rates.at(0).toDouble();
        const double den = rates.at(1).toDouble();
        if (den > 0.0) {
            result.fps = num / den;
        }
    }
    result.ok = height > 0;
    return result;
}

// The P5 resize decision: false = apply the original as-is. <outHeight> is
// always set to the planned target height.
bool computeResizePlan(int srcHeight, double srcFps, int targetHeight, int targetFps,
                       int &outHeight, int &outFps)
{
    outHeight = qMin(targetHeight, srcHeight) & ~1;
    outFps = 0;
    const bool scale = outHeight < (srcHeight & ~1);
    const bool fpsFilter = targetFps > 0 && targetFps < srcFps - 0.01;
    if (fpsFilter) {
        outFps = targetFps;
    }
    return scale || fpsFilter;
}

// Encodes the resized copy to <output> via a tmp file + rename; the
// -progress stream is drained so the pipe never fills during a long encode.
bool encodeResized(const QString &ffmpegBinary, const QString &video, const QString &output,
                   int targetHeight, int fps, bool scale)
{
    // Unique per encode: a batch job and an on-demand apply-time resize of
    // the same video can overlap; a shared tmp name would interleave the
    // two writers in one file.
    static std::atomic<quint64> tmpSeq{0};
    const QString tmp = output + QStringLiteral(".%1-%2.tmp")
                            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                            .arg(tmpSeq.fetch_add(1));
    QProcess process;
    QStringList args;
    args << QStringLiteral("-v")
         << QStringLiteral("error")
         << QStringLiteral("-y")
         << QStringLiteral("-progress")
         << QStringLiteral("pipe:1")
         << QStringLiteral("-nostats")
         << QStringLiteral("-i")
         << video
         << QStringLiteral("-map")
         << QStringLiteral("0:v:0")
         << QStringLiteral("-an")
         << QStringLiteral("-sn")
         << QStringLiteral("-dn");
    QStringList filter;
    if (scale) {
        filter << QStringLiteral("scale=-2:%1:flags=lanczos").arg(targetHeight);
    }
    if (fps > 0) {
        filter << QStringLiteral("fps=%1").arg(fps);
    }
    if (!filter.isEmpty()) {
        args << QStringLiteral("-vf") << filter.join(QLatin1Char(','));
    }
    args << QStringLiteral("-c:v")
         << QStringLiteral("libx264")
         << QStringLiteral("-preset")
         << QStringLiteral("veryfast")
         << QStringLiteral("-crf")
         << QStringLiteral("23")
         << QStringLiteral("-pix_fmt")
         << QStringLiteral("yuv420p")
         << QStringLiteral("-movflags")
         << QStringLiteral("+faststart")
         << QStringLiteral("-f")
         << QStringLiteral("mp4")
         << tmp;
    process.start(ffmpegBinary, args);
    if (!process.waitForStarted(5000)) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning) {
        if (timer.elapsed() > kResizeTimeoutMs) {
            process.kill();
            return false;
        }
        if (process.waitForReadyRead(100)) {
            process.readAllStandardOutput();
            process.readAllStandardError();
        }
    }
    process.readAllStandardOutput();
    process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile::remove(tmp);
        return false;
    }
    // Single atomic replace (rename(2)); no remove window on the cache copy.
    if (!tools::renameAtomic(tmp, output)) {
        QFile::remove(tmp);
        return false;
    }
    return true;
}

struct ResizeResult {
    // "" = no resize applied; otherwise the cache copy path.
    QString output;
    // The plan ran to a conclusion (encoded, reused, or not needed by the
    // settings); false = probe/encode failure, retry on the next scan.
    bool planned = false;
    bool noFfmpeg = false;
};

// Probes <video> and encodes the resized cache copy when the P5 settings
// call for it; reuses an existing non-empty copy.
ResizeResult planAndEncode(const QString &resizesRoot, const QString &video, int targetHeight,
                           int targetFps)
{
    ResizeResult result;
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (ffmpeg.isEmpty() || ffprobe.isEmpty()) {
        result.noFfmpeg = true;
        return result;
    }
    const VideoProbe probe = probeVideo(ffprobe, video);
    if (!probe.ok) {
        return result;
    }
    int height = 0;
    int fps = 0;
    const bool scale = computeResizePlan(probe.height, probe.fps, targetHeight, targetFps, height, fps);
    result.planned = true;
    if (!scale) {
        return result;
    }
    const int fpsToken = fps > 0 ? fps : qRound(probe.fps);
    const QString output = resizesRoot + QStringLiteral("/")
        + contentMd5(video) + QStringLiteral("_%1p%2.mp4").arg(height).arg(fpsToken);
    if (QFileInfo(output).size() > 0) {
        result.output = output;
        return result;
    }
    QDir().mkpath(QFileInfo(output).absolutePath());
    if (encodeResized(ffmpeg, video, output, height, fps, height < (probe.height & ~1))) {
        result.output = output;
    }
    return result;
}

// Re-runs the P5 resize plan for one video (settings change).
class ResizeJob : public QRunnable
{
public:
    ResizeJob(QPointer<WallpapersInfo> info, QString path)
        : m_info(std::move(info))
        , m_path(std::move(path))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (m_info) {
            m_info->processResize(m_path);
        }
    }

private:
    QPointer<WallpapersInfo> m_info;
    QString m_path;
};
} // namespace

WallpapersInfo *WallpapersInfo::s_instance = nullptr;

void WallpapersInfo::initialize(QObject *parent)
{
    if (s_instance) {
        s_instance->deleteLater();
    }
    s_instance = new WallpapersInfo(parent);
    s_instance->load();
}

WallpapersInfo *WallpapersInfo::instance()
{
    return s_instance;
}

WallpapersInfo::WallpapersInfo(QObject *parent)
    : QObject(parent)
{
    // Shared with the legacy Python app: ~/.cache/kwal (XDG cache, no org nesting).
    m_root = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/kwal");
    QDir().mkpath(framesDir());
    m_pool.setMaxThreadCount(kRefreshThreadCount);
}

void WallpapersInfo::refreshFile(const QString &path, const QString &type)
{
    if (path.isEmpty() || (type != QLatin1String("image") && type != QLatin1String("video"))) {
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        ++m_pending;
    }
    m_pool.start(new RefreshJob(QPointer<WallpapersInfo>(this), path, type));
}

void WallpapersInfo::processFile(const QString &path, const QString &type)
{
    const QFileInfo fi(path);
    if (!fi.isFile()) {
        jobFinished();
        return;
    }
    const double mtime = fi.lastModified().toMSecsSinceEpoch() / 1000.0;
    bool upToDate = false;
    bool carryCategoriesManual = false;
    bool carryTagsManual = false;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_entries.constFind(path);
        // Up to date: same mtime, (video) the first frame was already
        // extracted, image categories were computed (or are manual), and
        // matugen extraction succeeded whenever it is enabled. Empty
        // categories (failed decode) and a failed or missing matugen run
        // (no flag) are retried on the next scan.
        const bool matugenOk = !m_matugenEnabled || it == m_entries.constEnd()
            || !it->sourceColorIndex.isEmpty() || it->matugenDone;
        const bool categoriesOk = it == m_entries.constEnd()
            || it->categoriesManual || !it->colorCategories.isEmpty();
        upToDate = it != m_entries.constEnd()
            && it->lastModified == mtime
            && !(type == QLatin1String("video") && it->firstFrame.isEmpty())
            && matugenOk && categoriesOk;
        if (it != m_entries.constEnd()) {
            carryCategoriesManual = it->categoriesManual;
            carryTagsManual = it->tagsManual;
        }
    }
    if (upToDate) {
        // jobFinished() acquires m_mutex itself; it must not be called
        // while this scope still holds it (non-recursive self-deadlock).
        jobFinished();
        return;
    }

    Entry entry;
    entry.file = path;
    entry.type = type;
    entry.lastModified = mtime;
    if (type == QLatin1String("video")) {
        const QString framePath = framesDir() + QStringLiteral("/") + contentMd5(path) + QStringLiteral(".png");
        if (QFileInfo(framePath).size() > 0 || extractFirstFrame(path, framePath)) {
            entry.firstFrame = framePath;
        }
    }
    const QString image = (type == QLatin1String("video")) ? entry.firstFrame : path;
    // Color categories from the image itself (material-color-utilities:
    // Celebi top colors + HCT classification), independent of matugen.
    computeImageCategories(image, entry.colorCategories);
    // Optional matugen source colors (wallpaper dots only).
    if (m_matugenEnabled && matugenAvailable() && !image.isEmpty()) {
        QStringList colors;
        if (extractSourceColors(image, colors)) {
            entry.sourceColorIndex = colors;
            // Extraction succeeded; empty colors are final, a failed run is retried.
            entry.matugenDone = true;
        }
    }
    // Manually edited categories/tags survive rescans.
    if (carryCategoriesManual || carryTagsManual) {
        QMutexLocker locker(&m_mutex);
        const auto it = m_entries.constFind(path);
        if (it != m_entries.constEnd()) {
            if (carryCategoriesManual) {
                entry.colorCategories = it->colorCategories;
                entry.categoriesManual = true;
            }
            if (carryTagsManual) {
                entry.tags = it->tags;
                entry.tagsManual = true;
            }
        }
    }
    upsert(entry);
    jobFinished();
}

void WallpapersInfo::upsert(const Entry &entry)
{
    {
        QMutexLocker locker(&m_mutex);
        m_entries.insert(entry.file, entry);
        saveLocked();
    }
    const QString path = entry.file;
    if (QThread::currentThread() == thread()) {
        emit entryChanged(path);
    } else {
        QMetaObject::invokeMethod(this, [this, path]() { emit entryChanged(path); }, Qt::QueuedConnection);
    }
}

void WallpapersInfo::sweep()
{
    QVector<QString> removedPaths;
    QSet<QString> framesToRemove;
    QSet<QString> resizesToRemove;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_entries.begin(); it != m_entries.end();) {
            if (!QFileInfo(it.key()).isFile()) {
                if (!it->firstFrame.isEmpty()) {
                    framesToRemove.insert(it->firstFrame);
                }
                removedPaths.append(it.key());
                it = m_entries.erase(it);
                continue;
            }
            ++it;
        }
        const QDir frames(framesDir());
        for (const QString &fileName : frames.entryList(QDir::Files)) {
            const QString frame = frames.filePath(fileName);
            bool referenced = false;
            for (const Entry &entry : std::as_const(m_entries)) {
                if (entry.firstFrame == frame) {
                    referenced = true;
                    break;
                }
            }
            if (!referenced) {
                framesToRemove.insert(frame);
            }
        }
        const QDir resizes(resizesDir());
        for (const QString &fileName : resizes.entryList(QDir::Files)) {
            const QString resize = resizes.filePath(fileName);
            bool referenced = false;
            for (const Entry &entry : std::as_const(m_entries)) {
                if (entry.resized == resize) {
                    referenced = true;
                    break;
                }
            }
            if (!referenced) {
                resizesToRemove.insert(resize);
            }
        }
    }
    if (removedPaths.isEmpty() && framesToRemove.isEmpty() && resizesToRemove.isEmpty()) {
        return;
    }
    for (const QString &frame : framesToRemove) {
        QFile::remove(frame);
    }
    for (const QString &resize : resizesToRemove) {
        QFile::remove(resize);
    }
    if (!removedPaths.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        saveLocked();
    }
    for (const QString &path : removedPaths) {
        if (QThread::currentThread() == thread()) {
            emit entryChanged(path);
        } else {
            QMetaObject::invokeMethod(this,
                                      [this, path]() { emit entryChanged(path); },
                                      Qt::QueuedConnection);
        }
    }
    emit purged();
}

void WallpapersInfo::jobFinished()
{
    bool allDone = false;
    {
        QMutexLocker locker(&m_mutex);
        allDone = --m_pending <= 0;
    }
    if (allDone) {
        if (QThread::currentThread() == thread()) {
            emit scanFinished();
        } else {
            QMetaObject::invokeMethod(this, [this]() { emit scanFinished(); }, Qt::QueuedConnection);
        }
    }
}

void WallpapersInfo::load()
{
    QMutexLocker locker(&m_mutex);
    m_entries.clear();
    const QString path = jsonPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        // Corrupt file: drop it and rewrite an empty index.
        saveLocked();
        return;
    }
    // v1 stored matugen-derived categories (HSL, capped); recompute them
    // from the image on the next scan. Manual edits survive via the
    // categoriesManual flag; lastModified=0 forces one full reprocess.
    const bool migrate = doc.object().value(QStringLiteral("version")).toInt(1) < 2;
    bool migrated = false;
    for (const QJsonValue &value : doc.object().value(QStringLiteral("files")).toArray()) {
        const QJsonObject obj = value.toObject();
        Entry entry;
        entry.file = obj.value(QStringLiteral("file")).toString();
        entry.type = obj.value(QStringLiteral("type")).toString();
        entry.resized = obj.value(QStringLiteral("resized")).toString();
        entry.firstFrame = obj.value(QStringLiteral("first_frame")).toString();
        entry.sourceColorIndex = jsonStringList(obj.value(QStringLiteral("source_color_index")));
        entry.colorCategories = jsonStringList(obj.value(QStringLiteral("color_categories")));
        entry.tags = jsonStringList(obj.value(QStringLiteral("tags")));
        entry.lastModified = obj.value(QStringLiteral("last_modified")).toDouble();
        entry.matugenDone = obj.value(QStringLiteral("matugen_done")).toBool();
        entry.categoriesManual = obj.value(QStringLiteral("categories_manual")).toBool();
        entry.tagsManual = obj.value(QStringLiteral("tags_manual")).toBool();
        if (migrate) {
            if (!entry.categoriesManual) {
                entry.colorCategories.clear();
            }
            entry.lastModified = 0.0;
            migrated = true;
        }
        if (!entry.file.isEmpty()) {
            m_entries.insert(entry.file, entry);
        }
    }
    if (migrated) {
        saveLocked(); // persist the index as version 2
    }
}

void WallpapersInfo::saveLocked()
{
    QJsonArray files;
    for (const Entry &entry : std::as_const(m_entries)) {
        QJsonObject obj;
        obj[QStringLiteral("file")] = entry.file;
        obj[QStringLiteral("type")] = entry.type;
        obj[QStringLiteral("resized")] = entry.resized;
        obj[QStringLiteral("first_frame")] = entry.firstFrame;
        obj[QStringLiteral("source_color_index")] = QJsonArray::fromStringList(entry.sourceColorIndex);
        obj[QStringLiteral("color_categories")] = QJsonArray::fromStringList(entry.colorCategories);
        obj[QStringLiteral("tags")] = QJsonArray::fromStringList(entry.tags);
        obj[QStringLiteral("last_modified")] = entry.lastModified;
        if (entry.matugenDone) {
            obj[QStringLiteral("matugen_done")] = true;
        }
        if (entry.categoriesManual) {
            obj[QStringLiteral("categories_manual")] = true;
        }
        if (entry.tagsManual) {
            obj[QStringLiteral("tags_manual")] = true;
        }
        files.append(obj);
    }
    QJsonObject root;
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("files")] = files;

    const QString path = jsonPath();
    const QString tmp = path + QStringLiteral(".tmp");
    QFile file(tmp);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
        // Single atomic replace (rename(2)); no remove window on the cache.
        if (!tools::renameAtomic(tmp, path)) {
            QFile::remove(tmp);
        }
    }
}

WallpapersInfo::Entry WallpapersInfo::entryFor(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.value(path);
}

QString WallpapersInfo::firstFrameFor(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.value(path).firstFrame;
}

QStringList WallpapersInfo::sourceColorsFor(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.value(path).sourceColorIndex;
}

QStringList WallpapersInfo::colorCategoriesFor(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.value(path).colorCategories;
}

QStringList WallpapersInfo::tagsFor(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.value(path).tags;
}

void WallpapersInfo::setCategories(const QString &path, const QStringList &categories)
{
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_entries.find(path);
        if (it == m_entries.end()) {
            Entry entry;
            entry.file = path;
            it = m_entries.insert(entry.file, entry);
        }
        it->colorCategories = categories;
        it->categoriesManual = true;
        saveLocked();
    }
    emit entryChanged(path);
}

void WallpapersInfo::setTags(const QString &path, const QStringList &tags)
{
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_entries.find(path);
        if (it == m_entries.end()) {
            Entry entry;
            entry.file = path;
            it = m_entries.insert(entry.file, entry);
        }
        it->tags = tags;
        it->tagsManual = true;
        saveLocked();
    }
    emit entryChanged(path);
}

void WallpapersInfo::mergeTags(const QString &path, const QStringList &additions)
{
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_entries.find(path);
        if (it == m_entries.end()) {
            Entry entry;
            entry.file = path;
            it = m_entries.insert(entry.file, entry);
        }
        QSet<QString> seen;
        for (const QString &tag : it->tags) {
            seen.insert(tag.toLower());
        }
        for (const QString &tag : additions) {
            const QString key = tag.toLower();
            if (!seen.contains(key)) {
                seen.insert(key);
                it->tags.append(tag);
            }
        }
        it->tagsManual = true;
        saveLocked();
    }
    emit entryChanged(path);
}

int WallpapersInfo::clearAllResizes()
{
    QStringList changedPaths;
    QStringList copies;
    {
        QMutexLocker locker(&m_mutex);
        if (m_resizePending > 0) {
            // A "Generate resizes" batch is running; clearing mid-batch
            // would race with the jobs writing the new copies.
            return 0;
        }
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->resized.isEmpty()) {
                continue;
            }
            copies.append(it->resized);
            it->resized.clear();
            changedPaths.append(it.key());
        }
        if (copies.isEmpty()) {
            return 0;
        }
        saveLocked();
    }
    for (const QString &copy : std::as_const(copies)) {
        QFile::remove(copy);
    }
    for (const QString &path : std::as_const(changedPaths)) {
        emit entryChanged(path);
    }
    // Same contract as sweep(): cache files went away, so the desktop
    // video playlist gets its dead URLs pruned.
    emit purged();
    return copies.size();
}

int WallpapersInfo::clearAllTags()
{
    QStringList changedPaths;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->tags.isEmpty() && !it->tagsManual) {
                continue;
            }
            it->tags.clear();
            it->tagsManual = false;
            changedPaths.append(it.key());
        }
        if (changedPaths.isEmpty()) {
            return 0;
        }
        saveLocked();
    }
    for (const QString &path : std::as_const(changedPaths)) {
        emit entryChanged(path);
    }
    return changedPaths.size();
}

void WallpapersInfo::recalculateCategories(const QString &path)
{
    QString image;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_entries.constFind(path);
        if (it == m_entries.constEnd()) {
            return;
        }
        image = it->type == QLatin1String("video") ? it->firstFrame : path;
    }
    if (image.isEmpty()) {
        return;
    }
    m_pool.start(new RecalcJob(QPointer<WallpapersInfo>(this), path, image));
}

void WallpapersInfo::applyCalculatedCategories(const QString &path, const QStringList &categories, bool ok)
{
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_entries.find(path);
        if (it == m_entries.end() || !ok) {
            // Unknown entry, or the image could not be decoded: keep the
            // current categories (manual or not) untouched.
            return;
        }
        it->colorCategories = categories;
        it->categoriesManual = false;
        saveLocked();
    }
    emit entryChanged(path);
}

bool WallpapersInfo::ffmpegAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
}

bool WallpapersInfo::matugenAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("matugen")).isEmpty();
}

bool WallpapersInfo::matugenEnabled() const
{
    return m_matugenEnabled;
}

void WallpapersInfo::setMatugenEnabled(bool enabled)
{
    if (m_matugenEnabled == enabled) {
        return;
    }
    m_matugenEnabled = enabled;
    emit matugenEnabledChanged();
}

void WallpapersInfo::setResizeParameters(int height, int fps)
{
    m_resizeHeight = height;
    m_resizeFps = fps;
}

void WallpapersInfo::beginResizeBatch(int total)
{
    {
        QMutexLocker locker(&m_mutex);
        m_resizeTotal = total;
        m_resizeCompleted = 0;
        m_resizePending = total;
    }
    emit resizeProgressChanged();
}

void WallpapersInfo::resizeJobFinished()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_resizePending > 0) {
            --m_resizePending;
        }
        ++m_resizeCompleted;
    }
    emit resizeProgressChanged();
}

bool WallpapersInfo::resizing() const
{
    QMutexLocker locker(&m_mutex);
    return m_resizePending > 0;
}

int WallpapersInfo::resizeTotal() const
{
    QMutexLocker locker(&m_mutex);
    return m_resizeTotal;
}

int WallpapersInfo::resizeCompleted() const
{
    QMutexLocker locker(&m_mutex);
    return m_resizeCompleted;
}

void WallpapersInfo::planResize(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    m_pool.start(new ResizeJob(QPointer<WallpapersInfo>(this), path));
}

void WallpapersInfo::processResize(const QString &path)
{
    Entry entry;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_entries.constFind(path);
        if (it == m_entries.constEnd() || it->type != QLatin1String("video")) {
            resizeJobFinished();
            return;
        }
        entry = *it;
    }
    const ResizeResult result = planAndEncode(resizesDir(), path, m_resizeHeight, m_resizeFps);
    if (result.noFfmpeg) {
        warnFfmpegOnce();
    } else {
        entry.resized = result.planned ? result.output : QString();
    }
    upsert(entry);
    resizeJobFinished();
}

QString WallpapersInfo::ensureResizedCopy(const QString &path)
{
    if (path.isEmpty()) {
        return QString();
    }
    emit resizeCopyStarted(path);
    const ResizeResult result = planAndEncode(resizesDir(), path, m_resizeHeight, m_resizeFps);
    Entry entry = entryFor(path);
    if (entry.file.isEmpty()) {
        // Defensive: an untracked video; keep a minimal entry so the copy
        // survives sweeps.
        entry.file = path;
        entry.type = QStringLiteral("video");
    }
    // Mirror the batch job: only a completed plan (encoded, reused, or
    // "not needed by the settings") writes the result to the entry; a
    // probe/encode failure clears it and the original is applied.
    entry.resized = result.planned ? result.output : QString();
    upsert(entry);
    if (result.noFfmpeg) {
        warnFfmpegOnce();
    }
    emit resizeCopyFinished(path, result.planned);
    return result.output;
}

void WallpapersInfo::warnFfmpegOnce()
{
    bool expected = false;
    if (m_ffmpegWarned.compare_exchange_strong(expected, true)) {
        qWarning("kwal: ffmpeg/ffprobe not found; video resizing is disabled.");
    }
}

QString WallpapersInfo::jsonPath() const
{
    return m_root + QStringLiteral("/wallpapers_info.json");
}

QString WallpapersInfo::framesDir() const
{
    return m_root + QStringLiteral("/video_frames");
}

QString WallpapersInfo::resizesDir() const
{
    return m_root + QStringLiteral("/video_resizes");
}
