"""Generate VOICE_STUDIO_DA_GALAXIA.ipynb from the sources in voice_studio/.

The notebook embeds the studio code with ``%%writefile`` cells, so it runs in
Google Colab without cloning this repository. Re-run after editing any file
in voice_studio/:

    python tools/build_notebook.py
"""

import json
import os
import textwrap

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = ["voice_studio/__init__.py", "voice_studio/engine.py", "voice_studio/ui.py", "voice_studio/theme.css"]

# Versions validated for this release (see TESTES.md).
OMNIVOICE_COMMIT = "08be0b4ccbac3e13e374e86fbfead4b4cac343e2"  # k2-fsa/OmniVoice master, 2026-08-24
GRADIO_VERSION = "5.49.1"
FLASHINFER_VERSION = "0.6.15.post1"


def md(text):
    return {"cell_type": "markdown", "metadata": {}, "source": textwrap.dedent(text).strip("\n")}


def code(text, form=True):
    meta = {"cellView": "form"} if form else {}
    return {
        "cell_type": "code",
        "metadata": meta,
        "execution_count": None,
        "outputs": [],
        "source": textwrap.dedent(text).strip("\n"),
    }


INTRO = """
# 🌌 VOICE STUDIO DA GALAXIA

Estúdio de voz em Gradio com o motor **oficial [OmniVoice](https://github.com/k2-fsa/OmniVoice)**
(k2-fsa): clonagem de voz, Voice Design, Auto Voice, 600+ idiomas e narração longa nativa.

**Como usar (Colab):**
1. `Ambiente de execução → Alterar o tipo de ambiente de execução → GPU T4` (ou outra GPU NVIDIA).
2. Ajuste, se quiser, a célula **0. Configurações**.
3. `Ambiente de execução → Executar tudo` (ou execute as células 0 → 6 em ordem).
4. Abra o link impresso pela célula **6** (proxy do Colab ou link público `gradio.live`).

**Persistência:** pesos do modelo, vozes preparadas (cache), WAVs gerados e as últimas configurações da
interface ficam no Google Drive, em `MyDrive/VOICE_STUDIO_DA_GALAXIA/`. Numa nova sessão basta
executar tudo de novo: nada é baixado outra vez e as vozes continuam em cache.
O que **não** sobrevive ao fim da sessão: os pacotes instalados, os processos (servidor Gradio),
a memória da GPU e o link — eles são recriados automaticamente ao executar as células.

> Use apenas a sua própria voz ou vozes com autorização expressa do titular.
"""

CONFIG = """
#@title ⚙️ 0. Configurações (edite e execute)
USE_GOOGLE_DRIVE = True  #@param {type:"boolean"}
DRIVE_FOLDER = "VOICE_STUDIO_DA_GALAXIA"  #@param {type:"string"}
MODEL_ID = "k2-fsa/OmniVoice"  #@param {type:"string"}
#@markdown `pinned` = commit oficial validado · `latest` = branch master atual · `pypi` = última versão publicada (sem FlashInfer)
OMNIVOICE_SOURCE = "pinned"  #@param ["pinned", "latest", "pypi"]
#@markdown FlashInfer: `auto` liga só em GPUs sm_80+ (A100/L4/H100) após autoteste; na T4 mantém o baseline FP16.
FLASHINFER = "auto"  #@param ["auto", "on", "off"]
FLASHINFER_CUDA_GRAPHS = False  #@param {type:"boolean"}
INSTALL_TEXT_NORMALIZATION = True  #@param {type:"boolean"}
PUBLIC_SHARE_LINK = True  #@param {type:"boolean"}
RESTORE_LAST_SETTINGS = True  #@param {type:"boolean"}
PORT = 7860  #@param {type:"integer"}

import sys
IN_COLAB = "google.colab" in sys.modules
APP_DIR = "/content/voice_studio_app" if IN_COLAB else __import__("os").path.abspath("voice_studio_app")
print("Configurações carregadas. Colab:", IN_COLAB)
"""

