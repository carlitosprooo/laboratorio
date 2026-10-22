#!/usr/bin/env python3
"""BANKMASTER by KALOX - installer artwork generator (ice blue identity).

Generates (into this folder):
  dmg_background.png / dmg_background@2x.png   macOS DMG window background (720x460 pt)
  icon_1024.png                                 app / volume icon master
  wizard_large_100.bmp / wizard_large_200.bmp   Inno Setup WizardImageFile (164x314 / 328x628)
  wizard_small_100.bmp / wizard_small_200.bmp   Inno Setup WizardSmallImageFile (55x55 / 110x110)
  bankmaster.ico                                Windows setup / uninstall icon

Requires Pillow and the Inter font (SIL Open Font License).
Run: python3 generate_assets.py
"""
import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_DIRS = ["/usr/share/fonts/opentype/inter", "/Library/Fonts", os.path.expanduser("~/Library/Fonts")]

ICE = (124, 196, 240)
ICE_DEEP = (60, 143, 208)
TEXT = (29, 43, 57)
TEXT_DIM = (108, 127, 146)
BG_TOP = (240, 246, 251)
BG_BOT = (200, 216, 230)
NIGHT_TOP = (28, 42, 56)
NIGHT_BOT = (14, 22, 31)


def font(weight, size):
    names = {"black": "Inter-Black.otf", "bold": "Inter-Bold.otf", "semibold": "Inter-SemiBold.otf",
             "medium": "Inter-Medium.otf", "regular": "Inter-Regular.otf"}
    for d in FONT_DIRS:
        p = os.path.join(d, names[weight])
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    raise SystemExit("Inter font not found (apt install fonts-inter)")


def vgradient(w, h, top, bot):
    img = Image.new("RGB", (w, h), top)
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return img


