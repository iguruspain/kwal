#pragma once

#include <QList>
#include <QString>
#include <QObject>
#include <QStringList>

#include <optional>
#include <utility>


// Pure helpers for the Plasma wallpaper application pipeline: file URL
// encoding, the video playlist computation, and a byte-preserving linear
// editor for the appletsrc/kscreenlockerrc rc files.
namespace plasmaapply {

// Plasma wallpaper plugin ids (org.kde.image and the "Smart Video Wallpaper
// (reborn)" video plugin).
constexpr char kImagePluginId[] = "org.kde.image";
constexpr char kRebornPluginId[] = "luisbocanegra.smart.video.wallpaper.reborn";

// Shell scripting snippets for the `evaluateScript` D-Bus method: the
// live desktop ids and the logical geometry of every Plasma screen
// ("id:x,y,w,h;…"). The appletsrc parse (Ini::desktopIds) is the fallback.
constexpr char kDesktopIdsScript[] = "var d = desktops(); var r = []; "
                                 "for (var i = 0; i < d.length; i++) { "
                                 "if (d[i].formFactor == 0) { r.push(d[i].id); } } "
                                 "print(r.join(','));";
constexpr char kScreenGeomsScript[] = "var r = []; "
                                  "for (var i = 0; i < screenCount; i++) { "
                                  "var g = screenGeometry(i); "
                                  "r.push(i + ':' + g.x + ',' + g.y + ',' + g.width + ',' + g.height); } "
                                  "print(r.join(';'));";

// "file://" + percent-encoded path, preserving A-Za-z0-9-_.~/ (the same
// encoding the Rust reference and Plasma write into config values).
QString fileUrl(const QString &path);

// "file://" + path, unencoded: the value the video plugin stores in its
// LastVideo key.
QString rawVideoUrl(const QString &path);

// Computes the video plugin's VideoUrls JSON (compact, no whitespace):
// the target video is enabled, every other entry disabled, entry order
// preserved, and the target appended with default fields when missing.
// Fields are emitted in the plugin page's own serialization order (its
// JSON.stringify round-trip), so the stored string stays byte-identical and
// the page never reports unsaved changes. <existingJson> is the current
// VideoUrls value ("" when absent).
QString computeVideoUrls(const QString &existingJson, const QString &targetUrl);

// Line-based rc file editor: preserves byte-for-byte everything it does
// not touch (comments, spacing, unknown keys); writes are atomic (tmp file
// in the same directory + rename).
class Ini
{
public:
    Ini() = default;
    explicit Ini(const QString &content);

    // Loads <path>; a missing file yields an empty Ini.
    static Ini fromFile(const QString &path);

    bool save(const QString &path) const;
    QString render() const;

    // Value of <key> in <section>; empty when the section or key is
    // absent. <section> is a list like
    // {"Containments", "313", "Wallpaper", "org.kde.image", "General"}.
    QString get(const QStringList &section, const QString &key) const;
    void set(const QStringList &section, const QString &key, const QString &value);

    QStringList desktopIds() const;

private:
    // [section parts] -> {header line, next section line or size}.
    std::optional<std::pair<int, int>> findSection(const QStringList &parts) const;
    std::optional<QStringList> sectionParts(const QString &line) const;
    QString makeHeader(const QStringList &parts) const;
    std::optional<QString> keyOf(const QString &line) const;

    QList<QString> m_lines;
};

} // namespace plasmaapply

// Applies a wallpaper to every desktop: the Plasma Shell D-Bus
// setWallpaper (org.kde.plasmashell) per desktop, falling back to a direct
// appletsrc edit when D-Bus is unavailable or errors, and optionally the
// kscreenlockerrc lock screen.
class PlasmaApply : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaApply(QObject *parent = nullptr);

    // True when the "Smart Video Wallpaper (reborn)" plugin is installed in
    // the user or system Plasma wallpaper directories.
    bool rebornAvailable() const;

    // Applies <path> to every desktop. <type> is "image" or "video"; a
    // video falls back to its cached first frame when the reborn plugin is
    // not installed. With <includeLockscreen>, kscreenlockerrc is written
    // too. Emits result() when the operation finishes.
    Q_INVOKABLE void applyWallpaper(const QString &type, const QString &path,
                                    bool includeLockscreen);

    // Mirrors the applied wallpaper into kscreenlockerrc as the greeter
    // background (one wallpaper for all screens). <plugin> is the Plasma
    // wallpaper plugin id; <imageUrl> is the file URL of the image; for the
    // video plugin <videoEntryUrl> is the percent-encoded playlist entry
    // and <videoLastUrl> the raw LastVideo URL. Returns false when the
    // file cannot be written.
    bool mirrorLockscreen(const QString &plugin, const QString &imageUrl,
                          const QString &videoEntryUrl,
                          const QString &videoLastUrl);

signals:
    void result(bool allOk, const QString &message);
};