GPU = """
#@title 🖥️ 1. Verificar GPU e ambiente
import platform, shutil, subprocess

print("Python", sys.version.split()[0], "·", platform.platform())
if shutil.which("nvidia-smi"):
    print(subprocess.run(
        ["nvidia-smi", "--query-gpu=name,memory.total,driver_version,compute_cap", "--format=csv"],
        capture_output=True, text=True).stdout)

import torch
print("torch", torch.__version__, "· CUDA", torch.version.cuda, "· GPU disponível:", torch.cuda.is_available())
if torch.cuda.is_available():
    cc = torch.cuda.get_device_capability(0)
    free, total = torch.cuda.mem_get_info(0)
    print(f"GPU: {torch.cuda.get_device_name(0)} · {total/1024**3:.1f} GB · sm_{cc[0]}{cc[1]}")
    if cc < (8, 0) and FLASHINFER == "auto":
        print("→ FlashInfer ficará desligado (modo auto); será usado o baseline FP16 seguro para T4.")
else:
    print("⚠️ Nenhuma GPU NVIDIA encontrada. No Colab: Ambiente de execução → Alterar o tipo de "
          "ambiente de execução → GPU T4. Em CPU o estúdio funciona, mas é muito lento.")
"""

DRIVE = """
#@title 💾 2. Montar o Google Drive (persistência)
import os

if USE_GOOGLE_DRIVE and IN_COLAB:
    from google.colab import drive
    drive.mount("/content/drive")
    STUDIO_HOME = f"/content/drive/MyDrive/{DRIVE_FOLDER}"
else:
    STUDIO_HOME = "/content/voice_studio_data" if IN_COLAB else os.path.abspath("voice_studio_data")
    if USE_GOOGLE_DRIVE:
        print("(fora do Colab: usando pasta local)")

for sub in ("models", "hf_cache", "outputs", "voices"):
    os.makedirs(os.path.join(STUDIO_HOME, sub), exist_ok=True)
# Tudo que o Hugging Face baixar (ex.: Whisper para transcrição automática) também fica no Drive.
os.environ["HF_HOME"] = os.path.join(STUDIO_HOME, "hf_cache")
os.environ["GRADIO_ANALYTICS_ENABLED"] = "False"

n_voices = len([f for f in os.listdir(os.path.join(STUDIO_HOME, "voices")) if f.endswith(".pt")])
n_wavs = len([f for f in os.listdir(os.path.join(STUDIO_HOME, "outputs")) if f.endswith(".wav")])
print("Pasta do estúdio:", STUDIO_HOME)
print(f"Vozes em cache: {n_voices} · WAVs gerados: {n_wavs} · "
      f"configurações salvas: {os.path.isfile(os.path.join(STUDIO_HOME, 'settings.json'))}")
"""

INSTALL = f"""
#@title 📦 3. Instalar dependências compatíveis
import importlib, importlib.metadata as md, os, subprocess, sys

OMNIVOICE_COMMIT = "{OMNIVOICE_COMMIT}"
GRADIO_VERSION = "{GRADIO_VERSION}"
FLASHINFER_VERSION = "{FLASHINFER_VERSION}"

def pip(*args, check=True):
    cmd = [sys.executable, "-m", "pip", "install", "-q", *args]
    print("$ pip install", " ".join(args))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-3000:], r.stderr[-3000:])
        if check:
            raise RuntimeError("pip falhou: " + " ".join(args))
    return r.returncode == 0

def installed(name):
    try:
        return md.version(name)
    except md.PackageNotFoundError:
        return None

omnivoice_spec = {{
    "pinned": f"git+https://github.com/k2-fsa/OmniVoice.git@{{OMNIVOICE_COMMIT}}",
    "latest": "git+https://github.com/k2-fsa/OmniVoice.git",
    "pypi": "omnivoice",
}}[OMNIVOICE_SOURCE]
marker = os.path.join(APP_DIR, f".installed-{{OMNIVOICE_SOURCE}}-{{OMNIVOICE_COMMIT[:8]}}-{{GRADIO_VERSION}}")

if os.path.exists(marker):
    print("Dependências já instaladas nesta sessão ✔")
else:
    # Never let pip replace the CUDA build of torch that Colab ships.
    # (pin the *distribution* version: it may lack the "+cuXXX" suffix of torch.__version__)
    torch_ver = installed("torch")
    cu_tag = "cu" + torch.version.cuda.replace(".", "") if torch.version.cuda else None
    constraints = [f"torch=={{torch_ver}}"]
    ta_ver = installed("torchaudio")
    if ta_ver is None or ta_ver.split("+")[0] != torch_ver.split("+")[0]:
        index = [f"--index-url=https://download.pytorch.org/whl/{{cu_tag}}"] if cu_tag else []
        pip(f"torchaudio=={{torch_ver.split('+')[0]}}", *index)
        ta_ver = installed("torchaudio")
    constraints.append(f"torchaudio=={{ta_ver}}")
    os.makedirs(APP_DIR, exist_ok=True)
    cfile = os.path.join(APP_DIR, "constraints.txt")
    with open(cfile, "w") as f:
        f.write("\\n".join(constraints) + "\\n")

    pip(omnivoice_spec, f"gradio=={{GRADIO_VERSION}}", "-c", cfile)
    if INSTALL_TEXT_NORMALIZATION:
        if not pip("WeTextProcessing", "num2words", "-c", cfile, check=False):
            print("⚠️ Normalização de texto indisponível (o checkbox será ignorado com aviso).")

    want_fi = FLASHINFER == "on" or (FLASHINFER == "auto" and torch.cuda.is_available()
                                     and torch.cuda.get_device_capability(0) >= (8, 0))
    if want_fi and OMNIVOICE_SOURCE != "pypi" and cu_tag:
        ok = pip(f"flashinfer-python=={{FLASHINFER_VERSION}}",
                 f"flashinfer-jit-cache=={{FLASHINFER_VERSION}}+{{cu_tag}}",
                 f"--extra-index-url=https://flashinfer.ai/whl/{{cu_tag}}/", "-c", cfile, check=False)
        print("FlashInfer instalado ✔" if ok else "⚠️ FlashInfer não instalado — será usado o baseline FP16.")
    open(marker, "w").close()

importlib.invalidate_caches()
os.makedirs(os.path.join(APP_DIR, "voice_studio"), exist_ok=True)
if APP_DIR not in sys.path:
    sys.path.insert(0, APP_DIR)
for name in ("torch", "torchaudio", "transformers", "gradio", "omnivoice", "flashinfer-python", "WeTextProcessing"):
    print(f"  {{name:18s}} {{installed(name) or '—'}}")
"""

