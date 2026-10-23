#include "plasmaapply.h"
#include "wallpapersinfo.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QVariantMap>

#include <optional>

namespace plasmaapply {

namespace {

constexpr char kImagePluginId[] = "org.kde.image";
constexpr char kRebornPluginId[] = "luisbocanegra.smart.video.wallpaper.reborn";

// Live desktop IDs via the shell scripting API (same script as the Rust
// reference); empty when D-Bus is unavailable or the call fails.
constexpr char kDesktopIdsScript[] = "var d = desktops(); var r = []; "
                                     "for (var i = 0; i < d.length; i++) { "
                                     "if (d[i].formFactor == 0) { r.push(d[i].id); } } "
                                     "print(r.join(','));";

QString configPath(const QString &fileName)
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1Char('/') + fileName;
}

// A path byte passes through unencoded only when it is one of the
// characters Plasma keeps as-is in config file URLs.
bool isUnreserved(char c)
{
    const auto b = static_cast<quint8>(c);
    return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9')
        || b == '-' || b == '_' || b == '.' || b == '~' || b == '/';
}

QStringList pluginSection(int desktopId, const QString &plugin)
{
    return {QStringLiteral("Containments"), QString::number(desktopId),
            QStringLiteral("Wallpaper"), plugin, QStringLiteral("General")};
}

} // namespace

QString fileUrl(const QString &path)
{
    const QByteArray bytes = path.toUtf8();
    QByteArray encoded;
    encoded.reserve(bytes.size() * 3);
    for (const char c : bytes) {
        if (isUnreserved(c)) {
            encoded.append(c);
        } else {
            encoded.append('%');
            encoded.append(QStringLiteral("%1")
                               .arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0'))
                               .toUpper()
                               .toLatin1());
        }
    }
    return QStringLiteral("file://") + QString::fromUtf8(encoded);
}

QString rawVideoUrl(const QString &path)
{
    return QStringLiteral("file://") + path;
}

QString computeVideoUrls(const QString &existingJson, const QString &targetUrl)
{
    QJsonArray entries;
    if (!existingJson.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(existingJson.toUtf8());
        if (doc.isArray()) {
            entries = doc.array();
        }
    }

    // The plugin's config page re-serializes the playlist with JSON.stringify
    // in this exact field order (updateConfig in config.qml), dropping entries
    // without a filename. The stored string must round-trip byte-identically
    // through that serialization, or the page reports unsaved changes as soon
    // as the model is touched.
    const auto json = [](const QJsonValue &value) {
        QJsonArray wrapper;
        wrapper.append(value);
        const QByteArray compact = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(compact.constData() + 1, compact.size() - 2);
    };
    const auto fieldOf = [](const QJsonObject &entry, const QString &key, const QJsonValue &fallback) {
        return entry.contains(key) ? entry.value(key) : fallback;
    };
    QStringList rendered;
    bool present = false;
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString filename = entry.value(QStringLiteral("filename")).toString();
        if (filename.isEmpty()) {
            continue; // the page drops such entries
        }
        const bool isTarget = filename == targetUrl;
        if (isTarget) {
            present = true;
        }
        rendered.append(QStringLiteral("{\"filename\":") + json(QJsonValue(filename))
                        + QStringLiteral(",\"enabled\":") + json(QJsonValue(isTarget))
                        + QStringLiteral(",\"duration\":")
                        + json(fieldOf(entry, QStringLiteral("duration"), QJsonValue(0)))
                        + QStringLiteral(",\"customDuration\":")
                        + json(fieldOf(entry, QStringLiteral("customDuration"), QJsonValue(0)))
                        + QStringLiteral(",\"playbackRate\":")
                        + json(fieldOf(entry, QStringLiteral("playbackRate"), QJsonValue(0)))
                        + QStringLiteral(",\"alternativePlaybackRate\":")
                        + json(fieldOf(entry, QStringLiteral("alternativePlaybackRate"), QJsonValue(0)))
                        + QStringLiteral(",\"loop\":")
                        + json(fieldOf(entry, QStringLiteral("loop"), QJsonValue(false)))
                        + QStringLiteral(",\"dayNightPhase\":")
                        + json(fieldOf(entry, QStringLiteral("dayNightPhase"), QJsonValue(4)))
                        + QStringLiteral("}"));
    }
    if (!present) {
        rendered.append(QStringLiteral("{\"filename\":") + json(QJsonValue(targetUrl))
                        + QStringLiteral(",\"enabled\":true,\"duration\":0,\"customDuration\":0,"
                                         "\"playbackRate\":0,\"alternativePlaybackRate\":0,"
                                         "\"loop\":false,\"dayNightPhase\":4}"));
    }
    return QStringLiteral("[") + rendered.join(QStringLiteral(",")) + QStringLiteral("]");
}

