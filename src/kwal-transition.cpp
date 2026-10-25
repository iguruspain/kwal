// kwal-transition: standalone wallpaper transition binary (see
// KWAL_TRANSICIONES_PLAN.md). It runs the layer-shell overlay, performs the
// Plasma switch and the holds, and exits 0 when the switch succeeded. The
// app calls it through QProcess; it can also be run by hand to test a
// transition against the live desktop.
#include "plasmaapply.h"
#include "transitions/effects.h"
#include "transitions/overlay.h"
#include "transitions/outputs.h"
#include "transitions/planes.h"
#include "transitions/transitionspec.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStandardPaths>

#include <wayland-client.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

using plasmaapply::kDesktopIdsScript;
using plasmaapply::kImagePluginId;
using plasmaapply::kRebornPluginId;
using plasmaapply::kScreenGeomsScript;
using plasmaapply::Ini;
using transitions::Paint;
using transitions::Plane;
using transitions::Output;
using transitions::Overlay;
using transitions::Spec;
using transitions::TransitionConfig;

// The overlay hides the wallpaper switch for the time the new wallpaper
// takes to load (hold constants of the Rust reference).
constexpr int kRebornHoldMs = 1500;
constexpr int kImageHoldMs = 400;

namespace {

void usage(const QCoreApplication &app)
{
    std::fprintf(stderr,
                 "usage: %s --type image|video --file <path>\n"
                 "       [--frame <path>] [--no-transition] [--advanced]\n"
                 "       [--layer bottom|top|background|overlay] [--effect <type>]\n"
                 "       [--duration <s>] [--fps <n>] [--step <n>] [--angle <deg>]\n"
                 "       [--pos <x,y>] [--invert-y] [--bezier <x1,y1,x2,y2>]\n"
                 "       [--wave-width <w>] [--wave-height <h>]\n",
                 app.applicationName().toUtf8().constData());
}

// Parses the command line; returns false on an error (usage already shown).
bool parseArgs(int argc, char **argv, const QCoreApplication &app, QString &type,
               QString &filePath, QString &framePath, TransitionConfig &cfg)
{
    cfg.enabled = true;
    for (int i = 1; i < argc; i++) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        auto value = [&](bool &ok) -> QString {
            ok = i + 1 < argc;
            return ok ? QString::fromLocal8Bit(argv[++i]) : QString();
        };
        bool ok = true; // Only value-taking options can fail the check.
        if (arg == QLatin1String("--type")) {
            type = value(ok);
        } else if (arg == QLatin1String("--file")) {
            filePath = value(ok);
        } else if (arg == QLatin1String("--frame")) {
            framePath = value(ok);
        } else if (arg == QLatin1String("--no-transition")) {
            cfg.enabled = false;
        } else if (arg == QLatin1String("--advanced")) {
            cfg.advancedEnabled = true;
        } else if (arg == QLatin1String("--layer")) {
            cfg.layer = value(ok);
        } else if (arg == QLatin1String("--effect")) {
            cfg.type = value(ok);
        } else if (arg == QLatin1String("--duration")) {
            cfg.duration = value(ok).toDouble(&ok);
        } else if (arg == QLatin1String("--fps")) {
            cfg.fps = value(ok).toInt(&ok);
        } else if (arg == QLatin1String("--step")) {
            cfg.step = value(ok).toInt(&ok);
        } else if (arg == QLatin1String("--angle")) {
            cfg.angle = value(ok).toDouble(&ok);
        } else if (arg == QLatin1String("--pos")) {
            cfg.pos = value(ok);
        } else if (arg == QLatin1String("--invert-y")) {
            cfg.invertY = true;
        } else if (arg == QLatin1String("--bezier")) {
            cfg.bezier = value(ok);
        } else if (arg == QLatin1String("--wave-width")) {
            cfg.waveWidth = value(ok).toDouble(&ok);
        } else if (arg == QLatin1String("--wave-height")) {
            cfg.waveHeight = value(ok).toDouble(&ok);
        } else if (arg == QLatin1String("--help")) {
            usage(app);
            exit(0);
        } else {
            std::fprintf(stderr, "kwal-transition: unknown option %s\n",
                         arg.toUtf8().constData());
            usage(app);
            return false;
        }
        if (!ok) {
            std::fprintf(stderr, "kwal-transition: missing value for %s\n",
                         arg.toUtf8().constData());
            usage(app);
            return false;
        }
    }
    if (type != QLatin1String("image") && type != QLatin1String("video")
        || filePath.isEmpty()) {
        usage(app);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // This system's Qt build has a silent default message handler (qWarning
    // and co. print nothing from standalone processes; verified with a bare
    // test program), so route Qt messages to stderr explicitly — a CLI
    // binary must not swallow them.
    qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext &, const QString &msg) {
            std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
        });
    QString type;
    QString filePath;
    QString framePath;
    TransitionConfig cfg;
    if (!parseArgs(argc, argv, app, type, filePath, framePath, cfg)) {
        return 2;
    }

    std::fprintf(stderr, "kwal-transition: start type=%s file=%s frame=%s transition=%d\n",
                 type.toUtf8().constData(), filePath.toUtf8().constData(),
                 framePath.toUtf8().constData(), cfg.enabled ? 1 : 0);

    PlasmaApply plasma;

    // Resolve the target plugin and what the overlay paints, mirroring
    // PlasmaApply step 1: the image is itself; a video is covered by its
    // first frame (the app resolves it from WallpapersInfo and passes it).
    QString plugin = QString::fromLatin1(kImagePluginId);
    QString coverImage;
    if (type == QLatin1String("video")) {
        if (plasma.rebornAvailable()) {
            plugin = QString::fromLatin1(kRebornPluginId);
        }
        coverImage = framePath;
    } else {
        coverImage = filePath;
    }

    // Target desktops: live from the shell, falling back to the appletsrc
    // sections (shared code from PlasmaApply).
    const QString appletsrcPath
        = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
          + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
    const Ini appletsrc = Ini::fromFile(appletsrcPath);
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                         QStringLiteral("/PlasmaShell"),
                         QStringLiteral("org.kde.PlasmaShell"));
    const bool dbusAvailable = shell.isValid();
    QStringList desktops;
    if (dbusAvailable) {
        QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"),
                                               QString::fromLatin1(kDesktopIdsScript));
        if (reply.isValid()) {
            desktops = reply.value().trimmed().split(QLatin1Char(','), Qt::SkipEmptyParts);
        }
    }
    if (desktops.isEmpty()) {
        desktops = appletsrc.desktopIds();
    }
    if (desktops.isEmpty()) {
        std::fprintf(stderr, "kwal-transition: no desktops found to set the wallpaper on\n");
        return 1;
    }

    // Direction: the image target and the frame fallback are covered by the
    // incoming image (Cover); the reborn video freezes the outgoing image on
    // top (Reveal) only when every target desktop currently shows an image
    // wallpaper, otherwise the incoming first frame covers it.
    bool reveal = false;
    if (type == QLatin1String("video") && plugin == QLatin1String(kRebornPluginId)) {
        bool allImage = true;
        for (const QString &id : desktops) {
            if (transitions::outgoingPlugin(appletsrc, id) != QLatin1String(kImagePluginId)) {
                allImage = false;
                break;
            }
        }
        reveal = allImage;
    }

    // Overlay setup. Any failure degrades to a plain switch.
    Spec spec;
    Overlay overlay;
    std::fprintf(stderr, "kwal-transition: reveal=%d cover=%s\n", reveal ? 1 : 0,
                 coverImage.toUtf8().constData());
    if (cfg.enabled && !coverImage.isEmpty()) {
        std::vector<Plane> planes
            = reveal ? transitions::outgoingPlanes(appletsrc, desktops)
                     : transitions::incomingPlanes(appletsrc, desktops, coverImage, plugin);
        if (planes.empty()) {
            std::fprintf(stderr, "kwal-transition: no planes; plain switch\n");
        } else {
            std::fprintf(stderr, "kwal-transition: listOutputs: start\n");
            struct wl_display *enumDisplay = wl_display_connect(nullptr);
            std::vector<Output> outputs
                = enumDisplay ? transitions::listOutputs(enumDisplay) : std::vector<Output>();
            if (enumDisplay) {
                wl_display_disconnect(enumDisplay);
            }
            std::fprintf(stderr, "kwal-transition: listOutputs: done outputs=%zu\n", outputs.size());
            if (dbusAvailable) {
                QDBusReply<QString> reply
                    = shell.call(QStringLiteral("evaluateScript"),
                                 QString::fromLatin1(kScreenGeomsScript));
                if (reply.isValid()) {
                    if (!transitions::assignPlasmaIds(outputs,
                                                      transitions::parseScreenGeoms(reply.value().trimmed()))) {
                        std::fprintf(stderr,
                                     "kwal-transition: could not match Plasma screens to the outputs; "
                                     "assuming compositor order\n");
                    }
                }
            }
            if (outputs.empty()) {
                std::fprintf(stderr, "kwal-transition: no outputs enumerated; falling back to a plain switch\n");
            } else {
                spec = transitions::resolve(cfg);
                const Paint paint = reveal ? Paint::Reveal : Paint::Cover;
                std::fprintf(stderr, "kwal-transition: begin: start paint=%d\n",
                             paint == Paint::Reveal ? 1 : 0);
                if (!overlay.begin(outputs, planes, transitions::parseSurfaceLayer(cfg.layer), paint)) {
                    std::fprintf(stderr, "kwal-transition: overlay setup failed; falling back to a plain switch\n");
                } else if (paint == Paint::Cover) {
                    // Paint the incoming image until opaque before the
                    // switch (the old wallpaper keeps playing below).
                    overlay.animate(spec);
                    std::fprintf(stderr, "kwal-transition: animate: done\n");
                }
            }
        }
    }

    // The Plasma switch (D-Bus setWallpaper per desktop, appletsrc fallback).
    bool switchOk = false;
    QString switchMsg;
    QObject::connect(&plasma, &PlasmaApply::result, [&](bool ok, const QString &msg) {
        switchOk = ok;
        switchMsg = msg;
    });
    // Never the lock screen: the app mirrors it after the binary succeeds
    // (the lockscreen is never a parameter of the transition command).
    plasma.applyWallpaper(type, filePath, false);
    std::fprintf(stderr, "kwal-transition: switch ok=%d msg=%s\n", switchOk ? 1 : 0,
                 switchMsg.toUtf8().constData());

    // Transition end: hold for the new wallpaper to load (1500 ms for the
    // reborn video, 400 ms for image/frame), then reveal the outgoing image
    // (Reveal) or drop the overlay (Cover). A failed switch drops the
    // overlay as fast as possible.
    if (overlay.active()) {
        if (switchOk) {
            if (reveal) {
                overlay.finish(spec, kRebornHoldMs);
            } else {
                overlay.close(kImageHoldMs);
            }
        } else {
            overlay.close(0);
        }
    }

    if (!switchOk) {
        std::fprintf(stderr, "kwal-transition: %s\n", switchMsg.toUtf8().constData());
    }
    return switchOk ? 0 : 1;
}