SOURCES_MD = """
### 3.b Código do estúdio
As células abaixo gravam o código-fonte da interface e do motor em `APP_DIR/voice_studio/`
(gerado a partir do repositório por `tools/build_notebook.py`). Basta executá-las.
"""

LOAD = """
#@title 🧠 4. Baixar e carregar o OmniVoice
import logging, os, time
logging.basicConfig(level=logging.WARNING)
for mod in [m for m in list(sys.modules) if m == "voice_studio" or m.startswith("voice_studio.")]:
    del sys.modules[mod]  # pick up freshly written sources when re-running
from huggingface_hub import snapshot_download
from voice_studio.engine import StudioEngine

if os.path.isdir(MODEL_ID):
    MODEL_PATH = MODEL_ID
else:
    MODEL_PATH = os.path.join(STUDIO_HOME, "models", MODEL_ID.replace("/", "__"))
    t0 = time.perf_counter()
    try:
        snapshot_download(MODEL_ID, local_dir=MODEL_PATH)  # resumes / verifies; instant when cached
    except Exception as e:
        if not os.path.isfile(os.path.join(MODEL_PATH, "config.json")):
            raise
        print("⚠️ Hub indisponível, usando a cópia do Drive:", type(e).__name__)
    print(f"Pesos prontos em {MODEL_PATH} ({time.perf_counter() - t0:.1f}s)")

prev = globals().get("ENGINE")
if prev is not None and getattr(prev, "model", None) is not None and prev.model_id == MODEL_PATH:
    # the module was re-imported above: hand the loaded model to a fresh engine object
    ENGINE = StudioEngine(model_id=MODEL_PATH, data_dir=STUDIO_HOME,
                          flashinfer=FLASHINFER, cuda_graphs=FLASHINFER_CUDA_GRAPHS).adopt(prev)
    print("Modelo já carregado nesta sessão ✔ (para mudar FlashInfer, reinicie o ambiente)")
else:
    if prev is not None:
        prev.model = None
        import gc; gc.collect(); torch.cuda.empty_cache() if torch.cuda.is_available() else None
    ENGINE = StudioEngine(model_id=MODEL_PATH, data_dir=STUDIO_HOME,
                          flashinfer=FLASHINFER, cuda_graphs=FLASHINFER_CUDA_GRAPHS).load()
    print(f"Modelo carregado em {ENGINE.load_seconds:.1f}s")
print("Aceleração:", ENGINE.acceleration_label, "| FlashInfer:", ENGINE.flashinfer_note)
print("Badge:", ENGINE.badge_text, "| Sample rate:", ENGINE.sampling_rate)
"""

