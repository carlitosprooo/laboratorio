#!/usr/bin/env bash
# BANKMASTER BY KALOX — macOS DMG (contains the validated .pkg, Read Me and uninstaller)
# Usage: installer/macos/dmg/build_dmg.sh path/to/BANKMASTER-BY-KALOX-1.0.0-macOS-universal.pkg
# Requires: python3 + dmgbuild (pip install dmgbuild), macOS tools (sips, iconutil, tiffutil, hdiutil).
# Optional signing: export BM_SIGN_APP="Developer ID Application: NAME (TEAMID)"
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
ASSETS="$ROOT/installer/assets"
PKG="${1:?usage: build_dmg.sh path/to/pkg}"
VERSION="$(sed -n 's/^project (BANKMASTER VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
OUT="$ROOT/dist"
DMG="$OUT/BANKMASTER-BY-KALOX-$VERSION-macOS-Universal.dmg"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# Retina background (1x + 2x in one TIFF)
tiffutil -cathidpicheck "$ASSETS/dmg_background.png" "$ASSETS/dmg_background@2x.png" -out "$WORK/background.tiff"

# Volume icon (.icns) from the 1024 px master
ICONSET="$WORK/BANKMASTER.iconset"
mkdir -p "$ICONSET"
for sz in 16 32 128 256 512; do
  sips -z $sz $sz "$ASSETS/icon_1024.png" --out "$ICONSET/icon_${sz}x${sz}.png" >/dev/null
  sips -z $((sz * 2)) $((sz * 2)) "$ASSETS/icon_1024.png" --out "$ICONSET/icon_${sz}x${sz}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$WORK/BANKMASTER.icns"

cp "$HERE/uninstall_bankmaster.command" "$WORK/uninstall.command"
chmod 755 "$WORK/uninstall.command"

rm -f "$DMG"
dmgbuild -s "$HERE/dmg_settings.py" \
  -D pkg="$PKG" -D readme="$HERE/readme.txt" -D uninstall="$WORK/uninstall.command" \
  -D background="$WORK/background.tiff" -D icon="$WORK/BANKMASTER.icns" \
  "BANKMASTER BY KALOX" "$DMG"

if [[ -n "${BM_SIGN_APP:-}" ]]; then
  codesign --force --timestamp --sign "$BM_SIGN_APP" "$DMG"
fi

hdiutil verify "$DMG"
(cd "$OUT" && shasum -a 256 "$(basename "$DMG")" | tee "$(basename "$DMG").sha256")
echo "dmg: $DMG"
