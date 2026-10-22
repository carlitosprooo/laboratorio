# BANKMASTER — BY KALOX 2.0.0 — instaladores

Gerados e testados pelo GitHub Actions (`.github/workflows/bankmaster.yml`) em máquinas limpas,
sem nenhum plugin FabFilter. DSP e presets idênticos aos da 1.0 (teste nulo bit-idêntico, ver `V2_REPORT.md`).

## Downloads — 2.0.0

| Instalador | Tamanho | SHA-256 | Download (artefato do run #12) |
|---|---|---|---|
| `BANKMASTER-BY-KALOX-2.0.0-macOS-Universal.dmg` | 20.224.399 bytes (19,3 MB) | `cc57bc45afa696dbc4306f51ba2b72cb06f3fc8eb9cec7a59fa1dfb2b2cfcf93` | https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12461921955 |
| `BANKMASTER-BY-KALOX-2.0.0-Windows-x64-Setup.exe` | 5.758.336 bytes (5,5 MB) | `347340f8774707c88db239462ed9f7c328a4ec1ced852c1e2cee401b08699920` | https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12461708615 |

Run #12: https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116 (macOS, Windows e Linux verdes).
Logs: macOS https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12461637633 ·
Windows https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12462262273
(os artefatos são .zip com o instalador e o `.sha256`; expiram em 20/01/2027; download exige login no GitHub).
Nenhum dos dois tem assinatura digital (pkg/DMG: "no signature"; Setup: Authenticode NotSigned).

Testes do CI em cada instalador: instalação real, lista exata de arquivos instalados, validadores no plugin
INSTALADO, desinstalação → sistema idêntico ao inicial; **upgrade real 1.0 → 2.0** a partir do instalador 1.0
publicado (run #8) e teste nulo de áudio 1.0 × 2.0 nos plugins instalados.

## macOS — `BANKMASTER-BY-KALOX-2.0.0-macOS-Universal.dmg`

| | |
|---|---|
| Compatibilidade | macOS 10.15+; Apple Silicon (arm64) e Intel (x86_64) — binários universais |
| Conteúdo do DMG | `Install BANKMASTER.pkg` (o mesmo `.pkg` já validado, byte a byte), `Read Me - Leia-me.txt`, `Uninstall BANKMASTER.command` |
| Apresentação | identidade V2 (faixa obsidiana com logo e wordmark BY KALOX, 1× + 2× Retina), ícone de volume novo, guia de 3 passos |
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

## Windows — `BANKMASTER-BY-KALOX-2.0.0-Windows-x64-Setup.exe`

| | |
|---|---|
| Compatibilidade | Windows 10 / 11, 64-bit (x64; `MinVersion=10.0`, instalação em modo 64-bit) |
| Ferramenta | Inno Setup 6 (assistente moderno, inglês e português do Brasil) |
| Identidade visual | imagem lateral V2 (logo + BANKMASTER — BY KALOX), ícone novo do Setup, do desinstalador e do app |
| Componentes | VST3 (padrão) e Standalone; tipos "VST3 + Standalone", "Somente VST3", "Personalizado" |
| Destinos | VST3 → `C:\Program Files\Common Files\VST3\BANKMASTER.vst3` · Standalone → `C:\Program Files\Kalox Audio\BANKMASTER\BANKMASTER.exe` (+ atalho no Menu Iniciar) |
| Arquivos instalados | somente o bundle VST3 (binário + `moduleinfo.json`), o exe Standalone, `bankmaster.ico`, `Read Me.txt` e o desinstalador |
| Registro | "Aplicativos instalados" (nome, versão 2.0.0, Publisher **Kalox Audio**, ícone, desinstalador) e `HKLM\SOFTWARE\Kalox Audio\BANKMASTER` (Version, InstallDir, VST3Path) |
| Upgrade da 1.0 | mesmo AppId: substitui a 1.0; remove `Program Files\KALOX\BANKMASTER`, o grupo do Menu Iniciar e `HKLM\SOFTWARE\KALOX\BANKMASTER` da 1.0 |
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
python3 -m pip install dmgbuild && installer/macos/dmg/build_dmg.sh dist/BANKMASTER-BY-KALOX-2.0.0-macOS-universal.pkg
# Windows (Inno Setup 6)
ISCC.exe /DArtefacts=<build>\BankMaster_artefacts\Release /DOutDir=<saida> installer\windows\BANKMASTER.iss
# arte (requer Pillow + fonte Inter)
python3 installer/assets/generate_assets.py
```