Ini::Ini(const QString &content)
{
    if (content.isEmpty()) {
        return;
    }
    QStringList lines = content.split(QLatin1Char('\n'));
    if (content.endsWith(QLatin1Char('\n'))) {
        lines.removeLast();
    }
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        m_lines.append(line);
    }
}

Ini Ini::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return Ini(QString::fromUtf8(file.readAll()));
}

bool Ini::save(const QString &path) const
{
    const QFileInfo info(path);
    QTemporaryFile tmp(QDir(info.absolutePath()).filePath(QStringLiteral(".kwal-rc-XXXXXX")));
    if (!tmp.open()) {
        return false;
    }
    const QByteArray content = render().toUtf8();
    if (tmp.write(content) != content.size()) {
        return false;
    }
    tmp.close();
    const QString tmpPath = tmp.fileName();
    // Qt's QFile::rename does not replace an existing target, so drop it first.
    if (QFile::exists(path)) {
        QFile::remove(path);
    }
    return QFile::rename(tmpPath, path);
}

QString Ini::render() const
{
    return m_lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString Ini::get(const QStringList &section, const QString &key) const
{
    auto found = findSection(section);
    if (!found) {
        return {};
    }
    for (int i = found->first + 1; i < found->second; ++i) {
        if (keyOf(m_lines.at(i)).value_or(QString()) == key) {
            const QString t = m_lines.at(i).trimmed();
            const int eq = t.indexOf(QLatin1Char('='));
            return eq >= 0 ? t.mid(eq + 1) : QString();
        }
    }
    return {};
}

void Ini::set(const QStringList &section, const QString &key, const QString &value)
{
    const QString kv = key + QLatin1Char('=') + value;
    auto found = findSection(section);
    if (found) {
        for (int i = found->first + 1; i < found->second; ++i) {
            if (keyOf(m_lines.at(i)).value_or(QString()) == key) {
                m_lines[i] = kv;
                return;
            }
        }
        m_lines.insert(found->second, kv);
    } else {
        if (!m_lines.isEmpty() && !m_lines.last().isEmpty()) {
            m_lines.append(QString());
        }
        m_lines.append(makeHeader(section));
        m_lines.append(kv);
    }
}

QStringList Ini::desktopIds() const
{
    QStringList ids;
    for (const QString &line : m_lines) {
        const auto parts = sectionParts(line);
        if (!parts || parts->size() != 2 || parts->first() != QStringLiteral("Containments")) {
            continue;
        }
        bool numeric = false;
        const int id = parts->last().toInt(&numeric);
        if (!numeric || id < 0) {
            continue;
        }
        if (get(*parts, QStringLiteral("formfactor")) == QStringLiteral("0")) {
            ids.append(parts->last());
        }
    }
    return ids;
}

std::optional<std::pair<int, int>> Ini::findSection(const QStringList &parts) const
{
    const QString header = makeHeader(parts);
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i).trimmed() != header) {
            continue;
        }
        int end = m_lines.size();
        for (int j = i + 1; j < m_lines.size(); ++j) {
            if (sectionParts(m_lines.at(j)).has_value()) {
                end = j;
                break;
            }
        }
        return std::make_pair(i, end);
    }
    return std::nullopt;
}

