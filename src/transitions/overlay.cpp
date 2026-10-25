#include "overlay.h"
#include "perf.h"

#include <QDebug>

#include <wayland-client.h>
#include <wayland-core-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace transitions {

namespace {

constexpr char kLayerNamespace[] = "kwal-transition";
constexpr int kEventTimeoutMs = 2000;
// Fade-out before destroying a surface that still shows the image.
constexpr double kCloseFadeSec = 0.15;
constexpr int kMaxDispatchRounds = 300;

// Draws one frame of `effect` at `progress` ∈ [0, 1] into `buf`, which must
// hold `orig` or an earlier frame of the same animation. `w`/`h` are the
// buffer size in pixels, `logicalW`/`logicalH` the surface's logical size
// (for pixel positions). `done` forces the exact end state.
void renderFrame(const Effect &effect, Paint paint, std::span<Pixel> buf,
                 const std::span<const Pixel> orig, int w, int h, int logicalW,
                 int logicalH, double progress, bool done)
{
    switch (effect.type) {
    case EffectType::None:
    case EffectType::Simple:
    case EffectType::Fade: {
        // `keep` = share of the held image that stays visible.
        const std::uint32_t keep =
            done ? (paint == Paint::Reveal ? 0u : 256u)
                 : static_cast<std::uint32_t>(std::lround(
                       (paint == Paint::Reveal ? 1.0 - progress : progress) * 256.0));
        drawFade(buf, orig, keep);
        break;
    }
    case EffectType::Wipe:
    case EffectType::Wave: {
        double waveW = 20.0;
        double waveH = 0.0;
        if (effect.type == EffectType::Wave) {
            waveW = effect.waveWidth;
            waveH = std::abs(effect.waveHeight);
        }
        // Travel direction: 0° = right→left, 90° = top→bottom (y down).
        const double a = effect.angle * M_PI / 180.0;
        const double dirX = -std::cos(a);
        const double dirY = std::sin(a);
        const auto [lo, hi] = projRange(w, h, dirX, dirY);
        const double m = waveH + 1.0;
        const double front =
            done ? hi + m : (lo - m) + progress * ((hi + m) - (lo - m));
        drawWipe(paint, buf, orig, w, h, dirX, dirY, front, waveW, waveH);
        break;
    }
    case EffectType::Grow: {
        const auto [cx, cy] = effect.pos.toPx(effect.invertY, logicalW, logicalH, w, h);
        const double rMax = maxRadius(cx, cy, w, h);
        const double r = done ? rMax + 2.0 : progress * rMax;
        drawGrow(paint, buf, orig, w, h, cx, cy, r);
        break;
    }
    case EffectType::Outer: {
        const auto [cx, cy] = effect.pos.toPx(effect.invertY, logicalW, logicalH, w, h);
        const double rMax = maxRadius(cx, cy, w, h) + 1.0;
        const double r = done ? -1.0 : (1.0 - progress) * rMax - progress;
        drawOuter(paint, buf, orig, w, h, cx, cy, r);
        break;
    }
    }
}

// Creates a memfd holding two copies of `argb` (double buffering) with a RW
// mapping.
bool makeMemfd(const Pixel *argb, size_t count, int *fdOut, uint8_t **memOut,
               size_t *bufLenOut, size_t *poolLenOut)
{
    const size_t bufLen = count * sizeof(Pixel);
    const size_t poolLen = bufLen * 2;
    const int fd = ::memfd_create("kwal-transition", 0);
    if (fd < 0) {
        return false;
    }
    if (::ftruncate(fd, static_cast<off_t>(poolLen)) != 0) {
        ::close(fd);
        return false;
    }
    void *mem = ::mmap(nullptr, poolLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        ::close(fd);
        return false;
    }
    std::memcpy(mem, argb, bufLen);
    std::memcpy(static_cast<uint8_t *>(mem) + bufLen, argb, bufLen);
    *fdOut = fd;
    *memOut = static_cast<uint8_t *>(mem);
    *bufLenOut = bufLen;
    *poolLenOut = poolLen;
    return true;
}

bool dispatchWait(OverlayState &st, int timeoutMs)
{
    struct wl_display *d = st.display;

    // 1. Deliver what is already queued (prepare_read fails while events
    //    are pending on the default queue).
    while (wl_display_prepare_read(d) != 0) {
        if (wl_display_dispatch_pending(d) < 0) {
            return false;
        }
    }

    // 2. Send our pending requests BEFORE sleeping. Polling first made the
    //    compositor wait for requests that were still in our buffer, so
    //    every round trip cost the whole timeout.
    short events = POLLIN;
    if (wl_display_flush(d) < 0) {
        if (errno != EAGAIN) {
            wl_display_cancel_read(d);
            return false;
        }
        events |= POLLOUT; // socket buffer full: wait until it drains
    }

    // 3. Sleep until the compositor answers (or the timeout).
    struct pollfd pfd;
    pfd.fd = wl_display_get_fd(d);
    pfd.events = events;
    pfd.revents = 0;
    int n;
    do {
        n = ::poll(&pfd, 1, timeoutMs);
    } while (n < 0 && errno == EINTR);

    if (n > 0 && (pfd.revents & POLLIN)) {
        if (wl_display_read_events(d) < 0) {
            return false;
        }
    } else {
        wl_display_cancel_read(d);
        if (n < 0) {
            return false;
        }
    }
    if (wl_display_dispatch_pending(d) < 0) {
        return false;
    }
    // Requests queued by the listeners we just ran.
    if (wl_display_flush(d) < 0 && errno != EAGAIN) {
        return false;
    }
    return true;
}

const wl_output_listener kOutputListener = {
    +[](void *, struct wl_output *, int32_t, int32_t, int32_t, int32_t, int32_t,
        const char *, const char *, int32_t) {},
    +[](void *, struct wl_output *, uint32_t, int32_t, int32_t, int32_t) {},
    +[](void *data, struct wl_output *out) {
        auto *s = static_cast<OverlayState *>(data);
        for (auto &o : s->outputs) {
            if (o.output == out) {
                o.done = true;
            }
        }
    },
    +[](void *, struct wl_output *, int32_t) {},
    +[](void *data, struct wl_output *out, const char *name) {
        auto *s = static_cast<OverlayState *>(data);
        for (auto &o : s->outputs) {
            if (o.output == out) {
                o.announcedName = name ? QString::fromUtf8(name) : QString();
            }
        }
    },
    +[](void *, struct wl_output *, const char *) {},
};

const wl_registry_listener kRegistryListener = {
    +[](void *data, struct wl_registry *reg, uint32_t name, const char *interface,
        uint32_t version) {
        auto *s = static_cast<OverlayState *>(data);
        if (std::string_view(interface) == "wl_compositor") {
            s->compositor = static_cast<struct wl_compositor *>(
                wl_registry_bind(reg, name, &wl_compositor_interface,
                                 std::min(version, 4u)));
        } else if (std::string_view(interface) == "wl_shm") {
            s->shm = static_cast<struct wl_shm *>(
                wl_registry_bind(reg, name, &wl_shm_interface, 1));
        } else if (std::string_view(interface) == "wl_output") {
            auto *out = static_cast<struct wl_output *>(
                wl_registry_bind(reg, name, &wl_output_interface, std::min(version, 4u)));
            wl_output_add_listener(out, &kOutputListener, s);
            s->outputs.push_back(OverlayState::OutputAnnouncement{name, out, QString()});
        } else if (std::string_view(interface) == "zwlr_layer_shell_v1") {
            s->shell = static_cast<struct zwlr_layer_shell_v1 *>(
                wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 1));
        } else if (std::string_view(interface) == "wp_viewporter") {
            s->viewporter = static_cast<struct wp_viewporter *>(
                wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
        }
    },
    +[](void *, struct wl_registry *, uint32_t) {},
};

const wl_callback_listener kFrameListener = {
    +[](void *data, struct wl_callback *, uint32_t) {
        auto *s = static_cast<OverlayState *>(data);
        s->framesPending = std::max(0, s->framesPending - 1);
    },
};

const wl_buffer_listener kReleaseListener = {
    +[](void *data, struct wl_buffer *) {
        static_cast<std::atomic<bool> *>(data)->store(true, std::memory_order_release);
    },
};

const zwlr_layer_surface_v1_listener kLayerListener = {
    +[](void *data, struct zwlr_layer_surface_v1 *ls, uint32_t serial, uint32_t w,
        uint32_t h) {
        const auto *u = static_cast<const LayerUser *>(data);
        zwlr_layer_surface_v1_ack_configure(ls, serial);
        if (u->index >= 0
            && u->index < static_cast<int>(u->state->logical.size())) {
            u->state->logical[u->index] = {static_cast<int>(w), static_cast<int>(h)};
        }
        u->state->configsPending = std::max(0, u->state->configsPending - 1);
    },
    +[](void *, struct zwlr_layer_surface_v1 *) {},
};

int layerShellValue(SurfaceLayer layer)
{
    switch (layer) {
    case SurfaceLayer::Background:
        return ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
    case SurfaceLayer::Top:
        return ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    case SurfaceLayer::Overlay:
        return ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    case SurfaceLayer::Bottom:
        break;
    }
    return ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
}

} // namespace

