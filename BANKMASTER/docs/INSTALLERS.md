# BANKMASTER by KALOX 1.0.0 — instaladores

Gerados e testados pelo GitHub Actions (`.github/workflows/bankmaster.yml`) em máquinas limpas,
sem nenhum plugin FabFilter. DSP, presets e código do plugin são os mesmos já validados.

## Downloads — run #8 (https://github.com/carlitosprooo/laboratorio/actions/runs/39829983406)

| Instalador | Tamanho | SHA-256 | Artefato |
|---|---|---|---|
| `BANKMASTER-by-KALOX-1.0.0-macOS-Universal.dmg` | 11.385.637 bytes (10,9 MB) | `4aed11c8b6eb31c190e41b2d0de130d525d6788b6defa790d2c15b85f727193d` | https://github.com/carlitosprooo/laboratorio/actions/runs/39829983406/artifacts/12448568065 |
| `BANKMASTER-by-KALOX-1.0.0-Windows-x64-Setup.exe` | 4.922.364 bytes (4,7 MB) | `560dc5c17e8a41763dd28633f2ad56ec7bb787c40832d8beb0f8bc0c022d733c` | https://github.com/carlitosprooo/laboratorio/actions/runs/39829983406/artifacts/12449780076 |

Logs: macOS https://github.com/carlitosprooo/laboratorio/actions/runs/39829983406/artifacts/12448738045 ·
Windows https://github.com/carlitosprooo/laboratorio/actions/runs/39829983406/artifacts/12449246997
(os artefatos chegam como .zip contendo o instalador e o arquivo `.sha256`; expiram em 20/01/2027; download exige login no GitHub).

Resultado medido no run #8 — macOS: `hdiutil verify` VALID (UDZO); `.pkg` interno idêntico ao validado;
instalação real adicionou exatamente `/Applications/BANKMASTER.app`, `/Library/Audio/Plug-Ins/Components/BANKMASTER.component`
e `/Library/Audio/Plug-Ins/VST3/BANKMASTER.vst3` (todos `x86_64 arm64`); auval no componente instalado SUCCEEDED
(arm64 e x86_64); validador Steinberg no VST3 instalado 47/0; desinstalador → sistema idêntico ao inicial, recibos removidos.
Windows: instalação silenciosa código 0; somente o bundle VST3 e `Program Files\KALOX\BANKMASTER`
(exe, ícone, Read Me, desinstalador); registro OK; validador Steinberg no VST3 instalado 47/0; desinstalação
código 0 → sistema idêntico ao inicial, chaves de registro removidas.

Correções feitas durante esta validação: bundles do `.pkg` marcados como não-relocáveis (antes o macOS
Installer podia atualizar uma cópia do app encontrada em outra pasta em vez de instalar em `/Applications`).

## macOS — `BANKMASTER-by-KALOX-1.0.0-macOS-Universal.dmg`

| | |
|---|---|
| Compatibilidade | macOS 10.15+; Apple Silicon (arm64) e Intel (x86_64) — binários universais |
| Conteúdo do DMG | `Install BANKMASTER.pkg` (o mesmo `.pkg` já validado, byte a byte), `Read Me - Leia-me.txt`, `Uninstall BANKMASTER.command` |
| Apresentação | fundo azul-gelo com a identidade BANKMASTER (1× + 2× Retina), ícone de volume, ícones posicionados, guia de 3 passos |
| Instalação (guiada, "Personalizar") | VST3 → `/Library/Audio/Plug-Ins/VST3/BANKMASTER.vst3` · AU → `/Library/Audio/Plug-Ins/Components/BANKMASTER.component` · Standalone → `/Applications/BANKMASTER.app` |
| Desinstalação | `Uninstall BANKMASTER.command`: remove só esses três itens (caminhos fixos, sem curingas) e os recibos `com.kalox.bankmaster.*`; presets do usuário são mantidos |
| Formato | UDIF UDZO (zlib), HFS+, checksum CRC32 |
| Assinatura | **não assinado** com Developer ID e **não notarizado** (bundles com assinatura ad-hoc). O Gatekeeper bloqueia o duplo clique: botão direito → Abrir, ou Ajustes → Privacidade e Segurança → Abrir Mesmo Assim |

