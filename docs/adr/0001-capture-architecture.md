# ADR 0001: Build a clean capture render pass in Hyprland

- Status: Implemented as a Stage 2 correctness PoC
- Date: 2026-07-24
- Hyprland baseline:
  `36b2e0cfe0c6094dbc47bd42a437431315bb3087`

## Context

The presenter overlay must remain visible on the physical output but be absent
from monitor and region screen sharing. The existing `no_screen_share` layer
rule paints black rectangles in the capture. It does not reconstruct the pixels
under an excluded surface.

Source tracing established that the monitor mirror framebuffer is saved only
after the physical scene, including layer-shell overlays and popups, has been
rendered. `CScreenshareFrame::renderMonitor()` starts from that flattened
mirror texture and paints privacy masks afterward.

The existing `no_screen_share` contract must remain unchanged. A future
transparent omission must be a separate, explicit mode.

## Decision

Use direction A: implement a correctness-first clean capture render pass in the
Hyprland fork during Stage 2.

The PoC should:

- introduce a new explicit `omit` mode without changing
  `no_screen_share`;
- render a capture-only scene that skips each omitted layer root together with
  its subsurfaces, popup tree, and fade-out state;
- leave the physical output pass untouched;
- support whole-output capture first;
- fall back to the existing black privacy mask if clean rendering fails;
- avoid a clean pass when no omitted surface affects the capture;
- add capture-specific diagnostics and tests before optimization.

## Stage 2 implementation

The PoC adds an explicit layer rule:

```ini
layerrule {
    name = presenter-overlay
    match:namespace = ^presenter$
    screen_share_mode = omit
}
```

`normal` is the default, `black` selects the privacy mask, and `omit` selects
the clean monitor pass. The existing `no_screen_share` option is unchanged and
takes safe black-mask priority if both options match.

When a visible omitted layer affects a whole-monitor capture, the screen-share
frame queues a second compositor scene traversal into the capture target. The
traversal skips the layer root, its subsurfaces, its popup pass, and captured
fade-out state. The physical output traversal never enters this capture-only
mode.

The PoC intentionally supports only normal-transform whole-monitor capture.
Region capture, transformed outputs, buffer-size mismatch, and session lock use
the existing black mask as a safe fallback. Region geometry and transforms
remain Stage 3 work. The capture pass preserves compositor notifications,
error overlays, and DPMS black opacity. Debug-only overlays and content drawn
by plugin render-stage hooks are not replayed in the PoC and require an
explicit policy before upstreaming.

## Considered directions

### A. Separate clean capture render pass

**Decision: selected.**

Advantages:

- reconstructs real underlay pixels from compositor scene state;
- preserves z-order, opacity, blur inputs, popups, and overlapping clients;
- can share renderer traversal and policy with the physical pass;
- can apply capture-only filtering without a hide/show race;
- provides a clear black fallback on internal failure.

Costs and risks:

- extra GPU work and framebuffer memory while omission is active;
- renderer refactoring may be needed to avoid duplicating a complex pass;
- damage, transforms, color management, and resource lifetime need dedicated
  validation;
- capture and physical scene snapshots must be consistent for the frame.

**Hypothesis:** the cost can remain near zero when no omitted surface
intersects the captured monitor or region, and can later be reduced with damage
tracking. This requires measurement.

### B. Clean framebuffer or underlay snapshots

**Decision: rejected as the baseline.**

Saving a framebuffer immediately before an excluded overlay is attractive
because it may avoid a complete second pass. It is not generally correct:

- an excluded surface may be followed by included overlay surfaces or popups;
- multiple excluded surfaces can be interleaved with included content;
- translucent surfaces, blur, fade-outs, and overlapping popup trees depend on
  ordering;
- region crop, transform, and damage histories complicate snapshot selection;
- stale snapshots can produce one-frame leaks, frozen rectangles, or trailing
  pixels;
- multiple snapshots increase framebuffer lifetime and memory pressure.

Snapshots may be reconsidered only as a measured optimization after direction A
produces a correct reference image.

### C. Version-pinned Hyprland plugin

**Decision: rejected for the correctness PoC.**

Public render events describe the physical pass and screen-share lifecycle.
They do not expose an alternate capture scene or a capture-only layer filter.
The exact 0.56.0 binary can be function-hooked at
`Screenshare::CScreenshareFrame::renderMonitor()`, but doing so would require a
plugin to replace or duplicate private renderer/session behavior.

Function hooks have no API-stability guarantee. This direction adds symbol,
object-layout, and private-contract coupling without removing the core
renderer work. A version-pinned plugin may be reconsidered at the packaging
stage only after a compositor PoC defines the correct behavior.

## Consequences

- The next implementation belongs in the compositor fork, not XDPH.
- The physical output and current `no_screen_share` semantics remain stable.
- A separate wiki PR will be required if Stage 2 introduces a user-visible
  config option.
- Correctness gates include monitor and region capture, popup subtrees,
  lifecycle transitions, transforms/scales, SHM/DMA-BUF, and black fallback.
- Performance is an explicit acceptance dimension: baseline and patched CPU,
  GPU, frame time, capture FPS, latency, and framebuffer memory must be
  measured.
- Static or frozen underlay and black rectangles are not successful omission.

## Evidence

The source trace, exact mirror-copy point, render order, protocol convergence,
live nested-session result, and known test gaps are recorded in
`docs/render-path.md` and `docs/test-matrix.md`.
