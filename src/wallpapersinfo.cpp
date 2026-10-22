#include "wallpapersinfo.h"
#include <QByteArray>
#include <QColor>
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
#include <QStandardPaths>
#include <QThread>

namespace {

constexpr int kRefreshThreadCount = 2;
constexpr int kMatugenTimeoutMs = 30000;
constexpr int kMaxSourceColors = 8;
constexpr int kMaxCategories = 5;

// Runs "matugen image <image> --show-source-colors" and returns the parsed
// "#rrggbb" colors: deduplicated, order preserved, at most kMaxSourceColors.
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
        if (colors.size() >= kMaxSourceColors) {
            break;
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
        // Up to date: same mtime, (video) the first frame was already extracted,
        // and matugen extraction succeeded whenever it is enabled. Empty
        // categories alone do not make an entry stale: lastModified is the
        // staleness signal, and a failed or missing matugen run (no flag) is
        // retried on the next scan.
        const bool matugenOk = !m_matugenEnabled || it == m_entries.constEnd()
            || !it->sourceColorIndex.isEmpty() || it->matugenDone;
        upToDate = it != m_entries.constEnd()
            && it->lastModified == mtime
            && !(type == QLatin1String("video") && it->firstFrame.isEmpty())
            && matugenOk;
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
    if (m_matugenEnabled && matugenAvailable()) {
        const QString image = (type == QLatin1String("video")) ? entry.firstFrame : path;
        QStringList colors;
        if (!image.isEmpty() && extractSourceColors(image, colors)) {
            entry.sourceColorIndex = colors;
            // Extraction succeeded; empty colors are final, a failed run is retried.
            entry.matugenDone = true;
            for (const QString &color : colors) {
                const QString category = colorCategory(color);
                if (!category.isEmpty() && !entry.colorCategories.contains(category)) {
                    entry.colorCategories.append(category);
                    if (entry.colorCategories.size() >= kMaxCategories) {
                        break;
                    }
                }
            }
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
        if (!entry.file.isEmpty()) {
            m_entries.insert(entry.file, entry);
        }
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
    root[QStringLiteral("version")] = 1;
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

QString WallpapersInfo::colorCategory(const QString &hex)
{
    const QColor color(hex);
    if (!color.isValid()) {
        return {};
    }
    const double lightness = color.lightness() / 255.0;
    const double saturation = color.saturation() / 255.0;
    if (lightness < 0.12) {
        return QStringLiteral("black");
    }
    if (lightness > 0.90 && saturation < 0.25) {
        return QStringLiteral("white");
    }
    if (saturation < 0.15) {
        return QStringLiteral("gray");
    }
    const int hue = color.hue(); // 0-359
    if (hue >= 345 || hue < 15) {
        return QStringLiteral("rose");
    }
    if (hue < 40) {
        return QStringLiteral("red");
    }
    if (hue < 65) {
        return QStringLiteral("orange");
    }
    if (hue < 90) {
        return QStringLiteral("yellow");
    }
    if (hue < 150) {
        return QStringLiteral("yellow-green");
    }
    if (hue < 170) {
        return QStringLiteral("green");
    }
    if (hue < 190) {
        return QStringLiteral("cyan-green");
    }
    if (hue < 210) {
        return QStringLiteral("cyan");
    }
    if (hue < 240) {
        return QStringLiteral("blue-cyan");
    }
    if (hue < 270) {
        return QStringLiteral("blue");
    }
    if (hue < 300) {
        return QStringLiteral("violet");
    }
    return QStringLiteral("magenta");
}

QString WallpapersInfo::jsonPath() const
{
    return m_root + QStringLiteral("/wallpapers_info.json");
}

QString WallpapersInfo::framesDir() const
{
    return m_root + QStringLiteral("/video_frames");
}
