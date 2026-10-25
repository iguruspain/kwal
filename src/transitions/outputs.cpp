#include "outputs.h"

#include <QByteArray>
#include <QDebug>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

#include <wayland-client.h>
#include <wayland-core-client-protocol.h>
#include <xdg-output-unstable-v1-client-protocol.h>

namespace transitions {

std::vector<ScreenGeom> parseScreenGeoms(const QString &s)
{
    std::vector<ScreenGeom> out;
    const QStringList entries = s.trimmed().split(QLatin1Char(';'));
    for (const QString &entry : entries) {
        const int colon = entry.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        bool ok = false;
        const quint32 id = entry.left(colon).trimmed().toUInt(&ok);
        if (!ok) {
            continue;
        }
        const QStringList parts = entry.mid(colon + 1).split(QLatin1Char(','));
        if (parts.size() < 2) {
            continue;
        }
        bool okX = false;
        bool okY = false;
        const double x = parts.at(0).trimmed().toDouble(&okX);
        const double y = parts.at(1).trimmed().toDouble(&okY);
        if (!okX || !okY) {
            continue;
        }
        out.push_back(ScreenGeom{id,
                                 static_cast<int>(std::lround(x)),
                                 static_cast<int>(std::lround(y))});
    }
    return out;
}

bool assignPlasmaIds(std::vector<Output> &outputs, const std::vector<ScreenGeom> &geoms)
{
    std::vector<bool> used(geoms.size(), false);
    std::vector<quint32> ids;
    ids.reserve(outputs.size());
    for (const auto &m : outputs) {
        bool found = false;
        for (size_t i = 0; i < geoms.size(); ++i) {
            if (!used[i] && geoms[i].x == m.x && geoms[i].y == m.y) {
                used[i] = true;
                ids.push_back(geoms[i].id);
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    for (size_t i = 0; i < outputs.size(); ++i) {
        outputs[i].id = ids[i];
    }
    std::sort(outputs.begin(), outputs.end(),
              [](const Output &a, const Output &b) { return a.id < b.id; });
    return true;
}

namespace {

struct OutputInfo {
    QString name;
    QString description;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    int scale = 1;
    // Logical position from xdg-output, preferred over x/y.
    int logicalX = 0;
    int logicalY = 0;
    bool hasLogical = false;
    bool done = false;
};

struct OutputCollect {
    struct zxdg_output_manager_v1 *manager = nullptr;
    // Bound wl_output proxies, in announcement order (one per infos entry).
    std::vector<wl_output *> outputs;
    // Deque: wayland listener `user_data` points at an element, and the
    // addresses must stay stable across emplace_back (a vector would
    // reallocate, leaving earlier listeners with a dangling pointer).
    std::deque<OutputInfo> infos;
};

// The C client stores the raw listener-array pointer in the proxy (no
// copy), so every struct registered below must outlive the proxies it
// binds: those stay alive on the display until disconnect, far past this
// function's stack frame. Stack locals here dangle (SIGSEGV in the libffi
// dispatch once the compositor reuses the freed frame).
const struct wl_output_listener kOutputListener = {
    +[](void *userData, struct wl_output *, int32_t x, int32_t y, int32_t, int32_t,
        int32_t, const char *, const char *, int32_t) {
        // Fallback position; xdg-output overrides it below.
        auto *info = static_cast<OutputInfo *>(userData);
        info->x = x;
        info->y = y;
    },
    +[](void *userData, struct wl_output *, uint32_t flags, int32_t width, int32_t height,
        int32_t) {
        // Pixel size comes from the current mode (geometry
        // only has physical mm dimensions).
        if (flags & WL_OUTPUT_MODE_CURRENT) {
            auto *info = static_cast<OutputInfo *>(userData);
            info->width = std::max(width, 1);
            info->height = std::max(height, 1);
        }
    },
    +[](void *userData, struct wl_output *) {
        static_cast<OutputInfo *>(userData)->done = true;
    },
    +[](void *userData, struct wl_output *, int32_t factor) {
        static_cast<OutputInfo *>(userData)->scale = factor;
    },
    +[](void *userData, struct wl_output *, const char *name) {
        auto *info = static_cast<OutputInfo *>(userData);
        info->name = name ? QString::fromUtf8(name) : QString();
    },
    +[](void *userData, struct wl_output *, const char *description) {
        auto *info = static_cast<OutputInfo *>(userData);
        info->description = description ? QString::fromUtf8(description)
                                        : QString();
    },
};

// wayland-scanner generates raw-parameter listeners (no event structs).
const struct zxdg_output_v1_listener kXdgOutputListener = {
    +[](void *data, struct zxdg_output_v1 *, int32_t x, int32_t y) {
        auto *info = static_cast<OutputInfo *>(data);
        info->logicalX = x;
        info->logicalY = y;
        info->hasLogical = true;
    },
    +[](void *, struct zxdg_output_v1 *, int32_t, int32_t) {},
    +[](void *, struct zxdg_output_v1 *) {},
    +[](void *, struct zxdg_output_v1 *, const char *) {},
    +[](void *, struct zxdg_output_v1 *, const char *) {},
};

const struct wl_registry_listener kRegistryListener = {
    +[](void *data, struct wl_registry *reg, uint32_t name, const char *interface,
        uint32_t version) {
        auto *c = static_cast<OutputCollect *>(data);
        if (std::string_view(interface) == "wl_output") {
            c->infos.emplace_back();
            c->outputs.push_back(static_cast<struct wl_output *>(
                wl_registry_bind(reg, name, &wl_output_interface, std::min(version, 4u))));
            wl_output_add_listener(c->outputs.back(), &kOutputListener, &c->infos.back());
        } else if (std::string_view(interface) == "zxdg_output_manager_v1") {
            c->manager =
                reinterpret_cast<struct zxdg_output_manager_v1 *>(
                    wl_registry_bind(reg, name, &zxdg_output_manager_v1_interface,
                                     std::min(version, 3u)));
        }
    },
};

} // namespace

std::vector<Output> listOutputs(struct wl_display *display)
{
    OutputCollect collect;
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &kRegistryListener, &collect);
    wl_display_roundtrip(display);

    // Ask xdg-output (when offered) for each output's *logical* position;
    // `wl_output.geometry` is not reliable for that (wlroots always reports
    // 0,0). The roundtrip delivers its events.
    if (collect.manager) {
        for (size_t i = 0; i < collect.outputs.size(); ++i) {
            zxdg_output_v1 *zo =
                zxdg_output_manager_v1_get_xdg_output(collect.manager, collect.outputs[i]);
            zxdg_output_v1_add_listener(zo, &kXdgOutputListener, &collect.infos[i]);
        }
        wl_display_roundtrip(display);
    }

    // Process events until every wl_output reports done (capped so we
    // cannot hang if an output never completes).
    for (int i = 0; i < 100; ++i) {
        if (wl_display_flush(display) != 0) {
            break;
        }
        bool all = !collect.infos.empty();
        for (const auto &info : collect.infos) {
            all = all && info.done;
        }
        if (all) {
            break;
        }
        if (wl_display_dispatch(display) < 0) {
            break;
        }
    }

    std::vector<Output> mons;
    mons.reserve(collect.infos.size());
    for (const auto &info : collect.infos) {
        // An output without a current mode (a connected but inactive
        // monitor) has no Plasma screen and cannot hold a plane: keeping
        // it would skew the id matching in assignPlasmaIds.
        if (info.width <= 0 || info.height <= 0) {
            qWarning("transition: skipping output %s with no current mode",
                     info.name.toUtf8().constData());
            continue;
        }
        mons.push_back(Output{static_cast<quint32>(mons.size()),
                              info.name,
                              info.description,
                              info.width,
                              info.height,
                              info.hasLogical ? info.logicalX : info.x,
                              info.hasLogical ? info.logicalY : info.y,
                              info.scale});
    }
    return mons;
}

} // namespace transitions