Overlay::~Overlay()
{
    teardown();
}

SurfaceLayer parseSurfaceLayer(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QLatin1String("background")) {
        return SurfaceLayer::Background;
    }
    if (v == QLatin1String("top")) {
        return SurfaceLayer::Top;
    }
    if (v == QLatin1String("overlay")) {
        return SurfaceLayer::Overlay;
    }
    return SurfaceLayer::Bottom;
}

bool Overlay::begin(const std::vector<Output> &mons, const std::vector<Plane> &planes,
                    SurfaceLayer layer, Paint paint)
{
    if (m_state || planes.empty()) {
        return false;
    }
    struct wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        return false; // No Wayland session.
    }
    auto st = std::make_unique<OverlayState>();
    st->display = display;
    st->registry = wl_display_get_registry(display);
    if (!st->registry) {
        wl_display_disconnect(display);
        return false;
    }
    wl_registry_add_listener(st->registry, &kRegistryListener, st.get());
    qWarning("perf: %lld begin: connected", perfMs());

    // All global announcements are delivered before the sync roundtrip
    // is acked.
    for (int i = 0; i < 10; ++i) {
        if (!dispatchWait(*st, kEventTimeoutMs)) {
            wl_display_disconnect(display);
            return false;
        }
        if (st->compositor && st->shm && st->shell) {
            break;
        }
    }
    if (!st->compositor || !st->shm || !st->shell) {
        qWarning("transition: missing wl_compositor/wl_shm/zwlr_layer_shell_v1 global; "
                 "no layer-shell support");
        wl_display_disconnect(display);
        return false;
    }
    qWarning("perf: %lld begin: globals", perfMs());
    if (!st->viewporter) {
        qWarning("transition: wp_viewporter missing; falling back to logical-size buffers");
    }
    // Each wl_output sends its initial state (geometry, mode, name,
    // description) right after the bind and closes the stream with `done`.
    // Dispatch until every output is done so the names are ready for the
    // plane matching below (same scheme as the reference implementation).
    for (int i = 0; i < 10; ++i) {
        bool allDone = true;
        for (const auto &o : st->outputs) {
            if (!o.done) {
                allDone = false;
                break;
            }
        }
        if (allDone) {
            break;
        }
        if (!dispatchWait(*st, kEventTimeoutMs)) {
            wl_display_disconnect(display);
            return false;
        }
    }
    qWarning("perf: %lld begin: outputs done", perfMs());

    // Phase 1: create each layer surface, anchor it, and commit WITHOUT a
    // buffer — layer-shell requires a configure/ack round-trip before any
    // buffer is attached.
    struct Pending {
        int plane = 0;
        int mon = 0;
        struct wl_surface *surface = nullptr;
    };
    std::vector<Pending> pendings;
    for (size_t pi = 0; pi < planes.size(); ++pi) {
        const auto &plane = planes[pi];
        int mi = -1;
        for (size_t i = 0; i < mons.size(); ++i) {
            if (mons[i].id == plane.screen) {
                mi = static_cast<int>(i);
                break;
            }
        }
        if (mi < 0) {
            qWarning("transition: no monitor geometry for screen %u; skipping",
                     plane.screen);
            continue;
        }
        const auto &m = mons[static_cast<size_t>(mi)];
        if (m.width <= 0 || m.height <= 0) {
            qWarning("transition: screen %u has no active mode; skipping", plane.screen);
            continue;
        }
        // Match by connector name only: guessing by index could put the
        // overlay on a different monitor than the one being changed.
        int oi = -1;
        for (size_t i = 0; i < st->outputs.size(); ++i) {
            const auto &o = st->outputs[i];
            if (!o.announcedName.isEmpty() && o.announcedName == m.name) {
                oi = static_cast<int>(i);
                break;
            }
        }
        if (oi < 0) {
            qWarning("transition: no wl_output named %s for screen %u; skipping",
                     m.name.toUtf8().constData(), plane.screen);
            continue;
        }

        struct wl_surface *surface = wl_compositor_create_surface(st->compositor);
        if (!surface) {
            continue;
        }
        // Empty input region: clicks go through to the desktop.
        struct wl_region *region = wl_compositor_create_region(st->compositor);
        wl_surface_set_input_region(surface, region);
        wl_region_destroy(region);

        struct zwlr_layer_surface_v1 *ls = zwlr_layer_shell_v1_get_layer_surface(
            st->shell, surface, st->outputs[static_cast<size_t>(oi)].output,
            layerShellValue(layer), kLayerNamespace);
        if (!ls) {
            wl_surface_destroy(surface);
            continue;
        }
        zwlr_layer_surface_v1_set_anchor(
            ls, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                 | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(ls, 0, 0);
        // -1: ignore panels' exclusive zones and cover the whole output.
        zwlr_layer_surface_v1_set_exclusive_zone(ls, -1);
        wl_surface_commit(surface);

        auto u = std::make_unique<LayerUser>();
        u->state = st.get();
        u->index = static_cast<int>(st->layers.size());
        zwlr_layer_surface_v1_add_listener(ls, &kLayerListener, u.get());
        st->layerUsers.push_back(std::move(u));
        st->layers.push_back(ls);
        st->logical.push_back({0, 0});
        st->configsPending += 1;
        pendings.push_back(Pending{static_cast<int>(pi), mi, surface});
    }
    if (pendings.empty()) {
        wl_display_disconnect(display);
        return false;
    }

    // Phase 2: wait for every configure and ack it.
    for (int i = 0; i < kMaxDispatchRounds; ++i) {
        if (st->configsPending == 0) {
            break;
        }
        if (!dispatchWait(*st, kEventTimeoutMs)) {
            wl_display_disconnect(display);
            return false;
        }
    }
    if (st->configsPending != 0) {
        qWarning("transition: layer-shell configure timed out");
        wl_display_disconnect(display);
        return false;
    }
    qWarning("perf: %lld begin: configures done", perfMs());

    // Phase 3: render, attach the first buffer and map the surfaces.
    for (size_t i = 0; i < pendings.size(); ++i) {
        const auto &p = pendings[i];
        const auto &m = mons[static_cast<size_t>(p.mon)];
        const auto &plane = planes[static_cast<size_t>(p.plane)];
        auto s = std::make_unique<OverlaySurface>();
        s->surface = p.surface;
        s->layer = st->layers[i];
        int lw = st->logical[i].first;
        int lh = st->logical[i].second;
        if (lw <= 0 || lh <= 0) {
            lw = m.width;
            lh = m.height;
        }
        // Buffer size: the output's physical mode (crisp at any scale),
        // mapped to the logical size by the viewport. Swap for rotated
        // outputs, where logical and mode orientations differ.
        int bw = m.width;
        int bh = m.height;
        if ((lw > lh) != (bw > bh)) {
            std::swap(bw, bh);
        }
        if (st->viewporter) {
            s->viewport = wp_viewporter_get_viewport(st->viewporter, p.surface);
            if (s->viewport) {
                wp_viewport_set_destination(s->viewport, lw, lh);
            }
        }
        if (!s->viewport) {
            bw = lw;
            bh = lh;
        }

        s->orig = composeImage(plane, bw, bh);
        if (s->orig.empty()) {
            qWarning("transition: cannot decode %s", plane.path.toUtf8().constData());
            wl_display_disconnect(display);
            return false;
        }
        qWarning("perf: %lld begin: composed %dx%d", perfMs(), bw, bh);
        // Reveal starts as the opaque image, Cover as a transparent overlay.
        Buffer blank;
        const Buffer *initial = &s->orig;
        if (paint == Paint::Cover) {
            blank.assign(s->orig.size(), 0);
            initial = &blank;
        }
        if (!makeMemfd(initial->data(), initial->size(), &s->fd, &s->mem, &s->bufLen,
                       &s->poolLen)) {
            qWarning("transition: memfd_create/mmap failed");
            wl_display_disconnect(display);
            return false;
        }
        s->pool = wl_shm_create_pool(st->shm, s->fd, static_cast<int>(s->poolLen));
        s->width = bw;
        s->height = bh;
        s->logical = {lw, lh};
        s->paint = paint;
        s->opaque = (paint == Paint::Reveal); // Reveal starts as the opaque old image
        s->shown = 0;
        s->released[0].store(false, std::memory_order_release);
        s->released[1].store(true, std::memory_order_release);
        s->buffers[0] = wl_shm_pool_create_buffer(s->pool, 0, bw, bh, bw * 4,
                                                  WL_SHM_FORMAT_ARGB8888);
        s->buffers[1] = wl_shm_pool_create_buffer(s->pool, static_cast<int>(s->bufLen), bw,
                                                  bh, bw * 4, WL_SHM_FORMAT_ARGB8888);
        if (!s->buffers[0] || !s->buffers[1]) {
            qWarning("transition: wl_shm_pool_create_buffer failed");
            wl_display_disconnect(display);
            return false;
        }
        wl_buffer_add_listener(s->buffers[0], &kReleaseListener, &s->released[0]);
        wl_buffer_add_listener(s->buffers[1], &kReleaseListener, &s->released[1]);

        wl_surface_attach(p.surface, s->buffers[0], 0, 0);
        wl_surface_damage_buffer(p.surface, 0, 0, bw, bh);
        st->framesPending += 1;
        struct wl_callback *cb = wl_surface_frame(p.surface);
        wl_callback_add_listener(cb, &kFrameListener, st.get());
        wl_surface_commit(p.surface);
        m_surfaces.push_back(std::move(s));
    }
    qWarning("perf: %lld begin: first buffer committed", perfMs());

    m_state = std::move(st);

    // Block until every first frame is composited: the overlay is then
    // opaque on screen and can safely hide the switch.
    for (int i = 0; i < kMaxDispatchRounds; ++i) {
        if (m_state->framesPending == 0) {
            break;
        }
        if (!dispatchWait(*m_state, kEventTimeoutMs)) {
            teardown();
            return false;
        }
    }
    qWarning("perf: %lld begin: first frames done", perfMs());
    return true;
}

