// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Layer-shell overlay surfaces used by the wallpaper transitions: one surface
// per target screen. Reveal: surfaces hold the opaque outgoing image; the caller
// switches the wallpaper, then finish() clears them. Cover: surfaces start
// transparent; animate() paints the incoming image over the old one, and once
// opaque the caller switches and close()s.
#pragma once

#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "effects.h"
#include "outputs.h"
#include "planes.h"
#include "transitionspec.h"

struct wl_buffer;
struct wl_compositor;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_shm;
struct wl_shm_pool;
struct wl_surface;
struct wp_viewport;
struct wp_viewporter;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace transitions {

struct OverlayState;

// Layer-surface listener user data (index = layers index).
struct LayerUser {
    OverlayState *state = nullptr;
    int index = 0;
};

struct OverlayState {
    struct wl_display *display = nullptr;
    struct wl_registry *registry = nullptr;
    struct wl_compositor *compositor = nullptr;
    struct wl_shm *shm = nullptr;
    struct zwlr_layer_shell_v1 *shell = nullptr;
    struct wp_viewporter *viewporter = nullptr;
    struct OutputAnnouncement {
        quint32 name = 0;
        struct wl_output *output = nullptr;
        QString announcedName;
        bool done = false;
    };
    std::vector<OutputAnnouncement> outputs;
    // Index = plane index.
    std::vector<struct zwlr_layer_surface_v1 *> layers;
    std::vector<std::unique_ptr<LayerUser>> layerUsers;
    // Logical size from each layer surface's configure.
    std::vector<std::pair<int, int>> logical;
    int configsPending = 0;
    int framesPending = 0;
};

struct OverlaySurface {
    struct wl_surface *surface = nullptr;
    struct zwlr_layer_surface_v1 *layer = nullptr;
    struct wp_viewport *viewport = nullptr;
    struct wl_shm_pool *pool = nullptr;
    // memfd kept open for the compositor.
    int fd = -1;
    // RW mapping of the pool: two buffers back to back (double buffering).
    uint8_t *mem = nullptr;
    size_t poolLen = 0;
    size_t bufLen = 0;
    // Buffer (= physical pixel) size; `logical` is the screen-local size.
    int width = 0;
    int height = 0;
    std::pair<int, int> logical{0, 0};
    struct wl_buffer *buffers[2] = {nullptr, nullptr};
    std::atomic<bool> released[2];
    // Pristine, opaque copy of the image (premultiplied ARGB8888).
    Buffer orig;
    int shown = 0;
    Paint paint = Paint::Reveal;
    // Surface currently shows opaque content; close() fades these out first.
    bool opaque = false;
};

// Layer-shell layer for the `layer` setting.
enum class SurfaceLayer : int {
    Background,
    Bottom,
    Top,
    Overlay,
};

// Parses the `layer` setting; absent or unknown values are `Bottom`.
SurfaceLayer parseSurfaceLayer(const QString &value);

class Overlay {
public:
    Overlay() = default;
    ~Overlay();
    Overlay(const Overlay &) = delete;
    Overlay &operator=(const Overlay &) = delete;
    Overlay(Overlay &&) = delete;
    Overlay &operator=(Overlay &&) = delete;

    // Connects, decodes planes, maps surfaces, and blocks until each surface's
    // first frame is composited (overlay visibly up). False → plain switch: no
    // Wayland session, no layer-shell, decode error, or no plane with a matching
    // output (unmatched planes are skipped with a warning).
    bool begin(const std::vector<Output> &mons,
               const std::vector<Plane> &planes,
               SurfaceLayer layer,
               Paint paint);

    // Runs the effect to its end state (Reveal: fully transparent, Cover: fully
    // opaque) without destroying the surfaces.
    void animate(const Spec &spec);

    // Reveal: hold holdMs (new wallpaper loading), run the effect, destroy surfaces.
    void finish(const Spec &spec, int holdMs);

    // Destroys the surfaces and closes the connection (Cover: call after the switch).
    // After holdMs, still-opaque surfaces fade out for ~150 ms first: destroying an
    // opaque surface makes the compositor play its window-close animation, a "ghost"
    // of the image over the new wallpaper.
    void close(int holdMs = 0);

    bool active() const { return !m_surfaces.empty(); }

private:
    void teardown();
    // Draws one effect frame and commits it; waits for a free buffer.
    bool presentFrame(OverlaySurface &s, const Effect &effect, Paint paint,
                      double progress, bool done);
    // Waits for the compositor's frame callbacks (vsync pacing).
    bool waitFrames();
    void fadeOutOpaque(double sec);

    std::unique_ptr<OverlayState> m_state;
    std::vector<std::unique_ptr<OverlaySurface>> m_surfaces;
};

} // namespace transitions
