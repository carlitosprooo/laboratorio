# BANKMASTER by KALOX V1.0 — relatório final (FASE 8)

Data: 22/10/2026 · Branch: `claude/quirky-curie-aoazly` · Repositório: `carlitosprooo/laboratorio`

## 1. Entregáveis

| Item | Onde |
|---|---|
| Instalador macOS universal (VST3 + AU + Standalone) | artefato **BANKMASTER-macOS-universal** do GitHub Actions → `BANKMASTER-by-KALOX-1.0.0-macOS-universal.pkg` (gerado em `BANKMASTER/dist/` pelo `installer/macos/build_pkg.sh`) |
| Bundles avulsos macOS | mesmo artefato: `BANKMASTER-VST3-macOS-universal.zip`, `BANKMASTER-AU-macOS-universal.zip`, `BANKMASTER-Standalone-macOS-universal.zip` |
| VST3 Windows x64 | artefato **BANKMASTER-windows-x64-VST3** |
| VST3 Linux x86_64 | artefato **BANKMASTER-linux** |
| Logs de validação macOS (auval, pluginval, Steinberg, testes, snapshots) | artefato **BANKMASTER-macOS-validation-logs** |
| Código-fonte, testes, ferramentas, documentação | `BANKMASTER/` (ver `docs/ARCHITECTURE.md`) |

## 2. Resultado do CI (GitHub Actions, run #4 — commit `107dc8c`)

Run: https://github.com/carlitosprooo/laboratorio/actions/runs/39817743841 — os três jobs **concluídos com sucesso**,
com `bash -eo pipefail` (nenhuma falha pode ficar escondida). Evidências conferidas nos logs, não só no status das etapas.

### macOS universal (runner macos-latest, Apple Silicon) — APROVADO

| Verificação | Resultado no log |
|---|---|
| Arquiteturas (`lipo -info`) | VST3, AU, Standalone e testes: `x86_64 arm64` |
| Testes DSP arm64 (nativo) | 19 testes, 133 verificações, 0 falhas |
| Testes DSP x86_64 (Rosetta 2) | 19 testes, 133 verificações, 0 falhas |
| Testes do plugin (estado, presets, A/B, undo, automação, snapshots) | 78 verificações, 0 falhas |
| `auval -strict -v aufx Bkms Klox` arm64 | AU VALIDATION SUCCEEDED |
| `arch -x86_64 auval -strict ...` | AU VALIDATION SUCCEEDED |
| pluginval nível 10 — VST3 | SUCCESS |
| pluginval nível 10 — AU | SUCCESS (1 aviso não-bloqueante, ver seção 6) |
| Validador VST3 Steinberg (SDK 3.7.12, compilado universal) arm64 | 47 aprovados, 0 falhas |
| Validador VST3 Steinberg x86_64 | 47 aprovados, 0 falhas |
| Instalador | `BANKMASTER-by-KALOX-1.0.0-macOS-universal.pkg`, 10,7 MB |
| Conteúdo do `.pkg` (`pkgutil --expand` + payloads) | `BANKMASTER-VST3.pkg` → `BANKMASTER.vst3/Contents/MacOS/BANKMASTER`; `BANKMASTER-AU.pkg` → `BANKMASTER.component/Contents/MacOS/BANKMASTER` (+ postinstall que recarrega o cache AU); `BANKMASTER-App.pkg` → `BANKMASTER.app` |

### Windows x64 (windows-latest) — APROVADO (VST3)

Build MSVC OK · testes DSP 19 / 133 verificações / 0 falhas (CPU 10,75 % de um núcleo @ 48 kHz, latência 357 amostras) ·
testes do plugin 0 falhas · pluginval nível 10 SUCCESS · validador Steinberg 47 aprovados / 0 falhas.
Sem instalador Windows nesta versão (só o bundle VST3).

### Linux x86_64 (ubuntu-24.04) — APROVADO (VST3)

Build OK · testes DSP e de plugin sem falhas · pluginval nível 10 SUCCESS · validador Steinberg sem falhas.

### Artefatos (run #4, expiram em 20/01/2027)

| Artefato | Conteúdo | Tamanho |
|---|---|---|
| `BANKMASTER-macOS-universal` | `BANKMASTER-by-KALOX-1.0.0-macOS-universal.pkg` + zips VST3 / AU / Standalone | 21,3 MB |
| `BANKMASTER-macOS-validation-logs` | logs auval, pluginval, Steinberg, testes, `pkg_contents.txt`, snapshots da UI no macOS | 1,2 MB |
| `BANKMASTER-windows-x64-VST3` | `BANKMASTER.vst3` | 2,9 MB |
| `BANKMASTER-linux` | `BANKMASTER.vst3` + logs | 2,2 MB |

Download: página do run → seção **Artifacts** (exige login no GitHub).


## 3. Validação local (Linux x86_64, este ambiente)

| Verificação | Resultado | Evidência |
|---|---|---|
| Testes DSP | 19 testes, 133 verificações, 0 falhas | `docs/validation/dsp_tests_linux_x86_64.txt` |
| Testes do plugin | 0 falhas (78 verificações com snapshots) | `docs/validation/plugin_tests_linux.txt` |
| pluginval nível 10 (VST3) | SUCCESS | `docs/validation/pluginval_linux_vst3_strictness10.txt` |
| Validador VST3 Steinberg (SDK 3.7.12) | 47 aprovados, 0 falhas | `docs/validation/steinberg_vst3_validator_linux.txt` |
| Dependências do binário | apenas bibliotecas do sistema; 0 referências a FabFilter | `ldd` / `strings` |

## 4. Verificações pedidas