bool Overlay::presentFrame(OverlaySurface &s, const Effect &effect, Paint paint,
                           double progress, bool done)
{
    const int next = 1 - s.shown;
    // Wait until the compositor released the buffer we reuse.
    for (int i = 0; i < kMaxDispatchRounds; ++i) {
        if (s.released[next].load(std::memory_order_acquire)) {
            break;
        }
        if (!dispatchWait(*m_state, kEventTimeoutMs)) {
            return false;
        }
    }
    auto *mem = s.mem + static_cast<size_t>(next) * s.bufLen;
    const std::span<Pixel> buf{reinterpret_cast<Pixel *>(mem), s.bufLen / sizeof(Pixel)};
    renderFrame(effect, paint, buf, s.orig, s.width, s.height, s.logical.first,
                s.logical.second, progress, done);

    s.released[next].store(false, std::memory_order_release);
    wl_surface_attach(s.surface, s.buffers[next], 0, 0);
    wl_surface_damage_buffer(s.surface, 0, 0, s.width, s.height);
    m_state->framesPending += 1;
    struct wl_callback *cb = wl_surface_frame(s.surface);
    wl_callback_add_listener(cb, &kFrameListener, m_state.get());
    wl_surface_commit(s.surface);
    s.shown = next;
    return true;
}

