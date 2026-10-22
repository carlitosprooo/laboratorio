# BANKMASTER — BY KALOX · Redesign V2.0 (fase de mockups)

**Status: aguardando aprovação.** Nada no plug-in foi alterado: DSP, presets, parâmetros, CMake do plug-in,
instaladores e interface atual estão intactos. Esta pasta contém apenas a proposta visual.

## Como os mockups foram feitos

- `V2/DesignSystem.h` — o sistema visual (paletas A/B, tipografia, knobs, botões, seletores, ícones,
  medidores, logo e wordmark) escrito em JUCE/C++ puro. **É o mesmo código que vai desenhar a interface
  real** depois da aprovação, em macOS e Windows.
- `Preview/` — programa separado (não entra no plug-in) que desenha as telas com esse sistema.
  Os dados mostrados são reais: o DSP atual, sem nenhuma alteração, processa uma mix sintética com o preset
  **KALOX MASTER — ORIGINAL** (entrada em −23,4 LUFS, INPUT LEARN → +3,4 dB → −20 LUFS na cadeia):
  espectro pré/pós-EQ, curvas de EQ (`bandMagnitudeDb`), curva do compressor (`CompCurve`), curva e
  harmônicos da saturação (`Shaper` + processador real), redução de ganho do compressor e do limiter,
  LUFS (−9,4 I), true peak (−0,10 dBTP), correlação e latência (357 amostras).
- Fontes: Inter e Montserrat (SIL Open Font License 1.1, licenças em `Fonts/`), embutidas no binário —
  o texto fica idêntico no macOS e no Windows, sem depender de fontes do sistema.

Reproduzir: `cmake -S Design/Preview -B build-design && cmake --build build-design && build-design/bankmaster_design_preview_artefacts/Release/bankmaster_design_preview Design/Fonts Design/mockups`

## Arquivos (`mockups/`)

| Arquivo | Conteúdo |
|---|---|
| `0_overview.png` | as 8 telas lado a lado (A em cima, B embaixo) |
| `conceptA_1_eq.png` … `conceptA_4_limiter.png` | Conceito A — OBSIDIAN / NEON, 1200×780 renderizado em 2× (Retina) |
| `conceptB_1_eq.png` … `conceptB_4_limiter.png` | Conceito B — GRAPHITE / VIOLET |
| `concept?_5_states.png` | preset modificado, UNDO/REDO, confirmação de "voltar ao preset", navegador de presets, família de controles |
| `concept?_6_brand.png` | logo, wordmark, ícone em tamanho real (256 → 16 px), janela do DMG, assistente do Setup.exe, navegador do Ableton |
| `concept?_7_min_size_1000x650_at_1x.png` | tamanho mínimo em tela 1× (pior caso de legibilidade, ex.: Windows 100 %) |
| `iconA_*.png`, `iconB_*.png` | ícone do app em 1024 / 256 / 32 / 16 px |

## Os dois conceitos

| | A — OBSIDIAN / NEON | B — GRAPHITE / VIOLET |
|---|---|---|
| Base | preto obsidiana, grafite, cinza metálico | preto, cinza chumbo, profundidade sutil |
| Acento | verde-lima controlado `#A6F23A` | violeta luminoso `#A47CFF` |
| Parentesco | mesma família do KaloxSooth (lima) | identidade própria, mais "estúdio noturno" |

Nos dois, cada módulo tem uma cor de assinatura usada só em marcadores, traços do display e no nó de
navegação: EQ (acento), SATURATION (âmbar), COMPRESSOR (turquesa), LIMITER (coral). Os knobs usam a cor
do módulo (no EQ, a cor da banda selecionada; no LIMITER, o acento da marca, porque o coral fica reservado
para a redução de ganho); o restante da interface é neutro, para o áudio continuar sendo o destaque.

## Identidade

- **Símbolo:** a curva de transferência de um limiter subindo até um teto luminoso, sobre uma área
  preenchida (o sinal) e uma grade de analisador — "entrada → saída masterizada". O ponto branco marca o
  joelho. Em 16–32 px o desenho é simplificado automaticamente (sem grade, traço mais grosso).
- **Wordmark:** "BANK" branco + "MASTER" na cor de acento (Montserrat ExtraBold), "BY KALOX" sempre em
  maiúsculas, espaçado, abaixo.
- **Ícone** igual em todos os lugares: app macOS (.icns), Windows (.ico do exe e do Setup), volume do DMG,
  imagem lateral do assistente do Windows.

## Navegação e ergonomia

1. **Cabeçalho (64 px), da esquerda para a direita:** logo · navegador de presets (◀ nome ▶, salvar,
   voltar ao preset) · A | B + copiar · **↶ UNDO | REDO ↷** · ONE MASTER (liga/desliga + knob) · menu.
2. **Faixa de fluxo do sinal:** INPUT → EQ → SATURATION → COMPRESSOR → LIMITER → OUTPUT, na ordem real da
   cadeia. Cada módulo é um "nó" com liga/desliga, resumo (bandas, estilo, GR) e mini-visual ao vivo;
   clicar abre o módulo. Mostra o estado da cadeia inteira sem trocar de aba.
3. **Módulo aberto:** display grande em cima (o que você ouve), mesa de controles embaixo. Na mesa, os
   controles principais são maiores (EQ: frequência / ganho / Q; SAT: DRIVE; COMP: threshold / ratio /
   attack / release; LIMITER: GAIN e teto) e os secundários menores e agrupados por função
   (ex.: "SHAPE", "ENVELOPE & LINK"). Sidechain do compressor fica num cartão recolhível.
