# Capture test matrix

## Status legend

- **PASS**: executed and matched the expected result.
- **FAIL**: executed and did not match the expected result.
- **PLANNED**: required for a later implementation stage.
- **N/A**: not applicable to the unmodified Stage 1 baseline.

The baseline is Hyprland `0.56.0` at
`36b2e0cfe0c6094dbc47bd42a437431315bb3087`.

## Stage 2 execution record

The correctness-first PoC was built on branch
`presenter/omit-capture-poc`.

| Check | Status | Evidence/result |
| --- | --- | --- |
| Explicit capture mode | PASS | `screen_share_mode` accepts `normal`, `black`, and `omit`; unknown values are rejected by the rule parser |
| Debug build | PASS | Patched `Hyprland` and `hyprland_gtests` targets built successfully |
| Formatting | PASS | All changed C++ files pass the repository `clang-format` check |
| Unit tests | PASS | CTest: 271/271 passed, including two new `LayerRule` mode tests |
| Config load | PASS | A block-style `layerrule` with `screen_share_mode = omit` loaded in the nested compositor and matched namespace `demo` |
| Physical output unchanged | PASS | An outer-compositor capture showed the GTK layer over the animated gears |
| Clean monitor capture | PASS | `grim` whole-output capture showed the live gears with no GTK layer and no black replacement |
| Live underlay | PASS | Whole-output captures taken at different times had different pixels as the gears rotated |
| Overlay close/reopen during active sharing | PASS | One direct wlr `capture_output` session stayed alive for 2015 frames while the GTK layer was closed and reopened; the post-reopen control frame was clean |
| Root/subsurface/popup omission | PASS implementation / PLANNED dedicated client | The capture pass returns before queuing the omitted layer surface tree or its later popup traversal; a popup-producing layer client remains to be exercised |
| Layer and popup fade-out policy | PASS implementation / PLANNED frame inspection | Fade-out state snapshots the owner layer policy and is skipped by the clean pass; transition frames remain a Stage 4 inspection item |
| Existing `no_screen_share` | PASS | With both policies present, `no_screen_share` retained priority and produced the existing black rectangle |
| Region safety fallback | PASS | `wf-recorder` used `capture_output_region`; the omitted bounds were black and the capture log selected the Stage 2 fallback |
| Clean-render diagnostics | PASS | Grepable `[clean-capture]` diagnostics report activation, deactivation, and fallback reason without per-frame log spam |
| Graceful nested shutdown | PASS | Test clients stopped and nested Hyprland reached its normal end |

Known test-harness diagnostics are unchanged from Stage 1. Reusing the
coverage-enabled build directory additionally produced stale `.gcda` checksum
warnings after source changes; builds and tests still exited successfully.

## Stage 1 execution record

| Check | Status | Evidence/result |
| --- | --- | --- |
| Exact checkout | PASS | `HEAD` and built ABI string match the required commit; there were no tracked modifications before the work |
| Unmodified debug build | PASS | CMake/Ninja build completed, 1099 build edges |
| Config validation | PASS | `Hyprland --verify-config` returned `config ok` |
| Unit tests | PASS | CTest: 269/269 passed; final run completed in 39.13 seconds |
| Safe nested startup | PASS | Wayland backend; systemd variable import, notify, and crash reporter disabled |
| Graceful nested shutdown | PASS | Nested `hyprctl dispatch exit`; compositor reached normal end |
| `no_screen_share` visible reference | PASS | With the temporary rule off, the GTK layer demo was present in output capture |
| `no_screen_share` monitor masking | PASS | With the rule on, the layer bounds were black in whole-output capture |
| `no_screen_share` region masking | PASS | Region inside the layer bounds was black |
| `ext-image-copy` monitor path | PASS | `grim` trace: output source/session, SHM buffer, capture frame, ready |
| wlr region path | PASS | `wf-recorder` trace: `capture_output_region`, SHM buffer, `copy_with_damage`, ready |
| wlr monitor path | PASS source trace / PLANNED live | `capture_output` converges on the managed monitor session; available live clients used ext-image-copy for monitor |
| XDPH/PipeWire source trace | PASS | XDPH 1.4.0 source traced from source selection through Wayland copy to PipeWire queue |
| Live portal/PipeWire E2E | PLANNED | Not run inside the isolated nested DBus session |
| DMA-BUF capture | PLANNED | SHM was forced/observed for Stage 1 live tests |
| Hyprtester integration suite | PLANNED | Full target built; runtime suite was not run because its `kitty` test dependency is absent |

