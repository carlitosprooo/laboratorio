# BANKMASTER — relatório de testes

Logs completos: `docs/validation/`. Testes DSP: `Tests/DspTests.cpp` (19 testes, 133 verificações).
Testes do plugin: `Tests/PluginTests.cpp` (10 testes, 78 verificações). Validação de formato: pluginval
(nível de rigor 10, inclui o validador VST3 da Steinberg) e auval (CI macOS).

Ambiente local: Linux x86_64, GCC 13.3, JUCE 8.0.4, build Release. Os sinais de teste são sintéticos
(kick + baixo + hats filtrados + tons com picos inter-amostra) ou tons de referência EBU.

| # | Exigência | Teste | Resultado medido |
|---|---|---|---|
| 1 | Processamento de áudio real | `kalox_preset_processes_real_audio_and_cpu_budget`, `processes_real_audio_reports_latency`, `bankmaster_render` com WAV | Cadeia completa a 44,1/48/96 kHz; WAV 20 s: −25,3 → −11,6 LUFS-I, true peak dentro do teto |
| 2 | Estabilidade dos filtros | `eq_stability_under_extreme_modulation` | 10 s de ruído com TODAS as bandas aleatorizadas a cada 64 amostras (±24 dB, Q 0,025–40, 10 Hz–24 kHz): sem NaN/Inf, resíduo 1e-44 após 2 s de silêncio; 63/63 respostas ao impulso decaem |
| 3 | Resposta em frequência | `svf_sections_match_analytic_response`, `eq_kalox_frequency_response` | Desvio medido × analítico: 0,0000 dB (11 tipos × 3 fc × 5 f). Preset KALOX: 25 Hz −3,02 dB (Butterworth 4ª ordem), 12,5 Hz ≈ −24 dB, 250 Hz −1,00 dB, 10 kHz +0,50 dB, 16 kHz +0,96 dB. Banda dinâmica: 0,00 dB a −60 dBFS, −0,98 dB a −6 dBFS |
| 4 | Comportamento da compressão | `compressor_static_curve_matches_design`, `compressor_attack_release_timing` | Curva estática −18 dB / 2:1 / knee 20: erro ≤ 0,02 dB (−40…0 dBFS). Ataque medido 30,0 ms (ajuste 30), release 100,8 ms (ajuste 100) |
| 5 | Distorção harmônica e aliasing | `saturation_harmonics_and_aliasing` | Clean Tube 20 % @ −6 dBFS: THD −31,9 dB. Aliasing (soft clip 90 %, 9 kHz): 1× −12,9 dB → 4× −48,1 dB → 16× −80,2 dB. Preset KALOX (4×): pior alias −97,5 dB |
| 6 | True peak e oversampling | `truepeak_meter_accuracy`, `limiter_respects_true_peak_ceiling`, `limiter_true_peak_stress_near_nyquist`, `oversampler_*` | Medidor: ±0,003 dB até 19 kHz. Limiter, teto −0,1 dBTP medido por referência IDEAL (interpolação FFT 16×): −0,095 (preset), −0,107 (LA 0, +18 dB), −0,096 (4×, LA 5 ms), −0,095 (1×); caso de estresse com energia até Nyquist: −0,118. Oversampler: latência inteira exata, passband ±0,01 dB até 19 kHz, rejeição de imagem −118 dB |
| 7 | Latência e compensação | `chain_latency_reported_matches_measured`, `processes_real_audio_reports_latency` | Latência reportada = medida por impulso (597 amostras na configuração máxima testada; 357 = 7,44 ms no preset KALOX @ 48 kHz). MASTER BYPASS: null test bit-exato (resíduo 0). Bypass de cada módulo mantém alinhamento. Mudança de lookahead reportada ao host (453 amostras) |
| 8 | Automação | `automation_random_parameters_stays_finite_and_under_ceiling`, `automation_from_another_thread_while_processing`, pluginval "Automation"/"Parameter thread safety"/"Fuzz parameters" | Todos os parâmetros aleatorizados a cada 128 amostras por 8 s: sem NaN, pico de amostra −0,126 dBFS. Automação concorrente de outra thread: OK |
| 9 | Salvar/recuperar presets | `state_save_and_restore_roundtrip`, `factory_presets_load_exact_values`, `user_preset_save_load_delete`, `ab_compare_and_undo_redo`, pluginval "Plugin state restoration" | 98 parâmetros, 0 divergências após restaurar; 7 presets de fábrica carregam valores exatos; preset de usuário salva/carrega/apaga; A/B e Undo/Redo corretos |
| 10 | Bypass sem cliques | `bypass_toggles_are_click_free` | Pico da 2ª diferença em relação ao estado estacionário: master 1,00×, EQ 1,05×, SAT 1,10×, COMP 1,00×, LIM 1,00×; troca de oversampling/lookahead 1,21× (antes do declick: 2900×) |
| 11 | Uso de CPU | `kalox_preset_processes_real_audio_and_cpu_budget` | Preset KALOX (sat 4× + limiter 16×), 1 núcleo do servidor de build: 44,1 kHz 12,6 %, 48 kHz 15,2 %, 96 kHz 22,5 % (limiter 8×). Em Apple Silicon espera-se menos; medir no Mac é pendência (ver abaixo) |
| 12 | Intel e Apple Silicon | CI macOS: `lipo -info`, testes DSP nativos (arm64) e sob Rosetta (x86_64), auval nas duas arquiteturas | ver seção "CI macOS" |
| 13 | Validação VST3 e AU | pluginval nível 10 (Linux VST3: SUCCESS), CI macOS: pluginval VST3 + AU e `auval -strict` | ver seção "CI macOS" |
| 14 | Ausência de crashes e NaN/Inf | `nan_inf_and_extreme_inputs` | Silêncio, DC 0 dBFS, quadrada em Nyquist, NaN, Inf, +18 dBFS: saída finita e ≤ −0,1 dBFS; cadeia se recupera |
| 15 | Comparação com a cadeia original | `bankmaster_render --compare` | **PENDENTE** — exige os renders da sua cadeia FabFilter (não disponível neste ambiente). Procedimento abaixo |

