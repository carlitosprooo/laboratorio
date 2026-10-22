#!/bin/bash
# BANKMASTER by KALOX — uninstaller (macOS)
# Removes ONLY the files installed by "Install BANKMASTER.pkg" and their installer receipts.
# Other plug-ins (KaloxStart, KaloxSooth, ...) and your user presets are never touched.
# User presets live in ~/Library/Application Support/KALOX/BANKMASTER/Presets (kept).
#
#   Uninstall BANKMASTER.command          interactive (asks for confirmation + admin password)
#   Uninstall BANKMASTER.command --yes    non-interactive (used by the automated tests)
set -u

TARGETS=(
  "/Library/Audio/Plug-Ins/VST3/BANKMASTER.vst3"
  "/Library/Audio/Plug-Ins/Components/BANKMASTER.component"
  "/Applications/BANKMASTER.app"
)
RECEIPTS=(com.kalox.bankmaster.vst3 com.kalox.bankmaster.au com.kalox.bankmaster.app)

echo "BANKMASTER by KALOX - uninstaller"
echo
found=0
for t in "${TARGETS[@]}"; do
  if [[ -e "$t" ]]; then echo "  will remove: $t"; found=1; fi
done
for r in "${RECEIPTS[@]}"; do
  if pkgutil --pkg-info "$r" >/dev/null 2>&1; then echo "  will forget receipt: $r"; found=1; fi
done
if [[ $found -eq 0 ]]; then
  echo "BANKMASTER is not installed. Nothing to do."
  exit 0
fi

if [[ "${1:-}" != "--yes" ]]; then
  echo
  read -r -p "Remove BANKMASTER now? [y/N] " answer
  [[ "$answer" =~ ^[Yy]$ ]] || { echo "Cancelled."; exit 0; }
fi

for t in "${TARGETS[@]}"; do
  # exact, fixed paths only - never wildcards
  [[ -e "$t" ]] && sudo /bin/rm -rf -- "$t"
done
for r in "${RECEIPTS[@]}"; do
  pkgutil --pkg-info "$r" >/dev/null 2>&1 && sudo /usr/sbin/pkgutil --forget "$r" >/dev/null
done
killall -9 AudioComponentRegistrar 2>/dev/null || true

remaining=0
for t in "${TARGETS[@]}"; do [[ -e "$t" ]] && { echo "  still present: $t"; remaining=1; }; done
if [[ $remaining -eq 0 ]]; then
  echo "BANKMASTER was removed. Rescan plug-ins in your DAW."
  exit 0
fi
echo "Some files could not be removed."
exit 1