### Diagnostics observed

No capture failure or compositor crash occurred. The following warnings remain
recorded:

- temporary-config reloads emitted `CConfigWatcher` unknown-watch events;
- nested shutdown emitted an `xdg_surface` role-order warning and expected
  client disconnect errors;
- continuous region capture logged repeated unnamed framebuffer construction;
  allocation/lifetime impact is not yet measured.

## Capture-source matrix

| Consumer/path | Monitor | Region | Buffer | Stage 1 status | Later acceptance |
| --- | --- | --- | --- | --- | --- |
| `grim` / `ext-image-copy-capture-v1` | Whole output | Client-side crop | SHM | PASS | Omitted surface absent, included content unchanged |
| `wf-recorder` / wlr screencopy | Not exercised | `capture_output_region` | SHM | PASS region | Omitted surface absent for every frame |
| XDPH / PipeWire / Firefox | Source-traced | Source-traced | SHM and DMA-BUF negotiation | PLANNED live | Stable 30/60 FPS sharing |
| XDPH / PipeWire / Chromium | Source-traced | Source-traced | SHM and DMA-BUF negotiation | PLANNED live | Stable 30/60 FPS sharing |
| XDPH / PipeWire / OBS | Source-traced | Source-traced | SHM and DMA-BUF negotiation | PLANNED live | Stable capture and stop/start |
| Direct ext-image-copy client | Supported | No protocol region source | SHM/DMA-BUF | PASS via `grim` SHM | Same omission policy as wlr path |

## Correctness matrix for Stage 2 and Stage 3

Each row must be checked for monitor and region capture unless stated
otherwise. “Clean” means the real scene below the omitted surface, never black
and never a frozen snapshot.

| Scenario | Physical output | Capture | Status |
| --- | --- | --- | --- |
| No matching layer | Unchanged | Bitwise/visual baseline, negligible overhead | PASS basic monitor path; performance measurement PLANNED |
| One visible omitted layer | Layer visible | Clean underlay | PASS monitor |
| Multiple omitted layers | All visible | Clean underlay at every omitted subtree | PLANNED |
| Hidden/unmapped omitted layer | Unchanged | No extra work or damage | PASS active monitor close/reopen; detailed damage inspection PLANNED |
| Included overlay above omitted layer | Both visible | Included overlay retained over clean underlay | PLANNED |
| Included overlay below omitted layer | Both visible | Included overlay revealed correctly | PLANNED |
| Translucent omitted layer | Correct blending | No contribution from omitted layer | PLANNED |
| Omitted layer subsurface | Visible | Root and subsurface tree omitted | PASS implementation; dedicated live client PLANNED |
| Omitted layer popup | Visible | Popup tree omitted | PASS implementation; dedicated live client PLANNED |
| Included popup from another layer | Visible | Popup retained | PLANNED |
| Layer fade-out | Visible animation | No transient leak or stale pixels | PASS implementation; frame-by-frame inspection PLANNED |
| Blur behind omitted layer | Physical blur unchanged | Capture scene recomputed without omitted contribution | PLANNED |
| Existing `no_screen_share` | Visible | Black privacy mask, unchanged semantics | PASS monitor regression |
| Clean-render internal failure | Visible | Black fallback, logged error | PASS guarded paths in implementation; forced monitor failure PLANNED |

## Geometry matrix

| Dimension | Values | Expected result | Status |
| --- | --- | --- | --- |
| Output scale | `1`, `2` | Correct dimensions and crop | `1`: PASS baseline; `2`: PLANNED |
| Fractional scale | `1.25`, `1.5`, `1.75` | Rounded bounds without gaps/leaks | PLANNED |
| Transform | normal, 90, 180, 270 | Correct orientation and capture box | normal: PASS baseline; others: PLANNED |
| Flipped transform | all supported variants | Correct inverse transform | PLANNED |
| Region position | origin, center, edges, partially outside layer | Exact intersection | center: PASS baseline; others: PLANNED |
| Output position | positive, negative, mixed | Correct global-to-output coordinates | PLANNED |
| Multi-output | independent, adjacent, mirrored | Correct source and no cross-output leak | PLANNED |
| Layer margins/anchors | every edge and center | Correct omitted geometry | center: PASS baseline; others: PLANNED |

