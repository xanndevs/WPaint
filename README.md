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
- Middle-click drag pans the canvas; Ctrl+wheel zooms at the cursor
- Pointing arrow shape: drag from the press point to the release point
- Preferences dialog (Ctrl+): behavior, what a new image starts as, and every
  shortcut — all rebindable
- Three colour palettes, or twenty swatches of your own

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

Every one of these can be changed in **Edit -> Preferences -> Shortcuts**; the table
below is the default.

| Action        | Shortcut |
|---------------|----------|
| New / Open / Save | Ctrl+N / Ctrl+O / Ctrl+S |
| Save As       | Ctrl+Shift+S |
| Undo / Redo   | Ctrl+Z / Ctrl+Y (+ Ctrl+Shift+Z) |
| Cut / Copy / Paste | Ctrl+X / Ctrl+C / Ctrl+V |
| Select All / Deselect / Delete | Ctrl+A / Ctrl+Shift+A / Del |
| Resize & Rotate | Ctrl+E |
| Zoom In / Out / 100% / Fit | Ctrl++ / Ctrl+- / Ctrl+0 / Ctrl+9 |
| Theme preference | Ctrl+Shift+T |
| Preferences | Ctrl+, |
| Select / Crop / Magnify | M / C / Z |
| Pencil / Bucket / Text / Eraser / Eyedropper | P / G / T / E / I |
| Brush / Shapes | B / U |
| Flip horizontal / vertical | Ctrl+F / Ctrl+Shift+F |
| Rotate left / right | Ctrl+R / Ctrl+Shift+R |
| Shape fill mode: next / previous | Ctrl+U / Ctrl+Shift+U |
| Next / previous shape | Ctrl+Tab / Ctrl+Shift+Tab |
| Brush size: larger / smaller | ] / [ |
| Swap color 1 and color 2 | X |
| Show or hide the layers panel | F7 |
| Focus the layer above / below | Alt+Up / Alt+Down |
| Add / duplicate a layer | Ctrl+Shift+N / Ctrl+Shift+D |
| Move the layer up / down | Ctrl+Shift+Up / Ctrl+Shift+Down |

Gestures that are not key combinations, and so cannot be rebound, are listed on
the same page: Space + wheel (brush size), Ctrl + wheel (zoom at the pointer),
middle-drag (pan), the arrow keys (nudge, ×10 with Shift), Space + arrows
(sketch), Shift + drag (square), Alt + drag (grow from the centre) and Esc
(cancel).

## Notes

- Transparency flattens on export; the eyedropper samples over white.
- Preferences are stored via `QSettings` (`~/.config/WPaint/WPaint.conf`); a
  display preference only affects the screen, never a saved file.
- The Copilot rail is a visual placeholder for a future model integration.
