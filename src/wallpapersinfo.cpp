#include "wallpapersinfo.h"
#include "colorcategory.h"
#include <QByteArray>
#include <QImageReader>
#include <QCryptographicHash>
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
    const QString tmp = target + QStringLiteral(".tmp");
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
    // Qt's QFile::rename does not replace an existing target, so drop it first.
    if (QFile::exists(target)) {
        QFile::remove(target);
    }
    return QFile::rename(tmp, target);
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
    }
    if (removedPaths.isEmpty() && framesToRemove.isEmpty()) {
        return;
    }
    for (const QString &frame : framesToRemove) {
        QFile::remove(frame);
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
        // Qt's QFile::rename does not replace an existing target, so drop it first.
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
        if (!QFile::rename(tmp, path)) {
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

QString WallpapersInfo::jsonPath() const
{
    return m_root + QStringLiteral("/wallpapers_info.json");
}

QString WallpapersInfo::framesDir() const
{
    return m_root + QStringLiteral("/video_frames");
}