bool Overlay::waitFrames()
{
    for (int i = 0; i < kMaxDispatchRounds; ++i) {
        if (m_state->framesPending == 0) {
            return true;
        }
        if (!dispatchWait(*m_state, kEventTimeoutMs)) {
            return false;
        }
    }
    return true;
}

void Overlay::animate(const Spec &spec)
{
    if (!m_state || m_surfaces.empty()) {
        return;
    }
    // `Reveal` + `none`: nothing to animate. `Cover` + `none` still has to
    // paint the final frame once.
    if (spec.effect.type == EffectType::None
        && m_surfaces[0]->paint == Paint::Reveal) {
        return;
    }

    // `simple` is frame-driven (step per frame, 30 fps like awww);
    // everything else follows the clock along the bezier curve.
    const bool isSimple = spec.effect.type == EffectType::Simple;
    const int fps = spec.fps != 0 ? spec.fps : (isSimple ? 30 : 0);
    const double minDt = fps != 0 ? 1.0 / static_cast<double>(fps) : 0.0;
    const auto start = std::chrono::steady_clock::now();
    qWarning("perf: %lld animate: start dur=%.3f", perfMs(), spec.durationSec);
    std::uint32_t frameNo = 0;
    for (;;) {
        const auto frameStart = std::chrono::steady_clock::now();
        ++frameNo;
        double progress = 0.0;
        bool done = false;
        if (spec.effect.type == EffectType::None) {
            progress = 1.0;
            done = true;
        } else if (isSimple) {
            const int covered =
                std::min<int>(static_cast<int>(frameNo) * spec.effect.step, 255);
            progress = static_cast<double>(covered) / 255.0;
            done = covered >= 255;
        } else {
            const double t = std::min(
                1.0,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                    .count()
                / spec.durationSec);
            progress = bezierEase(spec.bezier, t);
            done = t >= 1.0;
        }

        for (auto &sp : m_surfaces) {
            if (!presentFrame(*sp, spec.effect, sp->paint, progress, done)) {
                qWarning("transition: dispatch failed; aborting");
                teardown();
                return;
            }
            if (done) {
                // End state: Reveal = fully transparent, Cover = the new image.
                sp->opaque = (sp->paint == Paint::Cover);
            }
        }
        // Pace on the compositor's frame callbacks (vsync).
        if (!waitFrames()) {
            qWarning("transition: dispatch failed; aborting");
            teardown();
            return;
        }
        if (done) {
            break;
        }
        if (minDt > 0.0) {
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart)
                    .count();
            if (elapsed < minDt) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(minDt - elapsed));
            }
        }
    }
    qWarning("perf: %lld animate: done frames=%u", perfMs(), frameNo);
}