## Buffer, cursor, and color matrix

| Area | Cases | Expected result | Status |
| --- | --- | --- | --- |
| Client buffer | SHM | Correct pixels and damage | PASS baseline masking |
| Client buffer | DMA-BUF, supported modifiers | Same pixels as SHM | PLANNED |
| Cursor | excluded by client | No cursor | PASS for non-cursor baseline |
| Cursor | embedded software cursor | Cursor composited after clean scene | PLANNED |
| Cursor | PipeWire cursor metadata | Correct metadata and position | PLANNED |
| Output color | sRGB/default | Match current capture behavior | PASS baseline masking |
| Output color | ICC/HDR/CM presets | No regression from current mirror semantics | PLANNED |
| Screen shader | configured | Explicitly decide whether capture matches current pre-shader mirror | PLANNED |

## Damage and lifecycle matrix for Stage 4

Every transition must be inspected frame by frame. A pass requires no one-frame
overlay leak, stale background, trailing pixels, frozen rectangle, unwanted
black frame, or permanent full-output damage.

| Transition | Expected capture behavior | Status |
| --- | --- | --- |
| Map/unmap omitted layer | Clean before, during, and after transition | PLANNED |
| Resize/move | Old bounds repaired; new bounds clean | PLANNED |
| Collapse/expand | No stale pixels | PLANNED |
| Popup open/close | Popup subtree never leaks | PLANNED |
| Namespace rule reload | Atomic policy transition with safe fallback | PLANNED |
| Output hotplug/remove | Session recovers or closes cleanly | PLANNED |
| Workspace switch | Correct workspace scene | PLANNED |
| Fullscreen enter/leave | Correct z-order and underlay | PLANNED |
| Sharing start while overlay visible | First delivered frame is clean | PASS monitor PoC |
| Overlay opens during sharing | No intermediate leaked frame | PASS session continuity; frame-by-frame leak inspection PLANNED |
| Overlay closes during sharing | Underlay remains current, not frozen | PASS session continuity and live underlay; frame-by-frame inspection PLANNED |
| Sharing stop/start loop | No retained resources or stale snapshot | PLANNED |
| Client buffer resize/renegotiation | Correct dimensions and damage reset | PLANNED |

## Performance matrix for Stage 5

Measure unmodified baseline and patched build with the same scene. Record idle
CPU, compositor/GPU utilization, output frame time, capture FPS, dropped
frames, latency, and framebuffer memory.

| Resolution/FPS | Capture | Overlay state | Excluded count | Status |
| --- | --- | --- | --- | --- |
| 1920x1080 @ 30 | monitor, region | hidden, visible | 0, 1, many | PLANNED |
| 1920x1080 @ 60 | monitor, region | hidden, visible | 0, 1, many | PLANNED |
| 1920x1080 @ 120 | monitor, region | hidden, visible | 0, 1, many | PLANNED |

Required gates:

- practically zero overhead when no omitted surface affects the capture;
- no unbounded framebuffer creation or memory growth;
- no permanent full-monitor damage;
- no capture FPS regression outside an agreed and documented budget;
- black fallback remains cheaper and available on clean-pass failure.

## Stage 2 exit gate

The correctness-first monitor PoC is ready to advance only when:

1. the physical overlay remains visible;
2. monitor capture shows live clean underlay, not black or frozen pixels;
3. layer root, subsurfaces, and popup tree are omitted;
4. existing `no_screen_share` still produces its current black mask;
5. failure produces the black fallback and a diagnostic;
6. build and available tests pass;
7. the new config option has a linked separate wiki PR plan.

Items 1, 2, 4, and 6 pass for the monitor PoC. Item 3 is implemented in the
renderer traversal but still needs a dedicated popup/subsurface layer client.
The black fallback and diagnostics are implemented and region fallback was
exercised; a forced monitor-internal failure remains to be tested. A separate
wiki PR is required before proposing the config option upstream.
