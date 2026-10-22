# BANKMASTER — BY KALOX 2.0 · relatório da versão (redesign premium)

Data: 22/10/2026 · Branch `claude/quirky-curie-aoazly` · Base: conceito A — OBSIDIAN / NEON aprovado.

> Execução final: GitHub Actions run #12 — https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116
> (macOS, Windows e Linux verdes). Instaladores e checksums: seção 7.

## 1. O que mudou

| Área | 2.0 |
|---|---|
| Interface | nova, em um único código JUCE/C++ para macOS e Windows: cabeçalho (presets, A/B, **↶ UNDO \| REDO ↷**, voltar ao preset com confirmação, ONE MASTER, menu), faixa de fluxo INPUT → EQ → SATURATION → COMPRESSOR → LIMITER → OUTPUT, quatro páginas (display + mesa de controles), barra global (INPUT, LUFS M/S/I, TRUE PEAK, GR, correlação, OUTPUT, LEARN, BYPASS) |
| Identidade | logo refinada (aro usinado, brilho de vidro, curva em degradê, teto duplo, joelho com halo; versão simplificada em 16–40 px), wordmark "BANKMASTER — BY KALOX" (sempre maiúsculas), ícone do app (macOS .icns / Windows .ico), arte nova do DMG e do Setup |
| Paleta | preto obsidiana, grafite, verde neon; cores funcionais: EQ verde, SATURATION âmbar, COMPRESSOR turquesa, LIMITER coral |
| Tipografia | Inter + Montserrat embutidas (SIL OFL 1.1): texto idêntico nas duas plataformas; menor texto 10,5 px a 100 %; contraste dos textos secundários aumentado |
| Janela | redimensionável e proporcional: mínimo 1080 × 700, padrão 1200 × 780, máximo 2000 × 1300; tamanhos rápidos no menu; tamanho salvo com o projeto |
| Fabricante | "Kalox Audio" (era "KALOX") — códigos `Klox` / `Bkms`, IDs VST3, componente AU e bundle ID **inalterados** |
| Valores | nunca mais "−0.00" (exibição) |
| Som | **nenhuma alteração** — ver seção 3 |

## 2. Fabricante "Kalox Audio" — compatibilidade

| Identificador usado pelos projetos | 1.0 | 2.0 |
|---|---|---|
| VST3 class ID (componente) | `ABCDEF019182FAEB4B6C6F78426B6D73` | idêntico |
| VST3 uniqueId (JUCE) | `b2f4fe85` | idêntico |
| AU tipo / subtipo / fabricante | `aufx Bkms Klox` | idêntico |
| Bundle ID | `com.kalox.bankmaster` | idêntico |
| IDs dos parâmetros (97) | `in_gain` … `lim_dither` | idênticos (mesmo nome, padrão, passos) |
| Vendor exibido | KALOX | **Kalox Audio** (VST3 `moduleinfo.json`, AU "Kalox Audio: BANKMASTER", "Publisher" do Windows) |

Projetos do Ableton Live salvos com a 1.0 abrem a 2.0 com os mesmos ajustes. O Live passa a listar o plugin
em **Kalox Audio** depois de um *Rescan* (Option/Alt + Rescan faz a varredura completa); até lá, o cache do
Live pode mostrar "KALOX". No macOS o instalador reinicia o cache de Audio Units; se um host AU ainda mostrar
o nome antigo, logout/login resolve. Presets do usuário continuam na mesma pasta (`…/KALOX/BANKMASTER/Presets`).

## 3. Áudio V1 × V2 — teste nulo com os binários compilados

Ferramenta `Tools/HostCheck.cpp` (`bankmaster_hostcheck`): carrega o plugin **compilado e instalado** como um
host faz, renderiza o mesmo programa musical determinístico e compara amostra por amostra.
Cenários: estado padrão (48 kHz / 512 e 44,1 kHz / 256), os 7 presets de fábrica, e um estado aleatório de
todos os parâmetros salvo pela 1.0 e carregado na 2.0 (prova também a compatibilidade de estado).

