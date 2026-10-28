# Relatório de testes — VOICE STUDIO DA GALAXIA

Data: 2026-10-28. Este relatório separa o que foi **executado de fato** do que **não pôde ser testado**
no ambiente de desenvolvimento.

## Ambiente em que os testes rodaram

* Container Linux **sem GPU** (só CPU), Python 3.12.
* torch 2.8.0, transformers 5.21.0, Gradio 5.49.1, OmniVoice no commit oficial
  `08be0b4` (o mesmo que o notebook instala), WeTextProcessing 1.2.0, Chromium (Playwright).
* A rede do ambiente **bloqueia** `drive.google.com`, `colab.research.google.com`, `huggingface.co`,
  `download.pytorch.org` e `flashinfer.ai`. PyPI e GitHub estavam acessíveis.

## Inspeção do original

* **Não foi possível abrir o notebook de referência** (`colab.research.google.com/drive/1sqjOCHZ…`):
  as tentativas de acesso devolveram HTTP 403 (bloqueio de rede). Nenhum código dele foi examinado.
* A réplica foi feita a partir das **seis capturas de tela** e do **código oficial** do OmniVoice
  (`omnivoice/models/omnivoice.py`, `omnivoice_flashinfer.py`, `cli/demo.py`, `docs/generation-parameters.md`,
  `docs/OmniVoice.ipynb`), lidos integralmente.
* O que foi deduzido das capturas: tema `gr.themes.Soft` escuro (cores medidas nos pixels: fundo `#0b0f19`,
  painéis `#1f2937`, campos `#374151`, roxo `#4f46e5`, botões `#4b5563`; fonte Montserrat), e **Gradio 5.x**:
  o texto `>1 faster speech` aparece na referência como citação markdown (barra à esquerda, `>` sumido) — o
  Gradio 5.49.1 reproduz isso exatamente, o Gradio 6.30 não. Todos os rótulos, faixas, valores padrão e textos
  visíveis foram copiados das capturas.
* **Elementos que não aparecem nas capturas e que eu tive de definir** (não são cópia do original):
  o painel **Voice Design** (atributos do demo oficial: gênero, idade, tom, estilo, sotaque inglês, dialeto chinês
  + campo livre), o estado inicial do status (“Ready”), o texto “Voice not cached”, a transcrição automática com
  Whisper quando o campo de transcrição fica vazio, a persistência das configurações no Drive e a política do
  FlashInfer (detalhada abaixo).

## Testes executados (todos passaram na rodada final)

Como os pesos reais não podem ser baixados aqui, os testes usam um **checkpoint OmniVoice minúsculo com pesos
aleatórios** (`tests/make_tiny_model.py`), construído com as **classes oficiais** (`OmniVoice`, Qwen3,
`HiggsAudioV2TokenizerModel`, 25 frames/s, 8 codebooks). Assim roda o caminho de código real de ponta a ponta
(codificação da referência, decodificação iterativa, chunking nativo, decodificação para 24 kHz,
pós-processamento, callbacks Gradio). **O áudio gerado é ruído**: isso testa a integração, não a qualidade da voz.

### 1. Motor — `python tests/test_engine.py <modelo>` → 14/14

| Verificação | Resultado |
|---|---|
| Auto Voice gera áudio (1 chamada interna) | ok |
| Voice Design com `instruct` montado a partir dos menus | ok |
| Voice Clone: 1ª geração codifica a referência; 2ª é *cache hit* | ok |
| Narração longa: 20 chamadas do decodificador = estimativa, 299,3 s de áudio | ok |
| Mesma seed → áudio idêntico | ok |
| WAV salvo em 24 kHz, PCM 16-bit, mono | ok |
| Cache da voz persiste em disco e é reutilizado após recriar o motor | ok |
| Clear Voice Cache remove memória + disco | ok |
| Erros amigáveis: Voice Design inválido, clonagem sem referência, texto vazio | ok |

### 2. Interface no navegador — `node tests/e2e_ui.js <porta> <ref.wav>` → 18/18

