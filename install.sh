#!/usr/bin/env bash
# Install WPaint for the current user: builds (incremental), then installs the
# binary, a launcher entry, and hicolor app icons. No root required.
#
#   ./install.sh
#
# Opposite: ./uninstall.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BIN_DIR="$HOME/.local/bin"
APPS_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor"
DESKTOP_FILE="$APPS_DIR/wpaint.desktop"

# 1. Build (incremental; no-op when build/wpaint is already current).
echo "== Building =="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j

# 2. Binary.
echo "== Installing binary =="
mkdir -p "$BIN_DIR"
install -m 0755 "$ROOT/build/wpaint" "$BIN_DIR/wpaint"
echo "   installed $BIN_DIR/wpaint"

# 3. Hicolor icons (rendered from assets/wpaint.svg).
echo "== Installing icons =="
rsvg_icon()  { rsvg-convert -w "$3" -h "$3" "$1" -o "$2"; }
magick_icon(){ convert -background none -density 192 -resize "${3}x${3}" "$1" "$2"; }

ICONBUILD=""
if command -v rsvg-convert >/dev/null 2>&1; then
    ICONBUILD=rsvg_icon
elif command -v convert >/dev/null 2>&1; then
    ICONBUILD=magick_icon
fi

if [ -n "$ICONBUILD" ]; then
    for s in 32 48 64 128 256; do
        mkdir -p "$ICON_DIR/${s}x${s}/apps"
        "$ICONBUILD" "$ROOT/assets/wpaint.svg" "$ICON_DIR/${s}x${s}/apps/wpaint.png" "$s"
    done
    echo "   installed hicolor icons"
    gtk-update-icon-cache "$ICON_DIR" -f 2>/dev/null || true
else
    echo "   !! neither rsvg-convert nor convert found; skipping icons"
fi

# 4. Desktop entry.
echo "== Installing launcher entry =="
mkdir -p "$APPS_DIR"
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=WPaint
GenericName=Image Editor
Comment=Windows 11 Paint-style layered image editor
Exec="$BIN_DIR/wpaint"
Icon=wpaint
Terminal=false
Categories=Graphics;2DGraphics;RasterGraphics;
StartupNotify=false
EOF
chmod +x "$DESKTOP_FILE"
update-desktop-database "$APPS_DIR" 2>/dev/null || true
echo "   installed $DESKTOP_FILE"

# 5. KDE shell icon cache: plasmashell/krunner resolve Icon=wpaint through
#    QIcon, which caches in memory and never re-stats the PNG, so a running
#    shell keeps showing the previously installed icon. Offer to restart them.
RESTART_SHELL=0
if [ -n "${XDG_CURRENT_DESKTOP:-}" ] && [[ "${XDG_CURRENT_DESKTOP}" == *KDE* ]] && pgrep -x plasmashell >/dev/null 2>&1; then
    if [ -t 0 ]; then
        echo
        printf "Restart the KDE shell and krunner to pick up the new icon? [y/N] "
        read -r REPLY || REPLY=""
        case "$REPLY" in
            [yY]|[yY][eE][sS]) RESTART_SHELL=1 ;;
            *) echo "   skipped; the launcher keeps the old icon until you log out and back in." ;;
        esac
    fi
fi

if [ "$RESTART_SHELL" = "1" ]; then
    echo "   restarting plasmashell and krunner"
    kquitapp6 plasmashell >/dev/null 2>&1 || pkill -x plasmashell
    kquitapp6 krunner >/dev/null 2>&1 || pkill -x krunner
fi

echo
echo "Done. WPaint should now appear in your app launcher."