# SootheKalox — CI Windows (GitHub Actions)

Arquivos para gerar `KaloxSooth_1.3.0_Windows_x64_Setup.exe` e o ZIP do VST3 na nuvem
(runner `windows-latest`), sem compilar nada no Mac.

| Arquivo | Destino no projeto |
|---|---|
| `.gitignore` | raiz do SootheKalox |
| `.github/workflows/windows-build.yml` | `.github/workflows/` do SootheKalox |
| `setup_repo_mac.sh` | roda no Mac (não é copiado para o projeto) |
| `watch_and_download.sh` | roda no Mac (não é copiado para o projeto) |

## No Mac (só comandos leves)

```bash
gh repo clone carlitosprooo/laboratorio /tmp/kalox-ci -- --depth 1 -b claude/gallant-meitner-m1hhls
bash /tmp/kalox-ci/SootheKalox-CI/setup_repo_mac.sh "/Volumes/MacOS SSD2T/SootheKalox"
bash /tmp/kalox-ci/SootheKalox-CI/watch_and_download.sh "/Volumes/MacOS SSD2T/SootheKalox"
```

`setup_repo_mac.sh` copia os 2 arquivos (pergunta antes de sobrescrever), faz `git init -b main` e `git add -A`.
Depois mostra a contagem, o tamanho, as pastas e os maiores arquivos, e bloqueia `._*`, `build*`, JUCE, `Distribution/`
e arquivos acima de 100 MB. Ele pede confirmação **antes do commit**, **antes de `gh repo create --private`** e
**antes do push**, e cancela se o repositório não estiver PRIVATE.
O push na `main` já dispara o workflow, então não precisa de `gh workflow run`.

`watch_and_download.sh` acompanha o run do commit atual (`gh run watch --interval 60`) e baixa os artefatos em
`~/Downloads/KaloxSooth_Windows/run_<id>/`. Depois imprime os SHA-256 e as linhas PASS/FAIL dos relatórios.
Se o run falhar, salva `gh run view --log-failed` nessa pasta.

## Workflow

checkout → clone do JUCE `8.0.16-security` → Inno Setup 6 (choco) → pluginval (release oficial mais recente) →
`build_windows.ps1 -Pluginval <exe>` → `test_installer.ps1` → artefatos:
- `KaloxSooth_1.3.0_Windows_x64`: `Distribution/Windows/*` (Setup.exe, ZIP do VST3, SHA256SUMS)
- `KaloxSooth_Windows_Reports`: `TestReport_Windows.md`, `Benchmarks_Windows.md`, `InstallerTest.md`
  (sobe também quando o run falha)

## Problemas comuns
- `gh` sem login: `gh auth login --hostname github.com --git-protocol https --web`
- Avisos do git sobre `._pack-*.idx` dentro de `.git` (exFAT): são metadados AppleDouble do macOS.
  Remover só esses: `find .git -name '._*' -delete`
