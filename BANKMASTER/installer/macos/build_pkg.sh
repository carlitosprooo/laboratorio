#!/usr/bin/env bash
# BANKMASTER by KALOX — macOS installer (.pkg) with selectable VST3 / AU / Standalone.
# Usage: installer/macos/build_pkg.sh [path/to/BankMaster_artefacts/Release]
#
# Signing / notarisation (optional, needs an Apple Developer ID):
#   export BM_SIGN_APP="Developer ID Application: NAME (TEAMID)"
#   export BM_SIGN_INSTALLER="Developer ID Installer: NAME (TEAMID)"
#   export BM_NOTARY_PROFILE="notarytool-keychain-profile"
# Without them an unsigned package is produced (Gatekeeper will ask for confirmation).
#
# The package only writes BANKMASTER.vst3 / BANKMASTER.component / BANKMASTER.app.
# It never touches other plug-ins (e.g. KaloxStart, KaloxSooth).
set -euo pipefail
cd "$(dirname "$0")"
HERE="$(pwd)"
ROOT="$(cd ../.. && pwd)"
ART="${1:-$ROOT/build-mac/BankMaster_artefacts/Release}"
VERSION="$(sed -n 's/^project (BANKMASTER VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
OUT="$ROOT/dist"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for b in "VST3/BANKMASTER.vst3" "AU/BANKMASTER.component" "Standalone/BANKMASTER.app"; do
  [[ -d "$ART/$b" ]] || { echo "missing $ART/$b - build first (scripts/build_macos.sh)"; exit 1; }
done

mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/app" "$OUT"
cp -R "$ART/VST3/BANKMASTER.vst3" "$WORK/vst3/"
cp -R "$ART/AU/BANKMASTER.component" "$WORK/au/"
cp -R "$ART/Standalone/BANKMASTER.app" "$WORK/app/"

if [[ -n "${BM_SIGN_APP:-}" ]]; then
  for b in "$WORK/vst3/BANKMASTER.vst3" "$WORK/au/BANKMASTER.component" "$WORK/app/BANKMASTER.app"; do
    codesign --force --deep --options runtime --timestamp --sign "$BM_SIGN_APP" "$b"
  done
else
  # ad-hoc signature (required on Apple Silicon for the binaries to load)
  for b in "$WORK/vst3/BANKMASTER.vst3" "$WORK/au/BANKMASTER.component" "$WORK/app/BANKMASTER.app"; do
    codesign --force --deep --sign - "$b"
  done
fi

# Component plists: bundles must NOT be relocatable. By default pkgbuild lets the
# macOS Installer "upgrade" an existing copy of a bundle with the same identifier
# wherever it is on disk (e.g. a build folder) instead of installing it in the
# standard location.
nonrelocatable() {
  pkgbuild --analyze --root "$1" "$2" >/dev/null
  local n i
  n=$(/usr/libexec/PlistBuddy -c "Print" "$2" | grep -c "RootRelativeBundlePath" || true)
  for ((i = 0; i < n; i++)); do
    # the key is only present for some bundle types: delete if present, then add explicitly
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$2" >/dev/null 2>&1 || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$2"
  done
  echo "component plist $(basename "$2"): $n bundle(s), BundleIsRelocatable=false"
}
nonrelocatable "$WORK/vst3" "$WORK/vst3.plist"
nonrelocatable "$WORK/au" "$WORK/au.plist"
nonrelocatable "$WORK/app" "$WORK/app.plist"

pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier "com.kalox.bankmaster.vst3" --version "$VERSION" "$WORK/BANKMASTER-VST3.pkg"
pkgbuild --root "$WORK/au" --component-plist "$WORK/au.plist" --install-location "/Library/Audio/Plug-Ins/Components" \
  --scripts "$HERE/scripts" \
  --identifier "com.kalox.bankmaster.au" --version "$VERSION" "$WORK/BANKMASTER-AU.pkg"
pkgbuild --root "$WORK/app" --component-plist "$WORK/app.plist" --install-location "/Applications" \
  --identifier "com.kalox.bankmaster.app" --version "$VERSION" "$WORK/BANKMASTER-App.pkg"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/resources"
cp "$HERE/resources/"* "$WORK/resources/"

PKG="$OUT/BANKMASTER-by-KALOX-$VERSION-macOS-universal.pkg"
SIGN_ARGS=()
[[ -n "${BM_SIGN_INSTALLER:-}" ]] && SIGN_ARGS=(--sign "$BM_SIGN_INSTALLER")
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
  --package-path "$WORK" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG"

if [[ -n "${BM_NOTARY_PROFILE:-}" && -n "${BM_SIGN_INSTALLER:-}" ]]; then
  xcrun notarytool submit "$PKG" --keychain-profile "$BM_NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
fi

pkgutil --check-signature "$PKG" || true
echo "installer: $PKG"