std::optional<QStringList> Ini::sectionParts(const QString &line) const
{
    const QString t = line.trimmed();
    if (!t.startsWith(QLatin1Char('[')) || !t.endsWith(QLatin1Char(']'))) {
        return {};
    }
    return t.mid(1, t.size() - 2).split(QStringLiteral("]["));
}

QString Ini::makeHeader(const QStringList &parts) const
{
    return QLatin1Char('[') + parts.join(QStringLiteral("][")) + QLatin1Char(']');
}

std::optional<QString> Ini::keyOf(const QString &line) const
{
    const QString t = line.trimmed();
    if (t.isEmpty() || t.startsWith(QLatin1Char('[')) || t.startsWith(QLatin1Char('#'))) {
        return std::nullopt;
    }
    const int eq = t.indexOf(QLatin1Char('='));
    if (eq <= 0) {
        return std::nullopt;
    }
    return t.left(eq);
}

} // namespace plasmaapply

using plasmaapply::configPath;
using plasmaapply::computeVideoUrls;
using plasmaapply::fileUrl;
using plasmaapply::kDesktopIdsScript;
using plasmaapply::kImagePluginId;
using plasmaapply::kRebornPluginId;
using plasmaapply::pluginSection;
using plasmaapply::rawVideoUrl;
using plasmaapply::Ini;

PlasmaApply::PlasmaApply(QObject *parent)
    : QObject(parent)
{
}

bool PlasmaApply::rebornAvailable() const
{
    const QStringList dirs = {
        QDir::homePath() + QStringLiteral("/.local/share/plasma/wallpapers"),
        QStringLiteral("/usr/share/plasma/wallpapers"),
    };
    for (const QString &dir : dirs) {
        if (QFileInfo::exists(dir + QLatin1Char('/') + QString::fromLatin1(kRebornPluginId))) {
            return true;
        }
    }
    return false;
}