Testes adicionais: LUFS (EBU Tech 3341: 1 kHz −23 dBFS ⇒ −22,99 LUFS M/S/I; gating −36/−23/−36 ⇒ −23,01 LUFS;
coeficientes K-weighting = tabela BS.1770 @ 48 kHz), ONE MASTER (monotônico 0 → 200 %: −26,9 → −6,8 LUFS,
true peak ≤ teto em todos os pontos; 100 % = ajuste exato), INPUT LEARN (−31 LUFS medido ⇒ INPUT +10,99 dB),
layouts mono/estéreo aceitos e 5.1 rejeitado, editor criado e capturado (`docs/screenshots/`).

## Item 15 — como comparar com a sua cadeia

1. Exporte um trecho da mix **sem** processamento na master (`mix.wav`).
2. Exporte o mesmo trecho com Pro-Q 4 → Saturn 2 → Pro-C 2 → Pro-L 2 (`referencia.wav`).
3. `bankmaster_render --in mix.wav --out bankmaster.wav --preset "KALOX MASTER - ORIGINAL"`
4. `bankmaster_render --compare referencia.wav bankmaster.wav`

O relatório mostra diferença de LUFS integrado e short-term máximo, true peak, balanço por 1/3 de oitava
(nivelado) e resíduo de null test após alinhamento. Diferenças esperadas e onde ajustar:
loudness → Auto Gain/knee do compressor e drive do limiter; balanço espectral → threshold dinâmico das
bandas 861/1559 Hz e Mix da saturação (valores estimados). Não há equivalência declarada com FabFilter
enquanto essa comparação não for feita.

## Limitações conhecidas (medidas)

- True peak é garantido contra referência ideal com tolerância de +0,05 dB; o filtro de guarda do modo
  True Peak remove conteúdo acima de 0,44·fs (21,1 kHz @ 48 kHz; 19,4 kHz @ 44,1 kHz) — inaudível na prática,
  mas é uma diferença em relação a um limiter sem filtro.
- Em 96 kHz o oversampling interno é limitado a 8× (teto de 768 kHz por custo de CPU).
- O teto −0,1 dBTP é o do preset de referência; para streaming normalmente se usa −1 dBTP.
