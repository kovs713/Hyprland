# Hyprland capture render path

## Scope and provenance

This document records Stage 1 research only. It does not propose a production
implementation.

- Hyprland: `0.56.0`, commit
  [`36b2e0cfe0c6094dbc47bd42a437431315bb3087`](https://github.com/hyprwm/Hyprland/tree/36b2e0cfe0c6094dbc47bd42a437431315bb3087).
- Build type: `Debug`, with the source tree unchanged.
- XDPH: `1.4.0`, commit
  [`f36f5ff9e94dc5698d6a66e5cebd8d6b2e599068`](https://github.com/hyprwm/xdg-desktop-portal-hyprland/tree/f36f5ff9e94dc5698d6a66e5cebd8d6b2e599068).
- PipeWire: `1.6.8`.
- Test output: nested Wayland backend, `1920x1080@60`, scale `1`,
  transform `normal`.

Statements marked **confirmed** were established from the pinned source or a
live nested-session trace. Statements marked **hypothesis** need a later PoC or
measurement.

## Build and nested-session result

The unmodified checkout configured and built successfully:

```sh
cmake -S . -B build-presenter-stage1 -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/tmp/hyprland-presenter-stage1-install
cmake --build build-presenter-stage1 --parallel 4
./build-presenter-stage1/Hyprland --version
```

The resulting binary reports the requested clean commit and debug flag. A
minimal nested session was launched through the outer Wayland socket with
systemd/DBus environment mutation and the crash reporter disabled:

```sh
XDG_RUNTIME_DIR=/run/user/1000 \
WAYLAND_DISPLAY=wayland-1 \
HYPRLAND_NO_SD_VARS=1 \
HYPRLAND_NO_SD_NOTIFY=1 \
HYPRLAND_NO_CRASHREPORTER=1 \
./build-presenter-stage1/Hyprland \
    -c /tmp/hypr-presenter-stage1.conf
```

The temporary configuration disabled animations and blur and applied the
following rule to a `gtk4-layer-demo` overlay:

```ini
layerrule {
    name = stage1-no-screen-share
    match:namespace = ^demo$
    no_screen_share = on
}
```

The real namespace was checked with `hyprctl layers -j`; it is `demo`.

### Live reproduction

**Confirmed:** the overlay was rendered normally when the rule was disabled in
the temporary configuration. With `no_screen_share = on`, both a whole-output
capture and a region fully inside the overlay became a black rectangle. The
underlying scene was not revealed.

Two capture clients were used:

- `grim 1.5.0` selected `ext-image-copy-capture-v1`, allocated a
  `wl_shm` buffer, and captured the output. Its `-g` option cropped the
  whole-output result on the client side.
- `wf-recorder` selected `zwlr_screencopy_manager_v1`, and a client-side
  Wayland trace showed
  `capture_output_region(..., 700, 330, 400, 300)`,
  `wl_shm_pool.create_buffer(...)`, and `copy_with_damage(...)`.

The compositor log independently reported screen-share sessions for both
`monitor` (through `ext-image-copy`) and `region` (through wlr screencopy).
The legacy wlr monitor request was source-traced but not separately exercised
by the available clients. The nested compositor was then stopped with its own
`hyprctl dispatch exit`.

The following test-harness diagnostics were not hidden:

- changing the temporary config repeatedly produced
  `CConfigWatcher: got an event ... which we don't have?!`;
- shutdown after disconnecting the nested clients produced an
  `xdg_surface` role-order warning and client broken-pipe messages;
- continuous region recording logged repeated construction of unnamed
  framebuffers. This is not proof of a leak, but framebuffer lifetime and
  allocation rate must be profiled in the PoC.

No compositor crash or capture protocol failure occurred during the capture.
The XDG Portal/PipeWire path was traced through pinned source, but was not run
end to end inside the isolated nested DBus session.

## End-to-end capture path

### Portal, XDPH, and PipeWire

The portal side does not construct a compositor scene:

1. An application calls the XDG Desktop Portal ScreenCast interface.
2. XDPH binds `zwlr_screencopy_manager_v1` in
   `src/core/PortalManager.cpp`.
3. `SelectSources` resolves an output or geometry in
   `src/portals/Screencopy.cpp`.
4. XDPH calls:
   - `sendCaptureOutput(...)` for an output;
   - `sendCaptureOutputRegion(...)` for geometry.
5. XDPH negotiates a PipeWire stream and Wayland buffer.
6. On `buffer_done`, XDPH calls `sendCopyWithDamage(wlBuffer)`.
7. On `ready`, XDPH queues the compositor-filled buffer to PipeWire and
   requests the next frame.

**Confirmed:** XDPH transports the buffer produced by Hyprland. It has no
background, window, layer, popup, or damage state from which it could rebuild a
clean scene. Patching XDPH would therefore put scene policy at the wrong
boundary.

### Hyprland protocol to session

The legacy wlr protocol enters at `src/protocols/Screencopy.cpp`:

- `capture_output` creates a managed monitor session.
- `capture_output_region` creates a managed region session with a capture box.
- `CScreencopyFrame::shareFrame()` attaches the client buffer to the next
  `Screenshare::CScreenshareFrame`.

`src/managers/screenshare/ScreenshareSession.cpp` converts a logical region to
output pixels using monitor scale, rounds it, and swaps its buffer dimensions
for odd transforms. A pending frame blocks direct scanout and schedules monitor
damage.

The newer output `ext-image-copy-capture-v1` path in
`src/protocols/ImageCopyCapture.cpp` also creates a
`Screenshare::CScreenshareSession`, calls `nextFrame()`, and then calls the same
frame `share()` method. Whole-output capture therefore converges on the same
renderer path regardless of which of these two capture protocols the client
chooses.

### Output render, mirror, and capture

The full flow is:

```text
portal client
  -> XDG ScreenCast portal
  -> XDPH source selection
  -> wlr capture_output[_region]
  -> CScreencopyFrame
  -> CScreenshareSession::nextFrame()
  -> pending frame blocks direct scanout and damages output
  -> Renderer::renderMonitor()
  -> GLRenderer::endRender() executes the queued physical render pass
  -> OpenGL::end()
  -> OpenGL::saveBufferForMirror()
  -> output commit
  -> ScreenshareManager::onOutputCommit()
  -> CScreenshareFrame::copy()
  -> CScreenshareFrame::renderMonitor()
  -> client SHM or DMA-BUF
  -> XDPH PipeWire queue
```

The output commit listener is installed in `src/output/Monitor.cpp` and calls
`ScreenshareManager::onOutputCommit()`. That manager copies pending frames only
after an actual output commit.

## Exact point at which the overlay is in the mirror

This is the critical finding.

1. `Render::IHyprRenderer::renderMonitor()` in `src/render/Renderer.cpp`
   queues the normal scene, lock screen, compositor notifications/error/debug
   overlays, and software cursor.
2. `CHyprOpenGLImpl::endRender()` in `src/render/GLRenderer.cpp` executes the
   queued render pass.
3. It then calls `CHyprOpenGLImpl::end()` in `src/render/OpenGL.cpp`.
4. `end()` calls `saveBufferForMirror(monbox)` when
   `CMonitor::needsACopyFB()` is true.
5. `saveBufferForMirror()` copies the completed current framebuffer (or the
   current mirror source texture) into the monitor mirror framebuffer.

**Confirmed:** `saveBufferForMirror()` runs after all regular layer-shell
surfaces and their popups have been rendered. The mirror texture used by
screen sharing already contains the overlay.

There is one precise qualification: the mirror copy occurs before the final
monitor screen shader/output color-management pass. It is nevertheless the
completed compositor scene for capture purposes, and the excluded layer is
already flattened into it.

Later, `Screenshare::CScreenshareFrame::renderMonitor()` obtains
`getMirrorTexture()`, applies inverse output transform and region crop, and
copies it to the capture target. It then walks matching window and layer
surfaces and paints black rectangles for `no_screen_share`. Popups are covered
as part of the same policy. The optional software cursor is added afterward.

Therefore, deleting only those black-rectangle draws cannot reveal the
underlay. It reveals the already-flattened overlay pixels from the mirror.
This explains the live reproduction exactly.

## Physical compositor render order

The order below is established by
`IHyprRenderer::renderAllClientsForWorkspace()` and `renderMonitor()`:

1. Background clear/wallpaper.
2. Background layer-shell surfaces and fade-out surfaces.
3. `RENDER_POST_WALLPAPER`.
4. Bottom layer-shell surfaces and fade-out surfaces.
5. Pre-blur work.
6. Normal workspace windows:
   - tiled/normal windows;
   - fullscreen handling;
   - floating windows.
7. Special workspace dim/blur and special-workspace clients.
8. Pinned floating windows.
9. `RENDER_POST_WINDOWS`.
10. Top layer-shell surfaces and fade-out surfaces.
11. Input-method popups.
12. Overlay layer-shell surfaces and fade-out surfaces.
13. Popup trees for all layer levels, plus popup fade-outs.
14. Drag icon.
15. Lock-screen surfaces when locked.
16. Compositor notification, error, and debug overlays.
17. Software cursor when required.
18. DPMS black frame when required.
19. `RENDER_LAST_MOMENT`.
20. Execute render pass, save mirror framebuffer, then commit output.

For a layer surface, `renderLayer()` first renders its main surface and
subsurface tree. Popup trees are rendered in the later popup phase. Any
capture-only omission must account for the layer root, subsurfaces, popup
trees, and fade-out state without changing this physical order.

## Event hooks and Plugin API

### Public event hooks

`src/event/EventBus.hpp` exposes:

- layer open/close and rule-update events;
- physical render pre-check/pre/stage events;
- screen-share start/stop state;
- monitor pre-commit and related output events.

These hooks do not expose a capture render pass. Render-stage events are emitted
while building the physical output pass; `CScreenshareFrame::renderMonitor()`
does not emit an equivalent stage event. The screen-share state event carries
lifecycle information, not an alternate scene or a filtered layer tree.

**Confirmed:** the public event path cannot request a second, clean compositor
render or remove one layer subtree only from a capture frame.

### Function hooks

The Plugin API can look up and hook internal functions, but
`include/hyprland/src/plugins/PluginAPI.hpp` explicitly gives function hooks no
API-stability guarantee. The exact 0.56.0 binary exports
`Screenshare::CScreenshareFrame::renderMonitor()`, so a version-pinned plugin
could technically intercept it.

That is not a sufficient architecture:

- the current capture method copies an already-flattened mirror;
- a hook would need to replace or substantially duplicate the private capture
  renderer and its internal session/resource behavior;
- the implementation would depend on C++ symbols, object layout, and private
  renderer contracts;
- `renderLayer()` hooks alone do not help because the capture path never calls
  the scene renderer.

**Confirmed:** a plugin cannot obtain a correct clean frame through the public
Plugin API. A function-hook plugin is possible only as a fragile,
version-pinned replacement of private internals.

## Architecture consequences

Three directions were evaluated in detail in
`docs/adr/0001-capture-architecture.md`.

The Stage 2 recommendation is a correctness-first clean capture render pass in
the Hyprland fork:

- introduce a new explicit `omit` capture mode later;
- preserve the current black behavior of `no_screen_share`;
- skip the excluded layer root, subsurfaces, popups, and fade-outs only in the
  capture scene;
- never change the physical output render;
- use the existing black behavior as failure fallback;
- avoid the extra pass when no omitted surface intersects the capture;
- measure CPU, GPU, frame time, framebuffer lifetime, and damage behavior
  before optimizing.

This is a recommendation, not an implementation. Stage 2 has not started.

## Remaining validation

The following items intentionally remain open for later stages:

- live portal/XDPH/PipeWire capture with Firefox, Chromium, and OBS;
- DMA-BUF capture;
- fractional scale, transforms, negative output coordinates, and multi-output;
- cursor metadata and embedded-cursor modes;
- popup and fade-out lifecycle;
- damage correctness and one-frame leak checks;
- performance and framebuffer-allocation measurements.
