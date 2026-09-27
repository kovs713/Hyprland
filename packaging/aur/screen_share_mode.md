# `screen_share_mode` in `layerrule`

This build of Hyprland adds a third value to the `screen_share_mode` option of
`layerrule`, next to the stock `normal` (default) and `black` (privacy mask):

| value    | effect on a monitor/screen capture                                  |
| -------- | -------------------------------------------------------------------- |
| `normal` | capture the layer as-is (upstream default)                           |
| `black`  | mask the layer's area with black rectangles (upstream privacy mask)  |
| `omit`   | skip the layer surface entirely — the pixels behind it are captured  |

`omit` is what you want for a talk overlay: it stays visible locally, but a
screen share shows the slide, not your speaker notes.

## Usage

```hyprlang
layerrule {
    match:namespace = ^olay-overlay$
    screen_share_mode = omit
}
```

Works for any layer-shell client, not just overlays: notifications, docks and
launchers can be omitted the same way. `black` stays available for the case
where you want the *shape* hinted at without the content.

## Nix flake

The fork is a normal git remote, so a flake input is enough — no packaging
required:

```nix
inputs.hyprland.url = "github:kovs713/Hyprland?ref=feat/omit-capture";
```

## Blur

`blur` on the same `layerrule` needs `decoration.blur.enabled = true` in your
compositor config, which blurs *every* non-opaque window. It is off by default
in the packaged config for that reason.

## Upstream status

`screen_share_mode = omit` is a fork feature, not upstream. Upstream declined
taking on a compositor feature with a single implementation, so this stays
fork-only until that changes.
