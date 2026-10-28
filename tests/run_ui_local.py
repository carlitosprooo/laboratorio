"""Launch the studio locally (no Colab), e.g. against the tiny test checkpoint.

Usage: python tests/run_ui_local.py --model /path/to/tiny_omnivoice --data ./data
"""

import argparse
import logging
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from voice_studio.engine import StudioEngine  # noqa: E402
from voice_studio.ui import launch  # noqa: E402

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="k2-fsa/OmniVoice")
    ap.add_argument("--data", default="./voice_studio_data")
    ap.add_argument("--device", default=None)
    ap.add_argument("--port", type=int, default=7860)
    ap.add_argument("--flashinfer", default="auto", choices=["auto", "on", "off"])
    args = ap.parse_args()
    logging.basicConfig(level=logging.INFO)
    engine = StudioEngine(model_id=args.model, data_dir=args.data, device=args.device, flashinfer=args.flashinfer).load()
    launch(engine, server_name="127.0.0.1", server_port=args.port)