PREPARE = """
#@title 🧩 5. Preparar componentes (aquecimento, cache de vozes, configurações)
import json, time
import numpy as np
from omnivoice.utils.common import fix_random_seed

# Warm-up: initialises CUDA kernels so the first real generation reports honest timings.
fix_random_seed(42)
t0 = time.perf_counter()
_wav = ENGINE.model.generate(text="Warm up.", num_step=8)[0]
if torch.cuda.is_available():
    torch.cuda.synchronize()
assert _wav.size > 0 and np.isfinite(_wav).all(), "warm-up falhou"
print(f"Aquecimento OK ({time.perf_counter() - t0:.2f}s)")

voices = sorted(f for f in os.listdir(ENGINE.voices_dir) if f.endswith(".json"))
print(f"Vozes preparadas no Drive: {len(voices)}")
for f in voices[:20]:
    with open(os.path.join(ENGINE.voices_dir, f), encoding="utf-8") as fh:
        meta = json.load(fh)
    print(f"  • {meta.get('source_file', '?')}: “{meta.get('ref_text', '')[:70]}”")
print("Configurações da interface:", "restauradas do Drive" if RESTORE_LAST_SETTINGS
      and os.path.isfile(ENGINE.settings_path) else "padrão (32 steps · CFG 2.0 · seed 42 · chunk 15s/30s)")
"""

LAUNCH = """
#@title 🚀 6. Iniciar o Gradio e mostrar o link
from IPython.display import HTML, display
from voice_studio.ui import launch

if globals().get("DEMO") is not None:
    try:
        DEMO.close()
    except Exception:
        pass

port = PORT
for attempt in range(5):
    try:
        DEMO = launch(ENGINE, share=PUBLIC_SHARE_LINK, server_port=port,
                      restore_settings=RESTORE_LAST_SETTINGS, prevent_thread_lock=True, quiet=True)
        break
    except OSError:
        port += 1
else:
    raise RuntimeError("Nenhuma porta livre encontrada")

links = []
if IN_COLAB:
    from google.colab.output import eval_js
    links.append(("Link do Colab (mesma conta Google)", eval_js(f"google.colab.kernel.proxyPort({port})")))
else:
    links.append(("Link local", f"http://127.0.0.1:{port}/"))
if getattr(DEMO, "share_url", None):
    links.append(("Link público gradio.live (72 h)", DEMO.share_url))
for name, url in links:
    print(f"{name}: {url}")
display(HTML("<br>".join(f'<a href="{u}" target="_blank" style="font-size:18px">🌌 {n}</a>' for n, u in links)))
print("O servidor continua rodando em segundo plano. Para reiniciar a interface, execute esta célula de novo.")
"""

OUTRO = """
---
### Nova sessão / reabrir
Abra o notebook e use **Executar tudo**. A instalação dos pacotes é refeita (o Colab apaga o ambiente),
mas os pesos, as vozes em cache, os WAVs e as configurações vêm do Drive. Se o link parar de responder,
execute a célula **6** novamente.

### Dicas
* **Voice Clone**: referência de 3–10 s + transcrição exata. Deixe a transcrição vazia para usar o Whisper
  (baixado uma vez para o Drive). **Prepare / Cache Voice** codifica a referência uma vez; as gerações
  seguintes com a mesma referência reutilizam o cache (inclusive em sessões futuras).
* **Voice Design**: escolha atributos (gênero, idade, tom, sussurro, sotaque inglês, dialeto chinês).
* **Narração longa**: acima de *Activate long-form chunking above* o próprio OmniVoice divide o texto em
  trechos de ~*Target internal chunk duration* segundos e une o áudio.
* **Clear GPU / Graph Caches** libera memória da GPU (e os CUDA graphs do FlashInfer, se ativos).
"""


def build():
    cells = [md(INTRO), code(CONFIG), code(GPU), code(DRIVE), code(INSTALL), md(SOURCES_MD)]
    for rel in SOURCES:
        with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
            body = f.read()
        target = "{APP_DIR}/" + rel
        cells.append(code(f"%%writefile {target}\n{body}", form=False))
    cells += [code(LOAD), code(PREPARE), code(LAUNCH), md(OUTRO)]
    for c in cells:  # nbformat stores source as a list of lines
        c["source"] = c["source"].splitlines(keepends=True)
    nb = {
        "cells": cells,
        "metadata": {
            "accelerator": "GPU",
            "colab": {"gpuType": "T4", "provenance": [], "name": "VOICE_STUDIO_DA_GALAXIA.ipynb"},
            "kernelspec": {"display_name": "Python 3", "name": "python3"},
            "language_info": {"name": "python"},
        },
        "nbformat": 4,
        "nbformat_minor": 0,
    }
    out = os.path.join(ROOT, "VOICE_STUDIO_DA_GALAXIA.ipynb")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(nb, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("wrote", out, f"({len(cells)} cells)")


if __name__ == "__main__":
    build()
