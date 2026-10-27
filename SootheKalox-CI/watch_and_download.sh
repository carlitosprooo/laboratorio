#!/bin/bash
# Acompanha o último run do workflow Windows e baixa os artefatos.
# Só gh/shasum; o build acontece na nuvem.
# Uso: bash watch_and_download.sh ["/Volumes/MacOS SSD2T/SootheKalox"]
set -uo pipefail

PROJ="${1:-/Volumes/MacOS SSD2T/SootheKalox}"
OUT="$HOME/Downloads/KaloxSooth_Windows"
cd "$PROJ" || exit 1

git fetch -q origin main
SHA="$(git rev-parse origin/main)"
RUN_ID=""
for i in 1 2 3 4 5 6 7 8 9; do
  RUN_ID="$(gh run list --workflow windows-build.yml --commit "$SHA" -L 1 --json databaseId -q '.[0].databaseId' 2>/dev/null)"
  [ -n "$RUN_ID" ] && break
  sleep 10
done
[ -n "$RUN_ID" ] || { echo "Nenhum run encontrado. Dispare com: gh workflow run windows-build.yml --ref main"; exit 1; }

echo "Run $RUN_ID — $(gh run view "$RUN_ID" --json url -q .url)"
gh run watch "$RUN_ID" --interval 60 --exit-status
STATUS=$?

mkdir -p "$OUT"
if [ $STATUS -ne 0 ]; then
  gh run view "$RUN_ID" --log-failed > "$OUT/failed_run_$RUN_ID.log" 2>&1
  echo; echo "FALHOU. Log salvo em: $OUT/failed_run_$RUN_ID.log"
  gh run download "$RUN_ID" -n KaloxSooth_Windows_Reports -D "$OUT/reports_run_$RUN_ID" 2>/dev/null && echo "Relatórios parciais: $OUT/reports_run_$RUN_ID"
  exit 1
fi

DEST="$OUT/run_$RUN_ID"
gh run download "$RUN_ID" -D "$DEST"
echo; echo "=========== ARTEFATOS ==========="
find "$DEST" -type f \( -name '*.exe' -o -name '*.zip' \) -print0 | while IFS= read -r -d '' f; do
  echo "$f"; shasum -a 256 "$f"
done
SUMS="$(find "$DEST" -name 'SHA256SUMS*' | head -1)"
[ -n "$SUMS" ] && { echo; echo "SHA256SUMS do CI:"; cat "$SUMS"; }
echo; echo "=========== RESULTADOS (PASS/FAIL) ==========="
find "$DEST" -name '*.md' -print0 | while IFS= read -r -d '' f; do
  echo "--- $(basename "$f")"; grep -iE 'pass|fail' "$f" | head -40
done