4. **Barra global (62 px):** INPUT (L/R) · LUFS M / S / **I** · TRUE PEAK · GR (comp e limiter) ·
   CORRELAÇÃO · OUTPUT (L/R) · LEARN (alvo −20 LUFS) · BYPASS geral. O alvo do LEARN deixa de ser um
   slider sobre o texto (problema visto no Live) e vira um valor editável no próprio botão.
5. **Valores:** nunca "−0.00" (zero é sempre "0.00"); unidades sempre visíveis.
6. Atalhos mantidos: arrastar nós do EQ, Shift = só frequência, roda = Q, duplo clique liga/desliga
   banda, duplo clique no knob = valor padrão; ⌘Z / ⇧⌘Z no macOS e Ctrl+Z / Ctrl+Y no Windows quando a
   janela do plug-in tem foco (o host pode reservar esses atalhos — os botões sempre funcionam).

## UNDO / REDO / voltar ao preset

- **↶ UNDO | REDO ↷** usa o sistema de undo que já existe e já foi testado (mesmas transações,
  `beginUndoTransaction` / `undo` / `redo`). Cada botão fica apagado quando não há o que desfazer/refazer.
- **Voltar ao preset carregado:** o nome do preset mostra "MODIFIED" quando algo mudou desde o carregamento;
  o botão ⟲ ao lado abre uma confirmação ("Revert to loaded preset?" — CANCEL / REVERT, ESC cancela).
  A reversão é registrada como **uma** etapa de undo, então pode ser desfeita.

## Fabricante "Kalox Audio" — análise de impacto (nada foi alterado ainda)

O que identifica o plug-in para o host **não é o nome do fabricante**:

| Formato | Identificador usado pelos projetos | Muda com "Kalox Audio"? |
|---|---|---|
| VST3 | Class ID gerado pelo JUCE a partir dos códigos `Klox` + `Bkms` | **Não** |
| AU | tipo / subtipo / fabricante `aufx Bkms Klox` | **Não** |
| Parâmetros / estado | IDs de parâmetros (`in_gain`, `lim_gain`, …) e formato do estado | **Não** |
| Bundle ID macOS | `com.kalox.bankmaster` | **Não** (mantido) |

Muda apenas o **texto do fabricante** (`COMPANY_NAME` "KALOX" → "Kalox Audio"): vendor do VST3
(`moduleinfo.json` / factory info), nome do AU ("Kalox Audio: BANKMASTER"), "Publisher" do Windows.
Consequências:

- **Projetos existentes do Live continuam abrindo** o BANKMASTER com os mesmos ajustes (os códigos não mudam).
- Depois de atualizar, o Live mostra o plug-in em **Kalox Audio**, ao lado do KaloxSooth, mas só depois de
  um **Rescan** (Preferências → Plug-Ins; com Option/Alt pressionado faz uma varredura completa). Até lá o
  Live pode continuar mostrando "KALOX" pelo cache dele.
- **AU no macOS:** o sistema também guarda um cache de componentes. O instalador já reinicia o
  `AudioComponentRegistrar`; se o nome antigo persistir, um logout/login resolve.
- O código de fabricante `Klox` e os IDs **não serão trocados**: trocar quebraria os projetos.
- Pastas: presets do usuário continuam em `…/KALOX/BANKMASTER/Presets` (para não perder presets já salvos).
  No Windows o instalador V2 mantém o mesmo AppId (atualiza a instalação 1.0 em vez de duplicar).
- KaloxStart e KaloxSooth não são tocados.

## Tamanho, Retina e escala do Windows

- Interface 100 % vetorial (nenhum bitmap), por isso fica nítida em qualquer escala.
- Janela redimensionável com proporção fixa 1,54 : 1 — mínimo 1000 × 650, padrão 1200 × 780,
  máximo 2000 × 1300 — e tamanhos rápidos no menu (90 / 100 / 125 / 150 %). A proporção fixa garante o
  mesmo layout em qualquer tamanho e plataforma; o tamanho escolhido é salvo.
- macOS Retina e escalas do Windows (100 / 125 / 150 / 200 %) usam o fator de escala do sistema pelo JUCE;
  na implementação vou validar com capturas reais em 1× e 2× nas duas plataformas.

## macOS + Windows — uma única base de código

A interface V2 será um único código JUCE/C++ (este design system + componentes), compilado para:
macOS VST3 / AU / Standalone universal (Apple Silicon + Intel) e Windows VST3 / Standalone x64. Mesmos
knobs, cores, gráficos, animações, fontes embutidas, navegação e controles nas duas plataformas.
O projeto só será considerado concluído com as duas versões compiladas, validadas e com instaladores novos.

## Depois da aprovação (ainda não iniciado)

1. Implementar a interface V2 no plug-in reaproveitando `V2/DesignSystem.h`, sem tocar em `Source/DSP`.
2. Conferir todas as 98 conexões de parâmetros; testar presets, automação, A/B, undo/redo e revert.
3. Teste de nulo do áudio V1 × V2 (renders idênticos bit a bit), latência e presets inalterados.
4. "Kalox Audio" (`COMPANY_NAME`), ícone (`ICON_BIG`) e valores sem "−0.00"; corrigir o LEARN.
5. Validar tamanhos e nitidez (1× / 2×), pluginval, validador Steinberg e auval nas duas plataformas.
6. Gerar o novo DMG (macOS Universal) e o novo Setup.exe (Windows x64), com testes reais de instalação e
   desinstalação.
