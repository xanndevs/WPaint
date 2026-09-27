#!/usr/bin/env bash
# Remove the user-level WPaint installation: binary, launcher entry, and the
# hicolor icons installed by ./install.sh. Sources and the build dir are kept.
#
#   ./uninstall.sh
set -euo pipefail

BIN_DIR="$HOME/.local/bin"
APPS_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor"

echo "== Removing binary =="
rm -f "$BIN_DIR/wpaint"
echo "   removed $BIN_DIR/wpaint"

echo "== Removing launcher entry =="
rm -f "$APPS_DIR/wpaint.desktop"
echo "   removed $APPS_DIR/wpaint.desktop"

echo "== Removing icons =="
find "$ICON_DIR" -type f -name wpaint.png -delete 2>/dev/null || true
find "$ICON_DIR" -type d -empty -delete 2>/dev/null || true
echo "   removed wpaint hicolor icons"

gtk-update-icon-cache "$ICON_DIR" -f 2>/dev/null || true
update-desktop-database "$APPS_DIR" 2>/dev/null || true

echo
echo "Done. WPaint has been removed from your account."