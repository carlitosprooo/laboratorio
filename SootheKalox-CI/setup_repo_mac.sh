#!/bin/bash
# Prepara o git do SootheKalox e publica num repositório GitHub PRIVADO.
# Só comandos leves (cp, find, git, gh). Não compila, não testa, não apaga nada.
# Uso: bash setup_repo_mac.sh ["/Volumes/MacOS SSD2T/SootheKalox"] [kaloxsooth]
set -euo pipefail

PROJ="${1:-/Volumes/MacOS SSD2T/SootheKalox}"
REPO_NAME="${2:-kaloxsooth}"
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="${TMPDIR:-/tmp}/kaloxsooth_files_to_push.txt"

confirm() { local a; read -r -p "$1 [s/N] " a; [ "$a" = s ] || [ "$a" = S ]; }
die() { echo "ERRO: $*" >&2; exit 1; }

cd "$PROJ" || die "pasta não encontrada: $PROJ"
for f in CMakeLists.txt Scripts/windows/build_windows.ps1 Scripts/windows/test_installer.ps1 Installer/windows/KaloxSooth.iss; do
  [ -f "$f" ] || die "falta $f em $PROJ"
done

# --- 1. .gitignore e workflow (não sobrescreve sem perguntar) ---------------
install_file() {
  local src="$HERE/$1" dst="$PROJ/$1"
  mkdir -p "$(dirname "$dst")"
  if [ -f "$dst" ] && ! cmp -s "$src" "$dst"; then
    echo "== $1 já existe e é diferente:"; diff -u "$dst" "$src" || true
    confirm "Substituir $1 pela versão do CI?" || { echo "mantido: $1"; return; }
  fi
  cp "$src" "$dst"; echo "ok: $1"
}
install_file .gitignore
install_file .github/workflows/windows-build.yml

# .wav grandes (> 10 MB) fora das pastas já ignoradas
BIGWAV="$(find . \( -path ./build -o -path './build-*' -o -path ./ThirdParty/JUCE -o -path ./Distribution -o -path ./.git \) -prune -o \
          -type f -iname '*.wav' ! -name '._*' -size +10M -print | sed 's|^\./|/|')"
if [ -n "$BIGWAV" ]; then
  echo "== .wav maiores que 10 MB:"; echo "$BIGWAV"
  if confirm "Acrescentar estes .wav ao .gitignore?"; then echo "$BIGWAV" >> .gitignore; fi
fi

# --- 2. git init + revisão do que vai subir ---------------------------------
git config --global user.email >/dev/null || die 'configure: git config --global user.name "Seu Nome"; git config --global user.email "voce@exemplo.com"'
[ -d .git ] || git init -b main
git add -A

if git rev-parse --verify HEAD >/dev/null 2>&1 && git diff --cached --quiet; then
  echo "Commit já existe e não há mudanças novas."
else
  git ls-files -z | xargs -0 du -k > "$LIST.sizes"
  git ls-files > "$LIST"
  echo; echo "================= O QUE VAI PARA O REPOSITÓRIO ================="
  echo "Arquivos: $(wc -l < "$LIST" | tr -d ' ')   Tamanho total: $(awk '{s+=$1} END {printf "%.1f MB", s/1024}' "$LIST.sizes")"
  echo; echo "Por pasta de topo:"; cut -d/ -f1 "$LIST" | sort | uniq -c | sort -rn
  echo; echo "15 maiores arquivos (KB):"; sort -rn "$LIST.sizes" | head -15
  BAD="$(grep -E '(^|/)\._|(^|/)\.DS_Store$|^build/|^build-|^ThirdParty/JUCE/|^Distribution/' "$LIST" || true)"
  [ -z "$BAD" ] || { echo "Arquivos que deveriam estar ignorados:"; echo "$BAD"; die "ajuste o .gitignore"; }
  HUGE="$(awk '$1 > 100*1024 {print}' "$LIST.sizes")"
  [ -z "$HUGE" ] || { echo "$HUGE"; die "arquivos > 100 MB (o GitHub recusa). Ignore-os no .gitignore."; }
  echo; echo "Lista completa: $LIST   (veja com: less \"$LIST\")"
  echo "==============================================================="
  confirm "Confirmar o commit inicial com estes arquivos?" || { git reset -q; die "cancelado (nada commitado)"; }
  git commit -q -m "KaloxSooth 1.3.0: código-fonte + CI Windows (GitHub Actions)"
  echo "Commit criado: $(git log --oneline -1)"
fi

# --- 3. GitHub: login, repositório privado, push ----------------------------
gh auth status >/dev/null 2>&1 || die "não está logado no gh. Rode:  gh auth login --hostname github.com --git-protocol https --web   e rode este script de novo."
gh auth status

if ! git remote get-url origin >/dev/null 2>&1; then
  echo; echo "Vou criar o repositório PRIVADO '$REPO_NAME' na sua conta GitHub."
  confirm "Criar o repositório privado '$REPO_NAME'?" || die "cancelado"
  gh repo create "$REPO_NAME" --private --source=. --remote=origin
fi
VIS="$(gh repo view --json visibility -q .visibility)"
[ "$VIS" = PRIVATE ] || die "o repositório NÃO está privado (visibility=$VIS). Push cancelado."

echo; echo "Repositório: $(gh repo view --json url -q .url) (PRIVATE)"
echo "O push na main dispara o workflow 'Windows build (KaloxSooth)' no runner windows-latest."
confirm "Fazer o push agora?" || die "cancelado (nada enviado)"
git push -u origin main

echo; echo "Push feito. Para acompanhar e baixar os artefatos:"
echo "  bash \"$HERE/watch_and_download.sh\" \"$PROJ\""
