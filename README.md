# WPaint

A layered image editor for Linux that re-imagines the Windows 11 Paint
interface: Fluent toolbar clusters with captions and dividers, a tool-size
strip to the left of the canvas, a layers rail, a (stubbed) Copilot rail, and
an HSV/RGB color editor — all in Qt 6 / C++.

## Features

- Layered editing (add / remove / reorder / visibility / rename), a
  transparent floating selection, full undo/redo
- Pencil, brush (Round/Square/Spray/Calligraphy), eraser, fill, eyedropper,
  text with a format bar, crop, and ten shape tools (line, curve, rectangle,
  rounded rectangle, ellipse, triangle, right triangle, diamond, pentagon,
  arrow) with outline/fill/fill+outline styles
- Color 1 / color 2 wells, 2×10 preset palette, HSV/RGB color dialog
- Canvas boundary handles, image resize & rotate (whole canvas or selection)
- Image and transform undo via per-paint patches
- Export to PNG / JPEG / BMP / GIF; save/load layered `.wpa` projects
- Light and dark themes (Ctrl+Shift+T), native High-DPI icon rendering

## Requirements

- Qt 6.5+ (Core, Gui, Widgets, Svg) — `qt6-base-dev` / `qt6-svg-dev`
  (package names vary by distro)
- CMake 3.21+ and a C++17 compiler
- A working display (X11 or Wayland)

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/wpaint
```

Optional debug flags used by the smoke-test script:

```sh
./build/wpaint --screenshot /tmp/shot.png   # render offscreen, save, quit
./build/wpaint --dark --screenshot dark.png # same, dark theme
```

## Layout guide

- `src/` — application code (see `docs/ARCHITECTURE.md`)
- `assets/icons/` — monochrome SVGs tinted at runtime; regenerate with
  `/tmp/gen_icons.py` if icons are tweaked
- `resources.qrc` — resource manifest (keep in sync with new icons)

## Shortcuts

| Action        | Shortcut |
|---------------|----------|
| New / Open / Save | Ctrl+N / Ctrl+O / Ctrl+S |
| Save As       | Ctrl+Shift+S |
| Undo / Redo   | Ctrl+Z / Ctrl+Y (+ Ctrl+Shift+Z) |
| Cut / Copy / Paste | Ctrl+X / Ctrl+C / Ctrl+V |
| Select All / Delete | Ctrl+A / Del |
| Resize & Rotate | Ctrl+E |
| Zoom In / Out / 100% / Fit | Ctrl++ / Ctrl+- / Ctrl+0 / Ctrl+9 |

## Notes

- Transparency flattens on export; the eyedropper samples over white.
- The Copilot rail is a visual placeholder for a future model integration.
