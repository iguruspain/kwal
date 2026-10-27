// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Layer-shell overlay surfaces used by the wallpaper transitions (port of
// the Wayland half of the Rust reference, transitions.rs).
//
// The overlay holds one layer-shell surface per target screen. Two
// directions, chosen by the caller:
//  * `Paint::Reveal` — the overlay holds the *outgoing* wallpaper (opaque).
//    The caller switches the wallpaper underneath, then `finish()` makes the
//    overlay transparent and destroys the surfaces.
//  * `Paint::Cover` — the overlay is transparent at first; `animate()` paints
//    the *incoming* wallpaper over the old one (which keeps playing); once it
//    is opaque the caller switches the wallpaper underneath and `close()`s.
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

// User data of each layer surface's listener (parallel to the layers).
struct LayerUser {
    OverlayState *state = nullptr;
    int index = 0;
};

// Per-connection compositor state.
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
        // True once the output's initial state stream is closed.
        bool done = false;
    };
    // (registry name, output, announced name)
    std::vector<OutputAnnouncement> outputs;
    // Layer surfaces (index = plane idx).
    std::vector<struct zwlr_layer_surface_v1 *> layers;
    // User data of each layer surface's listener (parallel to layers).
    std::vector<std::unique_ptr<LayerUser>> layerUsers;
    // Logical size announced by each layer surface's configure.
    std::vector<std::pair<int, int>> logical;
    int configsPending = 0;
    int framesPending = 0;
};

// One layer-shell surface holding one screen's wallpaper image.
struct OverlaySurface {
    struct wl_surface *surface = nullptr;
    struct zwlr_layer_surface_v1 *layer = nullptr;
    struct wp_viewport *viewport = nullptr;
    struct wl_shm_pool *pool = nullptr;
    // Keeps the memfd open for the compositor.
    int fd = -1;
    // RW mapping of the whole pool (two buffers back to back).
    uint8_t *mem = nullptr;
    size_t poolLen = 0;
    // Size of one buffer in bytes.
    size_t bufLen = 0;
    // Buffer (= physical pixel) size.
    int width = 0;
    int height = 0;
    // Logical size of the surface (screen-local coordinate space).
    std::pair<int, int> logical{0, 0};
    struct wl_buffer *buffers[2] = {nullptr, nullptr};
    std::atomic<bool> released[2];
    // Pristine, opaque copy of the image (premultiplied ARGB8888).
    Buffer orig;
    // Index of the buffer currently attached.
    int shown = 0;
    Paint paint = Paint::Reveal;
    // True while the surface shows non-transparent content (the old image
    // in `Reveal` until its effect ends, the new image in `Cover` once its
    // effect ends). `close()` fades such surfaces out before destroying them.
    bool opaque = false;
};

// Layer-shell layer (the `layer` setting: background, bottom, top or
// overlay).
enum class SurfaceLayer : int {
    Background,
    Bottom,
    Top,
    Overlay,
};

// Parses the `layer` setting value; absent or unknown values are `Bottom`.
SurfaceLayer parseSurfaceLayer(const QString &value);

// A set of layer-shell surfaces holding the wallpaper image for the target
// screens.
class Overlay {
public:
    Overlay() = default;
    ~Overlay();
    Overlay(const Overlay &) = delete;
    Overlay &operator=(const Overlay &) = delete;
    Overlay(Overlay &&) = delete;
    Overlay &operator=(Overlay &&) = delete;

    // Connects to the Wayland session, decodes every plane's image, maps the
    // surfaces and blocks until the first frame of each is composited (the
    // overlay is then on screen).
    //
    // Returns false when there is no Wayland session or no plane can be
    // covered (planes without a matching output are skipped with a warning);
    // hard failures (no layer-shell support, image decode error) are logged
    // and also return false.
    bool begin(const std::vector<Output> &mons,
               const std::vector<Plane> &planes,
               SurfaceLayer layer,
               Paint paint);

    // Runs the effect to its end state (for `Reveal` a fully transparent
    // overlay, for `Cover` the fully opaque incoming image) without
    // destroying the surfaces.
    void animate(const Spec &spec);

    // `Reveal` direction: holds the old image for `holdMs` (time for the new
    // wallpaper to load underneath), runs the effect, destroys the surfaces.
    void finish(const Spec &spec, int holdMs);

    // Destroys the surfaces and closes the connection (`Cover` direction:
    // call after the wallpaper switch and a short wait for the new wallpaper
    // to load). After `holdMs`, any surface still showing the image is faded
    // out for ~150 ms first: destroying an opaque surface makes the compositor
    // play its window-close animation, which shows up as a "ghost" of the
    // image on top of the new wallpaper.
    void close(int holdMs = 0);

    bool active() const { return !m_surfaces.empty(); }

private:
    void teardown();
    // Draws one frame of `effect` on `s` (waits for a free buffer) and commits it.
    bool presentFrame(OverlaySurface &s, const Effect &effect, Paint paint,
                      double progress, bool done);
    // Waits for the compositor's frame callbacks (vsync pacing).
    bool waitFrames();
    // Fades every `opaque` surface to transparent over `sec` seconds.
    void fadeOutOpaque(double sec);

    std::unique_ptr<OverlayState> m_state;
    std::vector<std::unique_ptr<OverlaySurface>> m_surfaces;
};

} // namespace transitions
