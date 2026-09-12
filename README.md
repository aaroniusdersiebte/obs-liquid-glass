# Liquid Glass for OBS

An OBS Studio video filter that renders an Apple "Liquid Glass" style panel:
a rounded-rect glass surface with backdrop blur, edge refraction,
chromatic aberration, a specular edge highlight and an optional animated
shimmer. Apply it to a scene (or a source) to get a floating glass panel
over your layout.

**Free and open source (GPLv2+)**, works entirely standalone — no other
software required. It optionally syncs its panel position live from
[Zappify](https://aaronius.com) (a separate, commercial streaming-overlay
app by the same author) when that's installed and running, but that's purely
optional icing: with Zappify absent, disabled, or unlicensed, the filter
simply uses the manual panel position below instead of doing nothing.

## Adding the filter

1. Right-click a scene or source → **Filters**.
2. Under **Effect Filters**, click **+** → **Liquid Glass**.
3. Position/size the panel and tune the parameters below.

## Parameters

| Property | Description |
|---|---|
| Follow Zappify | When on, tries to fetch panel position(s) live from a locally running Zappify instance. Falls back to the manual panel below whenever Zappify has nothing to report. |
| Zappify Port / Poll Interval | Connection details for the optional Zappify sync — irrelevant if you don't use Zappify. |
| Fine-Tune Offset X / Y | Small resolution-independent nudge applied on top of Zappify's panel position, for alignment edge cases. |
| Manual Panel X / Y / Width / Height | Top-left position and size of the glass panel, as a **fraction (0–1) of the canvas** — this is what positions the panel when Zappify isn't providing one, i.e. in fully standalone use. Set Width or Height to 0 to hide the panel entirely. |
| Corner Radius | Rounding of the panel corners. |
| Backdrop Blur | Box-blur radius applied to the content behind the glass. |
| Refraction Strength | How far the backdrop is displaced near the panel edges (lensing). |
| Chromatic Aberration | Red/blue channel split strength near the edges. |
| Shimmer Speed / Amplitude | Animated ripple over time; set amplitude to 0 for a static panel. |
| Edge Highlight Width / Intensity | Soft specular highlight traced along the panel border. |
| Border Width / Color | Solid stroke drawn along the panel edge. |
| Tint Color | Optional color tint mixed into the glass (alpha controls strength). |

All of this lives in a single shader: `data/liquid_glass.effect`. The C side
(`src/liquid-glass-filter.c`) only wires up OBS properties and uploads the
uniforms every frame.

## Author

Liquid Glass is made by **Aaronius** — [aaronius.com](https://aaronius.com).

## Building

This project is based on the official [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

* Windows and macOS builds are produced automatically by the included
  GitHub Actions workflows (`.github/workflows/push.yaml`,
  `dispatch.yaml`) — push to a GitHub repo (or use "Run workflow") and
  download the built plugin from the Actions run artifacts, or from the
  draft GitHub Release created when you push a semver tag (e.g. `0.1.0`).
* For local Windows builds, install Visual Studio 2022 + CMake 3.30+ and
  run the CMake presets from `CMakePresets.json` (`windows-x64`).

See the [obs-plugintemplate wiki](https://github.com/obsproject/obs-plugintemplate/wiki)
for full build-system documentation (this repo inherits its CI/build setup
unchanged).

## Installing a built plugin on Windows 11

Copy the built `liquid-glass.dll` (and the `data/` folder contents) into:

```
%ProgramFiles%\obs-studio\obs-plugins\64bit\
%ProgramFiles%\obs-studio\data\obs-plugins\obs-liquid-glass\
```

or, for a per-user install, into the equivalent path under
`%APPDATA%\obs-studio\plugins\obs-liquid-glass\` (OBS ≥ 28 portable/user
plugin layout: `bin/64bit/` + `data/`).