| Plataforma | Comparação | Resultado |
|---|---|---|
| Linux x86_64 (local) | 1.0 compilada do commit `30f07e4` × 2.0, VST3 | 10/10 cenários **bit-idênticos**; latência, parâmetros e estado restaurado idênticos |
| macOS arm64 (CI) | **1.0 publicada (run #8)** × 2.0 instalada, VST3 e AU | 10/10 **bit-idênticos** em cada formato |
| macOS x86_64 (CI, Rosetta) | **1.0 publicada** × 2.0, VST3 e AU | 10/10 **bit-idênticos** em cada formato |
| Windows x64 (CI) | **1.0 publicada (Setup run #8)** × 2.0 instalada, VST3 | 10/10 **bit-idênticos** |

"Bit-idêntico" = diferença máxima 0 em todas as amostras (não apenas abaixo de um limiar).
Além disso, `Source/DSP` tem **0 linhas alteradas** em relação à 1.0 (`git diff 30f07e4 -- BANKMASTER/Source/DSP`).
A única diferença de texto detectada: no Apple Silicon a 1.0 exibia "-0.00 dB" em alguns valores zero; a 2.0
exibe "0.00 dB" (correção pedida, apenas exibição — listada linha a linha no relatório do CI).

## 4. Interface — testes

| Teste | Resultado |
|---|---|
| Os 97 parâmetros ligados a um controle, nos dois sentidos (host → UI e UI → host) | 97/97, 0 falhas |
| ↶ UNDO / REDO ↷ pelos próprios botões; atalhos ⌘Z / ⇧⌘Z / Ctrl+Y | OK |
| Voltar ao preset: pede confirmação; CANCEL mantém; REVERT restaura exatamente; desfazível em um passo; também para preset do usuário | OK |
| Tamanhos 1080 × 700, 1200 × 780, 1600 × 1040, 2000 × 1300 e proporções diferentes: conteúdo proporcional, nada cortado | OK |
| Tamanho, página aberta e banda do EQ restaurados com o projeto; preferências fora do estado dos parâmetros | OK |
| A/B, navegação de presets, nós do fluxo abrem as páginas | OK |
| pluginval nível 10 (inclui abrir o editor durante o processamento e automação com o editor aberto) | macOS VST3 + AU, Windows VST3, Linux VST3: SUCCESS |

Screenshots reais do plugin (renderizados pelo próprio sistema): `docs/screenshots/v2/` (Linux) e nos
artefatos do CI: macOS (Retina 2×, CoreGraphics) e Windows (100 %, 125 %, 150 %).

## 5. Validação por plataforma

| | macOS Universal (arm64 + x86_64) | Windows x64 |
|---|---|---|
| Build | VST3, AU, Standalone universais | VST3, Standalone |
| Testes DSP | 19 testes / 133 verificações, 0 falhas (arm64 e x86_64) | 19 / 133, 0 falhas |
| Testes do plugin + UI | 165 verificações, 0 falhas | 165 verificações, 0 falhas |
| Identidade | vendor Kalox Audio, CID e códigos inalterados, ícone `.icns` | vendor Kalox Audio, CID inalterado, ícone no `.exe` |
| auval | SUCCEEDED arm64 + x86_64 (também após o upgrade) | — |
| pluginval 10 | VST3 + AU SUCCESS | VST3 SUCCESS |
| Validador Steinberg | 47/0 arm64 e x86_64 (build e instalado) | 47/0 (build e instalado) |
| Instalação limpa | só os 3 itens BANKMASTER; desinstalador → sistema idêntico | só VST3 + `Program Files\Kalox Audio\BANKMASTER`; desinstalação → sistema e registro idênticos |
| Upgrade 1.0 → 2.0 real | itens substituídos, versão 2.0.0, IDs iguais, auval OK | 1.0 substituída (mesmo AppId), pasta `KALOX` e chave antiga removidas, Publisher Kalox Audio |

## 6. O que não foi possível testar aqui

- Ableton Live 12 real (abrir um projeto 1.0, ver o agrupamento "Kalox Audio" na lista de plugins): exige o
  seu computador. A compatibilidade foi provada pelos IDs idênticos e pelo teste nulo com estado salvo pela 1.0.
- Telas físicas Retina / Windows 125–150 %: validadas por renderização na escala correspondente no próprio
  sistema operacional do CI, não por um monitor físico.
- Assinatura digital: os instaladores continuam **sem** Developer ID / notarização (macOS) e sem Authenticode
  (Windows) — o sistema pede confirmação na primeira abertura (instruções no Read Me).

## 7. Instaladores 2.0.0 (run #12)

| Instalador | Tamanho | SHA-256 | Download |
|---|---|---|---|
| `BANKMASTER-BY-KALOX-2.0.0-macOS-Universal.dmg` | 20.224.399 bytes | `cc57bc45afa696dbc4306f51ba2b72cb06f3fc8eb9cec7a59fa1dfb2b2cfcf93` | https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12461921955 |
| `BANKMASTER-BY-KALOX-2.0.0-Windows-x64-Setup.exe` | 5.758.336 bytes | `347340f8774707c88db239462ed9f7c328a4ec1ced852c1e2cee401b08699920` | https://github.com/carlitosprooo/laboratorio/actions/runs/39859554116/artifacts/12461708615 |

Evidências do run #12 (logs completos nos artefatos de validação):

- macOS: `hdiutil verify` VALID (UDZO); `.pkg` dentro do DMG idêntico ao validado; instalação real adicionou
  apenas `BANKMASTER.vst3`, `BANKMASTER.component` e `BANKMASTER.app` (todos `x86_64 arm64`); auval no componente
  instalado SUCCEEDED arm64 + x86_64; Steinberg 47/0 no VST3 instalado; desinstalação → sistema idêntico, recibos
  removidos. Upgrade a partir do DMG 1.0 publicado (sha256 `4aed11c8…`): "The upgrade was successful", os três itens
  em 2.0.0, identidade VST3/AU inalterada, auval OK após o upgrade.
- Teste nulo macOS (1.0 publicada × 2.0): VST3 arm64, AU arm64, VST3 x86_64, AU x86_64 — 10/10 cenários
  bit-idênticos em cada um. Única diferença de texto (só arm64): 9 parâmetros exibiam "-0.00 dB" na 1.0 e agora
  "0.00 dB" (Input Gain, Output Gain, EQ 1/3/4 Gain, EQ 1/2/5 Dyn Range, Comp Makeup).
- Windows: Setup instala só o VST3 e `Program Files\Kalox Audio\BANKMASTER`; "Aplicativos instalados":
  BANKMASTER BY KALOX 2.0.0, Publisher Kalox Audio; Steinberg 47/0 no VST3 instalado; desinstalação → arquivos e
  registro idênticos ao estado inicial. Upgrade a partir do Setup 1.0 publicado (sha256 `560dc5c1…`): códigos 0,
  pasta `KALOX` e chave antiga removidas, identidade VST3 inalterada; teste nulo 10/10 bit-idêntico, textos idênticos.
- Identidade (ambas): `moduleinfo.json` Vendor "Kalox Audio", CIDs idênticos à 1.0; AU "Kalox Audio: BANKMASTER",
  `aufx Bkms Klox`; ícone novo no app macOS (`Icon.icns`) e no `.exe` do Windows.