Tema escuro forçado · contador de caracteres/palavras · **Insert Tag** na posição do cursor · troca de modo
mostra/esconde os painéis · geração Auto Voice · upload da referência + indicador “not cached” ·
**Prepare / Cache Voice** (“Reference encoded in X s”) · geração com *cache hit* · **player toca** (tempo avança
0:00 → 0:02) · **Download WAV** baixa `omnivoice_turbo_<hash>.wav` válido · geração Voice Design ·
narração longa pela interface (14 chamadas internas, 193,7 s) · **Clear GPU / Graph Caches** ·
**Clear Voice Cache** · **Clear Text / Output** · texto vazio rejeitado com mensagem.

### 3. Notebook executado de verdade — `python tests/run_notebook.py …` (nbclient)

O `.ipynb` é executado célula por célula (fora do Colab, com `MODEL_ID` apontando para o modelo de teste):
instala de fato o OmniVoice do GitHub no commit fixado + Gradio 5.49.1 + WeTextProcessing com o torch travado,
grava o código com `%%writefile`, carrega o modelo, faz o aquecimento, inicia o Gradio e roda o teste e2e acima
contra o servidor que o notebook abriu.

| Cenário | Resultado |
|---|---|
| A — kernel novo, todas as células, e **todas de novo no mesmo kernel** (reexecução na mesma sessão) | 36/36 (2 × 18), instalação e modelo reaproveitados |
| B — **kernel novo na mesma pasta persistida** (simula nova sessão com o Drive) | 18/18, configurações restauradas |

### 4. Fidelidade visual

Capturas com Playwright na mesma janela da referência (1470×758 CSS px, escala 2×), medidas e comparadas
lado a lado: `docs/screenshots/comparacao_1..6.jpg`. Posições/tamanhos do cabeçalho, colunas (7:5),
painéis, botões e textos ficaram a poucos pixels da referência. Diferenças restantes: o selo e o rodapé mostram
`CPU · OmniVoice FP32 — baseline` porque o teste rodou sem GPU (na T4 mostrará
`Tesla T4 · 14.6 GB · OmniVoice FP16 — T4-safe baseline`); o rodapé do Gradio aparece no idioma do navegador.

### Erros encontrados pelos testes e corrigidos

1. A trava do torch usava `torch.__version__` (`2.8.0+cu128`), que pode não bater com a versão do pacote →
   o pip falhava. Agora usa a versão da distribuição.
2. Reexecutar as células na mesma sessão reimportava o código, mas o motor reaproveitado era da versão
   antiga (erros de validação viravam “Generation failed”). Agora o motor novo assume o modelo já carregado
   (`StudioEngine.adopt`) e a instrumentação é idempotente.
3. Tamanho de fonte 21% maior que a referência, número das sliders mais largo, contagem dupla no
   “Clear Voice Cache” e layout da linha “expression tag / Insert Tag” — corrigidos.

## NÃO testado (precisa ser verificado no Colab)

* **Pesos reais `k2-fsa/OmniVoice`**, download para o Drive e qualidade real da clonagem/voz.
* **GPU (Tesla T4)**: FP16, tempos, RTF e VRAM reais. Nenhum número de desempenho deste relatório vale para a T4.
* **FlashInfer** (instalação e autoteste em GPU sm_80+); na T4 o modo `auto` não o liga.
* **Transcrição automática com Whisper** (download do `openai/whisper-large-v3-turbo`).
* Funções exclusivas do Colab: `drive.mount`, link `proxyPort` do Colab e link público `gradio.live`.
* Versões do torch/CUDA que o Colab estiver usando no dia (o notebook não troca o torch do Colab; testado
  aqui só com torch 2.8.0).

## Como repetir

```bash
python tests/make_tiny_model.py /tmp/tiny
python tests/test_engine.py /tmp/tiny /tmp/vs_work
python tests/run_ui_local.py --model /tmp/tiny --data /tmp/vs_data --port 7861 &
node tests/e2e_ui.js 7861 /tmp/vs_work/ref.wav
NB_KERNEL=python3 python tests/run_notebook.py /tmp/nbwork /tmp/tiny /tmp/vs_work/ref.wav --rerun
```
