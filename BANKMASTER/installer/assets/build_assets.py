#!/usr/bin/env python3
"""BANKMASTER BY KALOX - installer artwork packaging.

The artwork itself is drawn by the art tool (Design/Art, same design-system code as the
plug-in UI), which writes the PNG renders into this folder. This script only converts
them into the formats the Windows installer needs:

  bankmaster.ico                          Setup / uninstaller / app icon (16..256 px, each size drawn natively)
  wizard_large_100.bmp / _200.bmp         Inno Setup WizardImageFile (164x314 / 328x628)
  wizard_small_100.bmp / _200.bmp         Inno Setup WizardSmallImageFile (55x55 / 110x110)

Requires Pillow. Run: python3 build_assets.py
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def p(name):
    return os.path.join(HERE, name)


sizes = [16, 24, 32, 48, 64, 128, 256]
icons = [Image.open(p(f"icon_{s}.png")).convert("RGBA") for s in sizes]
icons[-1].save(p("bankmaster.ico"), format="ICO", sizes=[(s, s) for s in sizes], append_images=icons[:-1])

for name in ("wizard_large_100", "wizard_large_200", "wizard_small_100", "wizard_small_200"):
    Image.open(p(name + ".png")).convert("RGB").save(p(name + ".bmp"), format="BMP")

print("bankmaster.ico + wizard bitmaps written")
