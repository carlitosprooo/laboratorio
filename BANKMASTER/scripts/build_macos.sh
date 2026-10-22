#!/usr/bin/env bash
# BANKMASTER BY KALOX — macOS universal build (Apple Silicon + Intel), tests and validation.
# Usage: scripts/build_macos.sh [--skip-validation]
# Requires: Xcode command line tools, CMake >= 3.22. Optional: pluginval in PATH.
# Nothing is installed: the bundles stay in build-mac/. Use installer/macos/build_pkg.sh
# to create the installer.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
BUILD="$ROOT/build-mac"
SKIP_VALIDATION="${1:-}"

cmake -S . -B "$BUILD" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build "$BUILD" --config Release -j "$(sysctl -n hw.ncpu)"

ART="$BUILD/BankMaster_artefacts/Release"
echo "== architectures"
lipo -info "$ART/VST3/BANKMASTER.vst3/Contents/MacOS/BANKMASTER"
lipo -info "$ART/AU/BANKMASTER.component/Contents/MacOS/BANKMASTER"
lipo -info "$ART/Standalone/BANKMASTER.app/Contents/MacOS/BANKMASTER"

echo "== DSP tests (native $(uname -m))"
"$BUILD/bankmaster_dsp_tests"
if arch -x86_64 /usr/bin/true 2>/dev/null; then
  echo "== DSP tests (x86_64 slice via Rosetta)"
  arch -x86_64 "$BUILD/bankmaster_dsp_tests"
fi
echo "== plugin tests"
"$BUILD/bankmaster_plugin_tests_artefacts/Release/bankmaster_plugin_tests"

if [[ "$SKIP_VALIDATION" != "--skip-validation" ]]; then
  # auval needs the component in a scanned folder: use a temporary user-level copy
  COMP_DIR="$HOME/Library/Audio/Plug-Ins/Components"
  if [[ -e "$COMP_DIR/BANKMASTER.component" ]]; then
    echo "!! $COMP_DIR/BANKMASTER.component already exists - not touching it; skipping auval"
  else
    mkdir -p "$COMP_DIR"
    cp -R "$ART/AU/BANKMASTER.component" "$COMP_DIR/"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    sleep 2
    auval -v aufx Bkms Klox
    rm -rf "$COMP_DIR/BANKMASTER.component"
  fi
  if command -v pluginval >/dev/null; then
    pluginval --strictness-level 10 --validate-in-process --validate "$ART/VST3/BANKMASTER.vst3"
  else
    echo "(pluginval not found in PATH - skipped)"
  fi
fi
echo "== done: bundles in $ART"
