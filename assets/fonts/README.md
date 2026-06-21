# Fonts

Vendored static TrueType builds, fetched from the [Fontsource](https://fontsource.org)
CDN. Both are licensed under the SIL Open Font License 1.1 (OFL), which permits
bundling and redistribution.

| File | Family | Use | Source |
|------|--------|-----|--------|
| `Fraunces-600.ttf` | Fraunces (semibold) | "Elua" wordmark + headings | google/fonts (OFL) |
| `Fraunces-400.ttf` | Fraunces (regular) | serif accents | google/fonts (OFL) |
| `PublicSans-400.ttf` | Public Sans (regular) | body / UI text | google/fonts (OFL) |
| `PublicSans-600.ttf` | Public Sans (semibold) | buttons / labels | google/fonts (OFL) |

These mirror the typography of the companion website (`../website`), which loads
the same two families. Loaded at runtime by `src/UiTheme.cpp` via the relative
path `assets/fonts/...` (the game runs with its working directory at `new/`).