def tracked(draw, xy, text, fnt, fill, tracking=0.0, anchor_center=False):
    """Draws text with letter spacing (tracking in px)."""
    widths = [draw.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x, y = xy
    if anchor_center:
        x -= total / 2
    for ch, w in zip(text, widths):
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += w + tracking
    return total


def soft_fill(img, shape_fn, colour, blur, alpha=255):
    """Composites a solid colour through a blurred mask (avoids grey fringes of RGBA blur)."""
    mask = Image.new("L", img.size, 0)
    shape_fn(ImageDraw.Draw(mask))
    mask = mask.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: v * alpha // 255)
    layer = Image.new("RGBA", img.size, colour + (255,))
    out = img.copy()
    out.paste(layer, (0, 0), mask)
    return out


def overlay(img, draw_fn):
    """Alpha-blended drawing (ImageDraw on RGBA replaces pixels instead of blending)."""
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    return Image.alpha_composite(img, layer)


def eq_curve(draw, box, s, colour, width):
    """Decorative EQ curve (low cut + gentle bell + high shelf), like the plugin's display."""
    x0, y0, x1, y1 = box
    pts = []
    for i in range(0, 401):
        t = i / 400.0
        x = x0 + (x1 - x0) * t
        lowcut = -14 * math.exp(-t * 26)
        bell = -2.2 * math.exp(-((t - 0.36) / 0.09) ** 2)
        shelf = 2.6 / (1 + math.exp(-(t - 0.80) * 22))
        db = lowcut + bell + shelf
        y = (y0 + y1) / 2 - db * (y1 - y0) / 30
        pts.append((x * s, y * s))
    w = max(1, int(width * s))
    draw.line(pts, fill=colour, width=w)
    r = w / 2.0
    for (px, py) in pts[::2]:  # round joints: no notches between segments
        draw.ellipse([px - r, py - r, px + r, py + r], fill=colour)


def dmg_background(scale):
    W, H = 720, 460
    s = scale
    img = vgradient(W * s, H * s, BG_TOP, BG_BOT).convert("RGBA")
    img = soft_fill(img, lambda d: d.polygon([(0, 0), (int(W * 0.55 * s), 0), (int(W * 0.25 * s), H * s), (0, H * s)], fill=255),
                    (255, 255, 255), 50 * s, 90)

    # night header band: faint grid + EQ curve in the lower part, title above it
    band_h = 150
    band = vgradient(W * s, band_h * s, NIGHT_TOP, NIGHT_BOT).convert("RGBA")
    def grid(d):
        for i in range(1, 12):
            x = int(W * s * i / 12)
            d.line([(x, 0), (x, band_h * s)], fill=(219, 233, 245, 14), width=max(1, s))
        for j in range(1, 4):
            y = int(band_h * s * j / 4)
            d.line([(0, y), (W * s, y)], fill=(219, 233, 245, 14), width=max(1, s))
    band = overlay(band, grid)
    curve_box = (380, 40, W - 20, 142)
    band = soft_fill(band, lambda d: eq_curve(d, curve_box, s, 255, 8), ICE, 7 * s, 170)
    band = overlay(band, lambda d: eq_curve(d, curve_box, s, (235, 246, 255, 230), 2.2))
    img.paste(band, (0, 0))
    img = overlay(img, lambda d: d.rectangle([0, band_h * s, W * s, band_h * s + 2 * s], fill=ICE + (255,)))
    d = ImageDraw.Draw(img)

    d.text((40 * s, 34 * s), "BANKMASTER", font=font("black", 46 * s), fill=(255, 255, 255, 255))
    f_by = font("bold", 15 * s)
    wby = tracked(d, (42 * s, 94 * s), "by KALOX", f_by, ICE + (255,), 0.5 * s)
    tracked(d, (42 * s + wby + 14 * s, 96 * s), "4-IN-1 MASTERING SUITE", font("semibold", 12 * s), (200, 214, 228, 255), 2.4 * s)
    fv = font("bold", 13 * s)
    d.text((W * s - 40 * s - d.textlength("V1.0", font=fv), 26 * s), "V1.0", font=fv, fill=(200, 214, 228, 255))

    # icon pedestals (Finder draws the icons at the coordinates used in dmg_settings.py)
    for cx, cy, r, a in ((200, 272, 70, 230), (430, 272, 50, 150), (580, 272, 50, 150)):
        img = soft_fill(img, lambda d, cx=cx, cy=cy, r=r: d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s], fill=255),
                        (255, 255, 255), 12 * s, a)
    img = overlay(img, lambda d: d.ellipse([(200 - 76) * s, (272 - 76) * s, (200 + 76) * s, (272 + 76) * s], outline=ICE + (210,), width=2 * s))
    d = ImageDraw.Draw(img)

    # step guide
    f_step = font("semibold", 12 * s)
    f_num = font("bold", 11 * s)
    steps = ["Open the installer", "Customize: VST3 · AU · Standalone", "Rescan plug-ins in your DAW"]
    widths = [30 * s + d.textlength(t, font=f_step) for t in steps]
    gap = (W * s - 80 * s - sum(widths)) / 2
    x, y = 40 * s, 392 * s
    for i, t in enumerate(steps):
        d.ellipse([x, y, x + 20 * s, y + 20 * s], fill=ICE_DEEP + (255,))
        n = str(i + 1)
        d.text((x + 10 * s - d.textlength(n, font=f_num) / 2, y + 3.5 * s), n, font=f_num, fill=(255, 255, 255, 255))
        d.text((x + 28 * s, y + 2.5 * s), t, font=f_step, fill=TEXT + (255,))
        x += widths[i] + gap
    tracked(d, (W * s / 2, 432 * s), "APPLE SILICON + INTEL  ·  macOS 10.15 OR LATER  ·  © 2026 KALOX",
            font("medium", 10 * s), TEXT_DIM + (255,), 1.2 * s, anchor_center=True)
    return img.convert("RGB")