Testes automáticos (CI macOS):
`hdiutil verify` (checksum VALID) → montagem → layout presente → `cmp` do `.pkg` interno com o validado →
instalação real com `sudo installer` → diferença do sistema de arquivos antes/depois (somente os 3 itens
BANKMASTER adicionados, nada removido) → `lipo` dos binários instalados → `auval -strict` no componente
instalado (arm64 + x86_64) → validador Steinberg no VST3 instalado → desinstalador do DMG →
sistema de arquivos idêntico ao inicial e recibos removidos.

## Windows — `BANKMASTER-by-KALOX-1.0.0-Windows-x64-Setup.exe`

| | |
|---|---|
| Compatibilidade | Windows 10 / 11, 64-bit (x64; `MinVersion=10.0`, instalação em modo 64-bit) |
| Ferramenta | Inno Setup 6 (assistente moderno, inglês e português do Brasil) |
| Identidade visual | imagem lateral e ícone BANKMASTER, ícone do Setup e do desinstalador |
| Componentes | VST3 (padrão) e Standalone; tipos "VST3 + Standalone", "Somente VST3", "Personalizado" |
| Destinos | VST3 → `C:\Program Files\Common Files\VST3\BANKMASTER.vst3` · Standalone → `C:\Program Files\KALOX\BANKMASTER\BANKMASTER.exe` (+ atalho no Menu Iniciar) |
| Arquivos instalados | somente o bundle VST3 (binário + `moduleinfo.json`), o exe Standalone, `bankmaster.ico`, `Read Me.txt` e o desinstalador |
| Registro | "Aplicativos instalados" (nome, versão 1.0.0, KALOX, ícone, desinstalador) e `HKLM\SOFTWARE\KALOX\BANKMASTER` (Version, InstallDir, VST3Path) |
| Desinstalador | `unins000.exe` / Configurações → Aplicativos; remove arquivos e chaves; presets do usuário (`%APPDATA%\KALOX\BANKMASTER`) são mantidos |
| Assinatura | **não assinado** (Authenticode: NotSigned). O SmartScreen mostra "O Windows protegeu o computador" → Mais informações → Executar assim mesmo |

Testes automáticos (CI Windows):
compilação ISCC → SHA-256, versão e status Authenticode → instalação silenciosa (`/VERYSILENT /COMPONENTS=vst3,standalone`,
código 0) → lista exata de arquivos adicionados → chaves de registro → validador Steinberg no VST3 instalado
(47 aprovados, 0 falhas) → desinstalação silenciosa (código 0) → sistema de arquivos idêntico ao inicial e
chaves de registro removidas.

## Para distribuição comercial

- macOS: Apple Developer ID Application + Installer e notarização — `BM_SIGN_APP`, `BM_SIGN_INSTALLER`,
  `BM_NOTARY_PROFILE` (`installer/macos/build_pkg.sh`, `installer/macos/dmg/build_dmg.sh`).
- Windows: certificado de assinatura de código (OV/EV) e `signtool` sobre o Setup e o desinstalador
  (`SignTool=` no `.iss`).
- Licença JUCE adequada ao uso comercial.

## Reproduzir localmente

```bash
# macOS
scripts/build_macos.sh
installer/macos/build_pkg.sh
python3 -m pip install dmgbuild && installer/macos/dmg/build_dmg.sh dist/BANKMASTER-by-KALOX-1.0.0-macOS-universal.pkg
# Windows (Inno Setup 6)
ISCC.exe /DArtefacts=<build>\BankMaster_artefacts\Release /DOutDir=<saida> installer\windows\BANKMASTER.iss
# arte (requer Pillow + fonte Inter)
python3 installer/assets/generate_assets.py
```