void Overlay::fadeOutOpaque(double sec)
{
    if (!m_state || sec <= 0.0) {
        return;
    }
    std::vector<OverlaySurface *> targets;
    for (auto &sp : m_surfaces) {
        if (sp->opaque) {
            targets.push_back(sp.get());
        }
    }
    if (targets.empty()) {
        return;
    }
    // `Reveal`-style fade: scales the pristine image from opaque to transparent.
    Effect fade{};
    fade.type = EffectType::Fade;
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        const double t = std::min(
            1.0,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                / sec);
        const bool done = t >= 1.0;
        for (auto *s : targets) {
            if (!presentFrame(*s, fade, Paint::Reveal, t, done)) {
                return;
            }
        }
        if (!waitFrames() || done) {
            break;
        }
    }
    for (auto *s : targets) {
        s->opaque = false;
    }
}

void Overlay::finish(const Spec &spec, int holdMs)
{
    if (m_surfaces.empty()) {
        return;
    }
    qWarning("perf: %lld finish: start hold=%d", perfMs(), holdMs);
    if (holdMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    }
    animate(spec);
    qWarning("perf: %lld finish: animate done", perfMs());
    teardown();
}

void Overlay::close(int holdMs)
{
    if (m_surfaces.empty()) {
        return;
    }
    qWarning("perf: %lld close: start hold=%d", perfMs(), holdMs);
    if (holdMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    }
    // Never destroy an opaque surface: the compositor would animate its
    // closing on top of the (identical) wallpaper underneath.
    fadeOutOpaque(kCloseFadeSec);
    teardown();
    qWarning("perf: %lld close: done", perfMs());
}

