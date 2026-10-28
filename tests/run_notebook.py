"""Execute VOICE_STUDIO_DA_GALAXIA.ipynb outside Colab against a local checkpoint, then run the browser e2e test
against the server the notebook launched.

Usage: python tests/run_notebook.py <workdir> <model_dir> <reference.wav> [--rerun]
  --rerun  executes all cells a second time in the same kernel (re-running the notebook in one session)
"""
import copy, json, os, sys, time
import nbformat
from nbclient import NotebookClient

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TINY = os.path.abspath(sys.argv[2])
REF = os.path.abspath(sys.argv[3])
nb = nbformat.read(os.path.join(ROOT, "VOICE_STUDIO_DA_GALAXIA.ipynb"), as_version=4)
cfg = nb.cells[1]
cfg.source = cfg.source.replace('MODEL_ID = "k2-fsa/OmniVoice"', f'MODEL_ID = "{TINY}"') \
                       .replace("PUBLIC_SHARE_LINK = True", "PUBLIC_SHARE_LINK = False") \
                       .replace("PORT = 7860", "PORT = 7871")
check = nbformat.v4.new_code_cell(f"""
import subprocess, urllib.request
html = urllib.request.urlopen(f"http://127.0.0.1:{{port}}/").read().decode()
assert "VOICE STUDIO DA GALAXIA" in html, "title missing"
r = subprocess.run(["node", "{ROOT}/tests/e2e_ui.js", str(port), "{REF}"], capture_output=True, text=True)
print(r.stdout[-6000:]); print(r.stderr[-2000:])
assert "FAIL" not in r.stdout and r.returncode == 0, "e2e failed"
print("PORT", port, "E2E-OK")
""")
cells = nb.cells + [check]
if "--rerun" in sys.argv:  # run cells 0..6 + check again in the same kernel
    cells = cells + [copy.deepcopy(c) for c in nb.cells[1:]] + [copy.deepcopy(check)]
nb.cells = cells
workdir = sys.argv[1]
os.makedirs(workdir, exist_ok=True)
t0 = time.time()
client = NotebookClient(nb, timeout=1800, kernel_name=os.environ.get("NB_KERNEL", "python3"), resources={"metadata": {"path": workdir}})
try:
    client.execute()
finally:
    for i, c in enumerate(nb.cells):
        if c.cell_type != "code":
            continue
        out = "".join(o.get("text", "") or json.dumps(o.get("data", {}).get("text/plain", ""))[:300] if o.get("output_type") != "error" else "ERROR " + o["ename"] + ": " + o["evalue"] for o in c.outputs)
        print(f"--- cell {i}: {c.source.splitlines()[0][:60]}\n{out[-2500:]}")
    print(f"total {time.time()-t0:.0f}s")