| Pedido | Resultado | Evidência |
|---|---|---|
| Funciona sem Pro-Q / Saturn / Pro-C / Pro-L | Sim: DSP 100 % próprio, nenhuma dependência; validado em runners limpos (sem FabFilter) | auval / pluginval / Steinberg no CI; `ldd`, `strings` |
| Preset KALOX MASTER - ORIGINAL preservado | Valores confirmados intactos; teste `defaults_are_kalox_master_original` verifica cada valor dos prints | `Tests/PluginTests.cpp`, `docs/PARAMETERS.md` |
| Latência | 357 amostras = 7,44 ms @ 48 kHz (preset KALOX); reportada = medida por impulso; reportada de novo ao host ao mudar lookahead | `chain_latency_reported_matches_measured`, `processes_real_audio_reports_latency` |
| CPU | Preset KALOX (sat 4×, limiter 16×), 1 núcleo do servidor Linux: 12,6 % @ 44,1 k, 15,2 % @ 48 k, 22,5 % @ 96 k | `kalox_preset_processes_real_audio_and_cpu_budget` |
| Oversampling | Latência inteira exata, passband ±0,01 dB até 19 kHz, rejeição −118 dB; aliasing −12,9 → −80,2 dB (1× → 16×) | `oversampler_*`, `saturation_harmonics_and_aliasing` |
| True peak | ≤ −0,1 dBTP + 0,05 dB contra referência ideal em todas as configurações TP (−0,095 dBTP no preset); medidor ±0,003 dB | `limiter_*`, `truepeak_meter_accuracy` |
| Automação | Todos os parâmetros aleatorizados a cada 128 amostras e de outra thread: sem NaN, sem exceder teto; pluginval "Automation", "Parameter thread safety", "Fuzz parameters" | testes + logs pluginval |
| Recuperação de presets | 98 parâmetros, 0 divergências; 7 presets exatos; preset de usuário salva/carrega/apaga; A/B; Undo/Redo | `state_save_and_restore_roundtrip`, `factory_presets_load_exact_values`, `user_preset_save_load_delete`, `ab_compare_and_undo_redo`, pluginval "Plugin state restoration" |

## 5. Problemas encontrados e corrigidos durante a validação

1. **Restauração de parâmetros booleanos** (pluginval): `APVTS::replaceState` não atualizava um bool
   cujo valor quantizado não mudava. Corrigido com sincronização explícita (`replaceStateAndSync`).
2. **Agrupamento de undo** (CI macOS): duas edições rápidas podiam cair no mesmo passo de undo
   (timer adaptativo do APVTS). Corrigido: o estado é gravado antes de cada transação e de undo/redo.
3. **Falha mascarada no CI**: `| tee` sem `pipefail` deixava uma etapa verde com teste falhando.
   Corrigido (`bash -eo pipefail`); o run final é o primeiro com essa proteção.
4. **Validador Steinberg não executado**: o pluginval o pula sem `--vst3validator`. Agora ele é compilado
   do VST3 SDK e executado em todas as plataformas (macOS nas duas arquiteturas).
5. Runner `macos-14` nunca foi atribuído (dois jobs ficaram 15 min na fila e foram cancelados);
   o job passou a usar `macos-latest`.

## 6. Limitações e pendências

- **Não instalado no seu Mac** (sem autorização e sem acesso a ele). Instalação e teste no Ableton Live 12
  são o próximo passo, feitos por você — ver seção 7.
- **Assinatura**: o `.pkg` sai com assinatura ad-hoc (sem Developer ID). O macOS vai bloquear na primeira
  abertura: clique com o botão direito no `.pkg` → Abrir (ou Ajustes do Sistema → Privacidade e Segurança →
  Abrir mesmo assim). Para distribuição: definir `BM_SIGN_APP`, `BM_SIGN_INSTALLER`, `BM_NOTARY_PROFILE`.
- **Comparação auditiva/medida com a sua cadeia FabFilter** (teste 15): pendente — requer renders seus
  (`bankmaster_render --compare`). Nenhuma equivalência sonora com FabFilter é declarada.
- **Parâmetros estimados** a confirmar: threshold/range das bandas dinâmicas (861/1559 Hz), Mix/Feedback/
  Freq/Output do Saturn, Knee e Auto Gain do Pro-C, Channel Linking do Pro-L (`docs/PARAMETERS.md`).
- CPU medido apenas no servidor Linux do build; medir no seu Mac dentro do Live.
- pluginval (AU) emite um aviso não-bloqueante "Current program is -1" (o wrapper AU só informa o preset
  ativo ao host depois que um preset é escolhido pelo host); o teste de programas passa.
- O modo True Peak inclui um filtro de guarda acima de 0,44·fs (21,1 kHz @ 48 kHz).

## 7. Como instalar e testar no Ableton Live 12

1. GitHub → Actions → run mais recente "BANKMASTER build, test & validate" → Artifacts →
   baixe **BANKMASTER-macOS-universal** e descompacte.
2. Abra `BANKMASTER-by-KALOX-1.0.0-macOS-universal.pkg` (botão direito → Abrir), escolha **Customize** e
   marque VST3 e/ou Audio Unit. Só são gravados `BANKMASTER.vst3`, `BANKMASTER.component` e
   (opcional) `BANKMASTER.app` — KaloxStart e KaloxSooth não são tocados.
3. Live → Settings → Plug-Ins → ative VST3 / AU → **Rescan**.
4. Coloque BANKMASTER na Master → preset KALOX MASTER - ORIGINAL → LEARN no trecho mais alto → ajuste.
5. Para o teste 15: exporte a mix, a versão FabFilter e a versão BANKMASTER e compare com
   `bankmaster_render --compare`.
