#pragma once

#include <QList>
#include <QString>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <optional>
#include <utility>


// Pure helpers: URL encoding, VideoUrls playlist computation, and a
// byte-preserving editor for the Plasma rc files.
namespace plasmaapply {

constexpr char kImagePluginId[] = "org.kde.image";
constexpr char kRebornPluginId[] = "luisbocanegra.smart.video.wallpaper.reborn";

// JS for the PlasmaShell evaluateScript D-Bus method; Ini::desktopIds() is
// the appletsrc fallback.
constexpr char kDesktopIdsScript[] = "var d = desktops(); var r = []; "
                                 "for (var i = 0; i < d.length; i++) { "
                                 "if (d[i].formFactor == 0) { r.push(d[i].id); } } "
                                 "print(r.join(','));";
constexpr char kScreenGeomsScript[] = "var r = []; "
                                  "for (var i = 0; i < screenCount; i++) { "
                                  "var g = screenGeometry(i); "
                                  "r.push(i + ':' + g.x + ',' + g.y + ',' + g.width + ',' + g.height); } "
                                  "print(r.join(';'));";

// "file://" + percent-encoding over Plasma's own unreserved set
// (A-Za-z0-9-_.~/), byte-identical to what Plasma writes into config.
QString fileUrl(const QString &path);

// "file://" + path, unencoded (QUrl::toString PreferredEncoding): the form
// the reborn plugin's config page stores in playlist entries and LastVideo.
QString rawVideoUrl(const QString &path);

// Compact VideoUrls JSON: target enabled, rest disabled, order kept, target appended
// when missing; field order = the plugin page's JSON.stringify, so it round-trips byte-identically.
QString computeVideoUrls(const QString &existingJson, const QString &targetUrl);

// Line-based rc editor: untouched lines kept byte-for-byte; save() is
// atomic (same-dir tmp + rename).
class Ini
{
public:
    Ini() = default;
    explicit Ini(const QString &content);

    static Ini fromFile(const QString &path);

    bool save(const QString &path) const;
    QString render() const;

    // Empty when <section>/<key> is absent. <section> e.g.
    // {"Containments","313","Wallpaper","org.kde.image","General"}.
    QString get(const QStringList &section, const QString &key) const;
    void set(const QStringList &section, const QString &key, const QString &value);

    QStringList desktopIds() const;

    QVector<QStringList> sectionsWithKey(const QString &key) const;

private:
    // [section parts] -> {header line, next section line or size}.
    std::optional<std::pair<int, int>> findSection(const QStringList &parts) const;
    std::optional<QStringList> sectionParts(const QString &line) const;
    QString makeHeader(const QStringList &parts) const;
    std::optional<QString> keyOf(const QString &line) const;

    QList<QString> m_lines;
};

} // namespace plasmaapply

// Wallpaper application: PlasmaShell D-Bus setWallpaper per desktop, falling
// back to a direct appletsrc edit when D-Bus is unavailable or errors.
class PlasmaApply : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaApply(QObject *parent = nullptr);

    bool rebornAvailable() const;

    // <type> "image"/"video"; a video without the reborn plugin falls back to
    // its cached first frame. <includeLockscreen> also writes kscreenlockerrc.
    Q_INVOKABLE void applyWallpaper(const QString &type, const QString &path,
                                    bool includeLockscreen);

    // Drops VideoUrls entries whose file:// URL is gone from disk; returns the
    // input unchanged when nothing was removed.
    QString pruneVideoUrls(const QString &existingJson) const;
    // Writes the appletsrc only when pruning changed something.
    Q_INVOKABLE void pruneDeadVideoUrls();

    // kscreenlockerrc greeter background, one wallpaper for all screens; <videoEntryUrl>
    // is percent-encoded, <videoLastUrl> the raw form (video plugin only).
    bool mirrorLockscreen(const QString &plugin, const QString &imageUrl,
                          const QString &videoEntryUrl,
                          const QString &videoLastUrl);

signals:
    void result(bool allOk, const QString &message);
};