def icon(size=1024):
    """Rendered at 2x and downsampled (clean anti-aliasing)."""
    S = size * 2
    s = S / 1024
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    box = [int(100 * s), int(100 * s), int(924 * s), int(924 * s)]
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius=int(185 * s), fill=255)
    img = soft_fill(img, lambda d: d.rounded_rectangle([box[0], box[1] + int(20 * s), box[2], box[3] + int(20 * s)], radius=int(185 * s), fill=255),
                    (0, 0, 0), int(24 * s), 110)
    body = vgradient(S, S, (38, 57, 76), (11, 18, 26)).convert("RGBA")
    # curve + glow drawn on the body, then clipped by the rounded mask
    curve_box = (170, 560, 854, 800)
    body = soft_fill(body, lambda d: eq_curve(d, curve_box, s, 255, 34), ICE, int(20 * s), 200)
    body = overlay(body, lambda d: eq_curve(d, curve_box, s, (236, 247, 255, 255), 15))
    img.paste(body, (0, 0), mask)
    d = ImageDraw.Draw(img)
    f = font("black", int(330 * s))
    w = d.textlength("BM", font=f)
    d.text(((S - w) / 2, 190 * s), "BM", font=f, fill=(255, 255, 255, 255))
    tracked(d, (S / 2, 790 * s), "KALOX", font("bold", int(60 * s)), ICE + (255,), 14 * s, anchor_center=True)
    img = overlay(img, lambda d: d.rounded_rectangle(box, radius=int(185 * s), outline=ICE + (190,), width=max(2, int(9 * s))))
    return img.resize((size, size), Image.LANCZOS)


def wizard_large(scale):
    W, H = 164 * scale, 314 * scale
    s = scale
    img = vgradient(W, H, NIGHT_TOP, NIGHT_BOT).convert("RGBA")
    img = overlay(img, lambda dd: [dd.line([(0, H * i / 6), (W, H * i / 6)], fill=(219, 233, 245, 16), width=max(1, s)) for i in range(1, 6)])
    img = soft_fill(img, lambda dd: eq_curve(dd, (0, 150, 164, 230), s, 255, 6), ICE, 5 * s, 170)
    img = overlay(img, lambda dd: eq_curve(dd, (0, 150, 164, 230), s, (235, 246, 255, 255), 1.8))
    d = ImageDraw.Draw(img)
    ic = icon(256).resize((96 * s, 96 * s), Image.LANCZOS)
    img.alpha_composite(ic, (int((W - 96 * s) / 2), 26 * s))
    tracked(d, (W / 2, 252 * s), "BANKMASTER", font("black", 19 * s), (255, 255, 255, 255), 0.6 * s, anchor_center=True)
    tracked(d, (W / 2, 276 * s), "by KALOX", font("bold", 11 * s), ICE + (255,), 0.6 * s, anchor_center=True)
    tracked(d, (W / 2, 294 * s), "4-IN-1 MASTERING SUITE", font("semibold", 7 * s), (190, 206, 222, 255), 1.0 * s, anchor_center=True)
    return img.convert("RGB")


def wizard_small(scale):
    n = 55 * scale
    bg = Image.new("RGBA", (n, n), (255, 255, 255, 255))
    ic = icon(512).resize((n, n), Image.LANCZOS)
    bg.alpha_composite(ic)
    return bg.convert("RGB")


def main():
    out = lambda name: os.path.join(HERE, name)
    dmg_background(1).save(out("dmg_background.png"))
    dmg_background(2).save(out("dmg_background@2x.png"))
    ic = icon(1024)
    ic.save(out("icon_1024.png"))
    wizard_large(1).save(out("wizard_large_100.bmp"))
    wizard_large(2).save(out("wizard_large_200.bmp"))
    wizard_small(1).save(out("wizard_small_100.bmp"))
    wizard_small(2).save(out("wizard_small_200.bmp"))
    ic.save(out("bankmaster.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("assets written to", HERE)


if __name__ == "__main__":
    main()