void Overlay::teardown()
{
    for (auto &sp : m_surfaces) {
        OverlaySurface *s = sp.get();
        if (s->buffers[0]) {
            wl_buffer_destroy(s->buffers[0]);
            s->buffers[0] = nullptr;
        }
        if (s->buffers[1]) {
            wl_buffer_destroy(s->buffers[1]);
            s->buffers[1] = nullptr;
        }
        if (s->viewport) {
            wp_viewport_destroy(s->viewport);
            s->viewport = nullptr;
        }
        if (s->layer) {
            zwlr_layer_surface_v1_destroy(s->layer);
            s->layer = nullptr;
        }
        if (s->surface) {
            wl_surface_destroy(s->surface);
            s->surface = nullptr;
        }
        if (s->pool) {
            wl_shm_pool_destroy(s->pool);
            s->pool = nullptr;
        }
        if (s->mem) {
            ::munmap(s->mem, s->poolLen);
            s->mem = nullptr;
        }
        if (s->fd >= 0) {
            ::close(s->fd);
            s->fd = -1;
        }
    }
    m_surfaces.clear();
    if (m_state) {
        // Send the queued destroys to the compositor; without a flush the
        // layer surfaces stay mapped until the connection closes (the
        // reference implementation flushes at the end of teardown).
        wl_display_flush(m_state->display);
        // The layer-user data is freed with the state (after the surfaces,
        // whose listeners are gone by then).
        m_state.reset();
    }
}

} // namespace transitions
