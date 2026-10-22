# BANKMASTER — arquitetura

```
 host ─► INPUT gain ─► MASTER EQ ─► MASTER SATURATION ─► MASTER COMPRESSOR ─► MASTER LIMITER ─► OUTPUT trim ─► host
           │  (LEARN)     0 latência     OS 1–16× FIR fase      lookahead 0–20 ms    OS 1–16× + TP           │
           │                             linear                                                          ├─ medidores: peak, dBTP,
           └─ loudness da entrada bruta (INPUT LEARN)                                                    │   LUFS M/S/I, correlação
 MASTER BYPASS = entrada atrasada pela latência total (null test bit-exato)  ◄──────────────────────────┘
```

Tudo roda numa única instância (`bm::MasterChain`). O código DSP (`Source/DSP`) não depende do JUCE:
é C++17 puro, header-only, testável e portável (macOS arm64/x86_64, Windows x64, Linux).
O JUCE é usado apenas como invólucro do plugin (VST3/AU/Standalone), parâmetros e interface.

## Módulos (implementações próprias)

| Módulo | Técnica |
|---|---|
| EQ | 5 bandas de filtros SVF trapezoidais (topologia publicada por A. Simper/Cytomic), estáveis sob modulação. Bell, Low/High Shelf (1ª ordem a 48 dB/oct por cascata), Low/High Cut (Butterworth 6–48 dB/oct; Q = 1 ⇒ Butterworth, Q > 1 adiciona ressonância), Notch, Band Pass. Por banda: Stereo / Mid / Side / L / R, bypass com crossfade em curva S, modo dinâmico (detector passa-faixa na mesma frequência, ataque 3 ms / release 80 ms, ganho = estático + range·(1 − e^(−excesso/6 dB))). Resposta analítica exata (bilinear pré-distorcida) usada pelo gráfico e pelos testes. |
| Saturação | Curvas próprias: Clean Tube (sigmóide algébrica com polarização pequena → 2ª/3ª harmônicas suaves), Warm Tube (tanh polarizada), Tape (tanh simétrica), Soft Clip (cúbica). Drive 0–100 % = −12…+24 dB antes da curva com compensação 1/drive na saída. Dynamics (drive dependente do envelope), Feedback (passa-faixa realimentado, ganho de malha < 1), Tone (tilt ±6 dB em 1 kHz), Mix com caminho seco alinhado à latência, Output, bloqueador de DC. Oversampling 1–16× em cascata de meias-bandas FIR fase linear (Kaiser, 85/110 dB). |
| Compressor | Feed-forward, domínio log, detector de pico com release de 10 ms (sem ripple em tons), knee quadrático, balística "smooth branching" (ataque/release reais medidos = valores do painel), hold, range, auto-release dependente de programa, auto-gain, lookahead, dry/wet, sidechain HP/LP, stereo link. Estilos Clean (detector de pico), Glue (RMS) e Punch (topologia feedback). |
| Limiter | Drive → oversampling 1–16× → requisito de ganho por amostra → estágio LENTO (Attack/Release do painel, absorve excessos sustentados) → estágio RÁPIDO sobre o resíduo: mínimo deslizante na janela de lookahead + release exponencial + média móvel do mesmo comprimento (garante ganho ≤ requisito na amostra atrasada, sem overshoot) → downsampling → filtro de guarda FIR fase linear (0,44·fs) → estágio de segurança TRUE PEAK a taxa base (estimador 8× 64 taps com refinamento parabólico, hold estendido ±4 amostras em torno do pico) → clamp final. Link de transientes e release, estilos Transparent / Punchy / Dynamic / Safe, dither TPDF opcional. Oversampling limitado a 768 kHz internos (96 kHz → 8×). |
| Medição | BS.1770-4 / EBU R128: K-weighting para qualquer taxa (coeficientes conferidos com a tabela de 48 kHz), Momentary 400 ms, Short-term 3 s, Integrated com gate absoluto −70 LUFS e relativo −10 LU (histograma fixo, sem alocação). True peak 8× com refinamento parabólico (±0,003 dB até 21 kHz). Correlação estéreo (300 ms). |
| ONE MASTER | Macro 0–200 % (100 % = como ajustado). Escala ganhos/ranges do EQ, drive da saturação, inclinação do ratio do compressor e drive do limiter (no máximo +6 dB acima do ajuste, teto absoluto +24 dB). Não toca no teto nem no trim de saída. Desligado ⇒ edição manual dos quatro módulos. |
| INPUT LEARN | Mede a loudness integrada da entrada bruta e ajusta INPUT = alvo − medido (alvo padrão −20 LUFS). Torna o preset independente do nível da mix. |

## Tempo real

- Nenhuma alocação, lock ou I/O no `processBlock`: todos os buffers são dimensionados em `prepare()`.
  Blocos do host maiores que o preparado são divididos internamente.
- Parâmetros lidos uma vez por bloco via ponteiros atômicos (`ParamCache`), suavizados por amostra
  (rampas lineares; crossfades em curva S).
- Mudanças estruturais (oversampling, lookahead, modo true peak) são aplicadas atrás de um fade-out de 12 ms,
  silêncio durante a nova latência e fade-in — sem cliques. A nova latência é reportada ao host pela thread
  de mensagens (`setLatencySamples`).
- Bypass de módulo e MASTER BYPASS processam sempre o módulo (estado coerente) e fazem crossfade com a
  entrada atrasada pela latência — latência constante para a DAW.
- Entradas NaN/Inf são zeradas antes de qualquer filtro; denormais desligados (FTZ/DAZ em x86, FZ em ARM).

## Latência (48 kHz, preset KALOX MASTER - ORIGINAL)

357 amostras (7,44 ms) = saturação 4× (87) + limiter 16× com lookahead 1,004 ms, guarda e segurança TP (270).
EQ e compressor (lookahead off) não adicionam latência. A latência é reportada e compensada pela DAW.

## Estrutura de arquivos

```
BANKMASTER/
  CMakeLists.txt                 VST3 / AU / Standalone, testes, ferramentas (JUCE 8.0.4 via FetchContent)
  Source/DSP/                    núcleo DSP independente de framework
    Common.h  Svf.h  MasterEQ.h  Oversampler.h  Saturation.h  Compressor.h
    TruePeak.h  Limiter.h  Meters.h  MasterChain.h  FactoryPresets.h
  Source/Plugin/                 invólucro JUCE
    Parameters.*  PresetManager.*  PluginProcessor.*  PluginEditor.*
    UI/  Theme.h  LookAndFeel.*  Widgets.h  EqGraph.*  Pages.*  MeterBar.*
  Tests/DspTests.cpp             19 testes DSP (sem JUCE)
  Tests/PluginTests.cpp          testes do plugin (estado, presets, A/B, undo, automação, UI)
  Tools/Render.cpp               render offline + comparação objetiva com a cadeia original
  scripts/build_macos.sh         build universal + testes + auval + pluginval
  installer/macos/               .pkg (VST3 / AU / Standalone selecionáveis)
  docs/                          parâmetros, arquitetura, testes, relatório final, screenshots, logs
```
