# BANKMASTER — parâmetros extraídos dos prints

Fonte: 15 capturas de tela (22/10/2026, 11:20–11:24) de FabFilter Pro-Q 4, Saturn 2,
Pro-C 2 e Pro-L 2. Legenda:

- **CONFIRMADO** — valor numérico legível no print (tooltip ou campo).
- **ESTIMADO** — deduzido de posição de knob, desenho de curva ou destaque visual. Precisa de confirmação.
- **PENDENTE** — não visível; foi usado um valor neutro/plausível.

Todos estão no preset **KALOX MASTER - ORIGINAL** (`Source/DSP/FactoryPresets.h`).

## Módulo 1 — MASTER EQ (Pro-Q 4)

| Banda | Parâmetro | Valor | Status | Evidência |
|---|---|---|---|---|
| 1 Low Cut | Freq / Q / Slope | 25 Hz / 1.000 / 24 dB/oct | CONFIRMADO | tooltip |
| 2 Bell | Freq / Gain / Q | 250 Hz / −1.00 dB / 1.000 | CONFIRMADO | tooltip |
| 3 Bell | Freq / Gain / Q | 861.41 Hz / 0.00 dB / 1.622 | CONFIRMADO | tooltip |
| 3 Bell | Dinâmico | ligado | CONFIRMADO | setas ▲▼ sob o nó e área sombreada |
| 3 Bell | Faixa dinâmica (range) | −1.0 dB | ESTIMADO | sombreado ≈ 28 px numa escala de 80 px / 3 dB; marca vermelha curta no anel do GAIN |
| 3 Bell | Threshold dinâmico | −30 dB | PENDENTE | não exibido (Pro-Q pode estar em "auto") |
| 4 Bell | Freq / Gain / Q | 1559.3 Hz / 0.00 dB / 1.622 | CONFIRMADO | tooltip |
| 4 Bell | Dinâmico / range / threshold | ligado / −1.0 dB / −30 dB | CONF. / ESTIM. / PEND. | idem banda 3 |
| 5 High Shelf | Freq / Gain / Q / Slope | 10 kHz / +1.00 dB / 1.000 / 12 dB/oct | CONFIRMADO | tooltip + painel |
| — | Processing | Zero Latency | CONFIRMADO | barra inferior |
| — | Stereo placement | Stereo (todas) | CONFIRMADO (bandas 3, 4, 5) / ESTIMADO (1, 2) | painel da banda |

Observação: no Pro-Q a numeração é 1=LowCut, 2=250 Hz, 3=High Shelf, 4=861 Hz, 5=1559 Hz.
No BANKMASTER as bandas são numeradas por frequência (a ordem não altera o som, as bandas são lineares).

## Módulo 2 — MASTER SATURATION (Saturn 2)

| Parâmetro | Valor | Status | Evidência |
|---|---|---|---|
| Style | Clean Tube | CONFIRMADO | botão de estilo |
| Drive | 20.0 % | CONFIRMADO | tooltip "Band 1 Drive 20.0%" |
| Linear Phase | ligado | CONFIRMADO | barra inferior (sem crossover em 1 banda; filtros de oversampling do BANKMASTER são sempre fase linear) |
| Quality | High Quality → 4× oversampling | CONFIRMADO / mapeamento próprio | barra inferior |
| Mix | ≈ 30 % | ESTIMADO | ponteiro ≈ 10 h; a escala do knob (7 h → 5 h) foi calibrada pelo Drive (20 % = 9 h) |
| Feedback | ≈ 0 % | ESTIMADO | ponteiro no mínimo |
| Feedback Freq | ≈ 2 kHz | ESTIMADO | ponteiro ≈ 14 h (escala log) |
| Dynamics | 0 | ESTIMADO | ponteiro ao centro |
| Tone (4 sliders) | 0 dB | ESTIMADO | sliders centralizados |
| Level / Pan | 0 dB / centro | ESTIMADO | ponteiro ao centro |
| Output global | −1.0 dB | ESTIMADO | barra inferior "100 %  0.0 dB  −1.0 dB" |

## Módulo 3 — MASTER COMPRESSOR (Pro-C 2)

| Parâmetro | Valor | Status | Evidência |
|---|---|---|---|
| Style | Clean | CONFIRMADO | |
| Threshold | −18.00 dB | CONFIRMADO | tooltip |
| Ratio | 2.00:1 | CONFIRMADO | tooltip |
| Attack | 30.00 ms | CONFIRMADO | tooltip |
| Release | 100.0 ms | CONFIRMADO | tooltip |
| Lookahead | Off (0 ms) | CONFIRMADO | barra inferior + slider |
| Oversampling | Off | CONFIRMADO | barra inferior |
| Knee | ≈ 20 dB (soft) | ESTIMADO | ajuste da curva de transferência desenhada no print (GR ≈ 1,25 dB no threshold → W/16 ⇒ W ≈ 20 dB; início da curvatura ≈ −28 dB) |
| Range | 60 dB (máximo) | ESTIMADO | slider no fim |
| Hold | 0 ms | ESTIMADO | slider no início |
| Auto Gain | **ligado** | ESTIMADO | rótulo AUTO GAIN em destaque amarelo |
| Auto Release | desligado | ESTIMADO | "AUTO" acinzentado |
| Dry | OFF (100 % wet) | ESTIMADO | knob em "M OFF" |
| Output | 0 dB | ESTIMADO | ponteiro ao centro / barra inferior 0.0 dB |
| Stereo link / sidechain | 100 % / sem filtro | PENDENTE | painel SIDE CHAIN fechado |

A fórmula exata do Auto Gain do Pro-C 2 não é pública. O BANKMASTER aplica metade da redução
estática que um sinal a 0 dBFS receberia (+4,5 dB para −18 dB / 2:1 / knee 20).

## Módulo 4 — MASTER LIMITER (Pro-L 2)

| Parâmetro | Valor | Status |
|---|---|---|
| Style | Transparent | CONFIRMADO |
| Input Gain | +11.0 dB (ajustável −12…+30 dB) | CONFIRMADO |
| Attack | 98.21 ms | CONFIRMADO |
| Release | 400.0 ms | CONFIRMADO |
| Lookahead | 1.004 ms | CONFIRMADO |
| True Peak Limiting | On | CONFIRMADO |
| Oversampling | 16× | CONFIRMADO |
| Dither | Off | CONFIRMADO |
| Output ceiling | −0.1 dBTP | CONFIRMADO |
| Channel linking Transients / Release | ≈ 0 % / ≈ 0 % | ESTIMADO (ponteiros perto do mínimo) |
| Loudness target exibido | −14 LUFS (Short Term) | CONFIRMADO (apenas visualização) |

## O que confirmar (para fechar a reprodução)

1. EQ bandas 861 Hz e 1559 Hz: abrir o painel dinâmico e anotar **threshold** e **range** exatos (ou se o threshold está em "Auto").
2. Saturn: passar o mouse em **Mix**, **Feedback**, **Freq** e no valor de saída da barra inferior.
3. Pro-C 2: valor do **Knee**, confirmar **Auto Gain** ligado, abrir **SIDE CHAIN** (stereo link / filtros).
4. Pro-L 2: valores de **Channel Linking** (Transients / Release).
5. Renderizar o mesmo trecho pela cadeia FabFilter e pelo BANKMASTER e rodar
   `bankmaster_render --compare referencia.wav bankmaster.wav` (veja `docs/TEST_REPORT.md`, item 15).