void PlasmaApply::applyWallpaper(const QString &type, const QString &path, bool includeLockscreen)
{
    // 1. Resolve the target plugin and its config values.
    QString plugin = QString::fromLatin1(kImagePluginId);
    QString imageUrl;
    QString videoEntryUrl;
    QString videoLastUrl;
    if (type == QLatin1String("video")) {
        if (rebornAvailable()) {
            plugin = QString::fromLatin1(kRebornPluginId);
            // The playlist entry is percent-encoded, the LastVideo key is
            // kept raw, as the plugin UI writes them (D9).
            videoEntryUrl = fileUrl(path);
            videoLastUrl = rawVideoUrl(path);
        } else {
            // Without the reborn plugin the cached first frame stands in
            // for the video (D11).
            WallpapersInfo *info = WallpapersInfo::instance();
            const QString frame = info ? info->firstFrameFor(path) : QString();
            if (frame.isEmpty()) {
                emit result(false, QStringLiteral("The video wallpaper plugin is not installed and no cached first frame exists; the video cannot be applied."));
                return;
            }
            imageUrl = fileUrl(frame);
        }
    } else {
        imageUrl = fileUrl(path);
    }

    // 2. Desktops: live from the shell, falling back to the appletsrc
    // sections.
    const QString appletsrcPath = configPath(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"));
    Ini appletsrc = Ini::fromFile(appletsrcPath);
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                         QStringLiteral("/PlasmaShell"),
                         QStringLiteral("org.kde.PlasmaShell"));
    const bool dbusAvailable = shell.isValid();
    QStringList desktops;
    if (dbusAvailable) {
        QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), QString::fromLatin1(kDesktopIdsScript));
        if (reply.isValid()) {
            desktops = reply.value().trimmed().split(QLatin1Char(','), Qt::SkipEmptyParts);
        }
    }
    if (desktops.isEmpty()) {
        desktops = appletsrc.desktopIds();
    }
    if (desktops.isEmpty()) {
        emit result(false, QStringLiteral("No desktops found to set the wallpaper on."));
        return;
    }

    // 3. Per desktop: D-Bus first, an appletsrc edit for the failures (D9).
    QStringList failed;
    int appliedDbus = 0;
    for (const QString &id : desktops) {
        const int screen = appletsrc.get({QStringLiteral("Containments"), id},
                                         QStringLiteral("lastScreen"))
                               .toInt();
        QVariantMap params;
        if (plugin == QLatin1String(kRebornPluginId)) {
            const QStringList wsec = pluginSection(id.toInt(), plugin);
            params.insert(QStringLiteral("VideoUrls"),
                          computeVideoUrls(appletsrc.get(wsec, QStringLiteral("VideoUrls")), videoEntryUrl));
            params.insert(QStringLiteral("LastVideo"), videoLastUrl);
            params.insert(QStringLiteral("LastVideoPosition"), 0);
        } else {
            params.insert(QStringLiteral("Image"), imageUrl);
        }
        bool ok = false;
        if (dbusAvailable) {
            QDBusReply<void> reply =
                shell.call(QStringLiteral("setWallpaper"), plugin, params, static_cast<quint32>(screen));
            ok = reply.isValid();
        }
        if (ok) {
            ++appliedDbus;
        } else {
            failed.append(id);
        }
    }

    if (!failed.isEmpty()) {
        if (appliedDbus > 0) {
            // The shell may have rewritten the file while applying through
            // D-Bus; reread it before editing (D9).
            QThread::msleep(300);
            appletsrc = Ini::fromFile(appletsrcPath);
        }
        for (const QString &id : failed) {
            appletsrc.set({QStringLiteral("Containments"), id},
                          QStringLiteral("wallpaperplugin"), plugin);
            if (plugin == QLatin1String(kRebornPluginId)) {
                const QStringList wsec = pluginSection(id.toInt(), plugin);
                appletsrc.set(wsec,
                              QStringLiteral("VideoUrls"),
                              computeVideoUrls(appletsrc.get(wsec, QStringLiteral("VideoUrls")), videoEntryUrl));
                appletsrc.set(wsec, QStringLiteral("LastVideo"), videoLastUrl);
                appletsrc.set(wsec, QStringLiteral("LastVideoPosition"), QStringLiteral("0"));
            } else {
                appletsrc.set(pluginSection(id.toInt(), plugin),
                              QStringLiteral("Image"),
                              imageUrl);
            }
        }
        if (!appletsrc.save(appletsrcPath)) {
            emit result(false, QStringLiteral("Could not write %1").arg(appletsrcPath));
            return;
        }
    }

    // 4. Optional lock screen (D15): one wallpaper for all screens.
    if (includeLockscreen) {
        const QString lockscreenPath = configPath(QStringLiteral("kscreenlockerrc"));
        Ini lockscreen = Ini::fromFile(lockscreenPath);
        lockscreen.set({QStringLiteral("Greeter")}, QStringLiteral("WallpaperPlugin"), plugin);
        const QStringList lsec = {QStringLiteral("Greeter"), QStringLiteral("Wallpaper"), plugin, QStringLiteral("General")};
        if (plugin == QLatin1String(kRebornPluginId)) {
            lockscreen.set(lsec,
                           QStringLiteral("VideoUrls"),
                           computeVideoUrls(lockscreen.get(lsec, QStringLiteral("VideoUrls")), videoEntryUrl));
            lockscreen.set(lsec, QStringLiteral("LastVideo"), videoLastUrl);
            lockscreen.set(lsec, QStringLiteral("LastVideoPosition"), QStringLiteral("0"));
        } else {
            lockscreen.set(lsec, QStringLiteral("Image"), imageUrl);
        }
        if (!lockscreen.save(lockscreenPath)) {
            emit result(false, QStringLiteral("Could not write %1").arg(lockscreenPath));
            return;
        }
    }

    emit result(true, QStringLiteral("Wallpaper set."));
}
