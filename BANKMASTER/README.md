# BANKMASTER — BY KALOX · 4-IN-1 MASTERING SUITE · 2.0

Plugin de masterização com quatro processadores em uma única instância:

**INPUT → MASTER EQ → MASTER SATURATION → MASTER COMPRESSOR → MASTER LIMITER → OUTPUT**

Formatos: macOS Universal (Apple Silicon + Intel, 10.15+) — VST3 · Audio Unit · Standalone;
Windows 10/11 x64 — VST3 · Standalone. Fabricante: **Kalox Audio**. Uma única base de código JUCE/C++:
mesma interface, mesmo som e mesmos controles nas duas plataformas.
Todo o DSP é implementação própria (sem código, interface ou algoritmo de terceiros), funciona offline
e não depende de plugins FabFilter instalados.

![EQ](docs/screenshots/v2/v2_eq_1200x780_2x.png)

| Saturação | Compressor | Limiter |
|---|---|---|
| ![](docs/screenshots/v2/v2_saturation_1200x780_1x.png) | ![](docs/screenshots/v2/v2_compressor_1200x780_1x.png) | ![](docs/screenshots/v2/v2_limiter_1200x780_1x.png) |

## Instaladores

| Plataforma | Arquivo | Conteúdo |
|---|---|---|
| macOS Universal (arm64 + x86_64) | `BANKMASTER-BY-KALOX-2.0.0-macOS-Universal.dmg` | instalador guiado (.pkg) VST3 / AU / Standalone, Read Me, desinstalador |
| Windows 10/11 x64 | `BANKMASTER-BY-KALOX-2.0.0-Windows-x64-Setup.exe` | assistente gráfico: VST3 / Standalone, desinstalador |

Gerados e testados (instalação + desinstalação reais) pelo GitHub Actions. Ainda **sem assinatura digital**
— detalhes em `docs/INSTALLERS.md`.

## Uso rápido (Ableton Live 12)

1. Instale pelo DMG (macOS) ou Setup.exe (Windows) e faça *Rescan* dos plugins.
2. Coloque **uma instância** de BANKMASTER na Master. Ela abre no preset **KALOX MASTER - ORIGINAL**.
3. Toque o trecho mais alto da música, clique **LEARN** (barra inferior), espere alguns segundos e clique de
   novo: o ganho de entrada é ajustado para que a mix chegue à cadeia em −20 LUFS (o alvo é ajustável
   arrastando o valor no próprio botão).
4. A faixa de fluxo **INPUT → EQ → SATURATION → COMPRESSOR → LIMITER → OUTPUT** abre cada módulo; o ⏻ de
   cada nó liga/desliga o módulo sem clique. INPUT e OUTPUT são ajustados arrastando o valor (duplo clique = 0 dB).
5. Cabeçalho: navegador de presets (◀ nome ▶, salvar, ⟲ voltar ao preset carregado com confirmação),
   **A | B** + copiar, **↶ UNDO | REDO ↷** (⌘Z / ⇧⌘Z, Ctrl+Z / Ctrl+Y), **ONE MASTER**, menu (tamanho da janela).
6. Barra global: INPUT, LUFS M/S/I, TRUE PEAK (clique = zerar), GR do compressor e do limiter, correlação,
   OUTPUT, LEARN e BYPASS.
7. Exporte pela DAW normalmente — a latência (7,4 ms @ 48 kHz no preset KALOX) é compensada pelo Live.

Knobs: arrastar, Shift = ajuste fino, roda do mouse, duplo clique = valor padrão, duplo clique no valor = digitar.
EQ: arraste os nós (Shift = só frequência), roda do mouse = Q, duplo clique no nó = liga/desliga,
duplo clique no fundo = nova banda. Janela redimensionável de 1080 × 700 a 2000 × 1300 (padrão 1200 × 780).

**Atualizando da 1.0:** instale por cima. Projetos existentes continuam abrindo com os mesmos ajustes
(mesmos IDs do plugin, som idêntico — ver `docs/V2_REPORT.md`). No Live, faça *Rescan* (Option/Alt +
Rescan = completo) para o plugin aparecer em **Kalox Audio**.

## Presets

| Preset | Observação |
|---|---|
| KALOX MASTER - ORIGINAL | parâmetros dos seus prints (confirmados + estimados, ver `docs/PARAMETERS.md`) |
| AFRO HOUSE / DEEP HOUSE / TECH HOUSE / CLUB MASTER | pontos de partida; ≈ −9 / −10 / −8,5 / −7,5 LUFS-I com entrada em −20 LUFS |
| TRANSPARENT MASTER | processamento leve, ≈ −12 LUFS-I |
| CUSTOM USER PRESET | base neutra (teto −1 dBTP, ≈ −14 LUFS-I) para criar o seu |

Presets são pontos de partida, não garantia de master pronto para qualquer plataforma.
O teto −0,1 dBTP é o do seu preset; para streaming é comum −1 dBTP.

## Compilar

macOS (Xcode CLT + CMake ≥ 3.22):

```bash
scripts/build_macos.sh              # universal arm64+x86_64, testes (nativo + Rosetta), auval, pluginval
installer/macos/build_pkg.sh        # dist/BANKMASTER-BY-KALOX-2.0.0-macOS-universal.pkg
```

Nada é instalado automaticamente (`COPY_PLUGIN_AFTER_BUILD FALSE`); o instalador só grava
`BANKMASTER.vst3`, `BANKMASTER.component` e `BANKMASTER.app` — KaloxStart e KaloxSooth não são tocados.
Assinatura Developer ID / notarização: variáveis `BM_SIGN_APP`, `BM_SIGN_INSTALLER`, `BM_NOTARY_PROFILE`
(sem elas o pacote sai com assinatura ad-hoc e o macOS pede confirmação na instalação).

Linux / Windows: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`.
JUCE 8.0.4 é baixado automaticamente (ou use `-DBANKMASTER_JUCE_DIR=/caminho/JUCE`).

Testes: `build/bankmaster_dsp_tests` e `build/bankmaster_plugin_tests_artefacts/Release/bankmaster_plugin_tests`.
Render/comparação offline: `bankmaster_render --help`.

## Documentação

- `docs/PARAMETERS.md` — parâmetros confirmados, estimados e pendentes dos prints
- `docs/ARCHITECTURE.md` — DSP, tempo real, latência, estrutura de arquivos
- `docs/TEST_REPORT.md` — os 15 testes exigidos com resultados medidos
- `docs/FINAL_REPORT.md` — relatório final (arquivos, testes, limitações, pendências)
- `docs/INSTALLERS.md` — instaladores DMG / EXE, testes de instalação, assinatura
- `docs/V2_REPORT.md` — relatório da versão 2.0 (interface, Kalox Audio, teste nulo V1 × V2, validação)
- `Design/README.md` — design system V2, arte e mockups

## Licenças

Construído com JUCE (https://juce.com) sob os termos da licença JUCE — para distribuição comercial,
verifique o plano JUCE adequado. VST é marca da Steinberg; Audio Units, da Apple. FabFilter, Pro-Q,
Saturn, Pro-C e Pro-L são marcas da FabFilter B.V.; o BANKMASTER não é afiliado à FabFilter.
