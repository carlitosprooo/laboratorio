// BANKMASTER — BY KALOX · art generator.
// Draws the app icon and the installer artwork with the same design-system code as the plug-in UI.
//
//   bankmaster_art <fontsDir> <repoRoot>
// writes
//   Resources/Icon/icon_1024.png                 plug-in / app icon master (ICON_BIG)
//   installer/assets/icon_<N>.png                icon renders 16..1024 (for .ico / .icns)
//   installer/assets/dmg_background(@2x).png     macOS DMG window background, 720 x 460 pt
//   installer/assets/wizard_large_100/200.png    Inno Setup side image 164 x 314 (x2)
//   installer/assets/wizard_small_100/200.png    Inno Setup header image 55 x 55 (x2)
//   docs/screenshots/v2_brand.png                identity sheet

#include <juce_graphics/juce_graphics.h>
#include "DesignSystem.h"

using namespace bmv2;
using juce::String;
using juce::Justification;
using F = Fonts;

namespace
{
String U (const char* s) { return String::fromUTF8 (s); }

void text (Graphics& g, Rectangle<float> r, const String& t, float h, F::Weight w, Colour c,
           Justification j = Justification::centredLeft, float kern = 0.0f)
{
    g.setColour (c);
    g.setFont (font (h, w, kern));
    g.drawText (t, r, j, false);
}

void save (const juce::Image& img, const juce::File& f)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

juce::Image renderIcon (int px)
{
    juce::Image img (juce::Image::ARGB, px, px, true);
    Graphics g (img);
    logoMark (g, Rectangle<float> (0, 0, (float) px, (float) px), palette(), true);
    return img;
}

// ------------------------------------------------------------------ DMG background (720 x 460 pt)
// Finder draws the icons centred at the positions of installer/macos/dmg/dmg_settings.py:
//   Install BANKMASTER.pkg (200, 268) · Read Me (430, 268) · Uninstall (580, 268), icon size 88
juce::Image dmgBackground (float scale)
{
    const auto& p = palette();
    const float W = 720, H = 460;
    juce::Image img (juce::Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    // light body: Finder draws the icon labels in dark text
    g.setGradientFill (juce::ColourGradient (Colour (0xfff1f3f4), 0, 150, Colour (0xffdfe3e6), 0, H, false));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setColour (Colour (0x0d000000));
    for (int i = 1; i < 12; ++i) g.fillRect (W * i / 12.0f, 150.0f, 1.0f, H - 150.0f);
    // dark brand band
    auto band = Rectangle<float> (0, 0, W, 150);
    g.setGradientFill (juce::ColourGradient (p.bg0.brighter (0.08f), 0, 0, p.bg1, 0, 150, false));
    g.fillRect (band);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    for (int i = 1; i < 12; ++i) g.fillRect (W * i / 12.0f, 0.0f, 1.0f, 150.0f);
    for (int i = 1; i < 4; ++i) g.fillRect (0.0f, 150.0f * i / 4.0f, W, 1.0f);
    // the mark's curve running across the band
    {
        Path c = markCurve (Rectangle<float> (430, 40, 290, 96));
        glowStroke (g, c, p.accent, 2.4f, 10.0f);
    }
    g.setColour (p.accent);
    g.fillRect (0.0f, 149.0f, W, 2.0f);
    logoMark (g, Rectangle<float> (34, 33, 84, 84), p, true);
    wordmark (g, Rectangle<float> (134, 44, 400, 80), p, 40.0f, true);
    text (g, Rectangle<float> (W - 120, 18, 100, 18), "V2.0", 12.0f, F::SemiBold, p.textDim, Justification::centredRight, 0.12f);

    // icon pedestals
    auto pedestal = [&] (float cx, float cy, bool main)
    {
        const auto r = Rectangle<float> (main ? 132.0f : 108.0f, main ? 132.0f : 108.0f).withCentre ({ cx, cy - 6 });
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.95f), cx, cy - 30, juce::Colours::white.withAlpha (0.0f), cx, cy + 70, true));
        g.fillEllipse (r);
        if (main)
        {
            g.setColour (p.accent.darker (0.25f).withAlpha (0.9f));
            g.drawEllipse (r.reduced (1.0f), 2.0f);
        }
    };
    pedestal (200, 268, true);
    pedestal (430, 268, false);
    pedestal (580, 268, false);

    // steps
    const Colour ink (0xff15181a), dim (0xff5b646a);
    const float xs[3] = { 40, 210, 480 };
    const String stepText[3] = { "Open the installer", U ("Customize: VST3 \xc2\xb7 AU \xc2\xb7 Standalone"), "Rescan plug-ins in your DAW" };
    for (int i = 0; i < 3; ++i)
    {
        auto c = Rectangle<float> (20, 20).withPosition (xs[i], 392);
        g.setColour (ink); g.fillEllipse (c);
        text (g, c, String (i + 1), 11.0f, F::Bold, p.accent, Justification::centred);
        text (g, Rectangle<float> (xs[i] + 28, 392, 240, 20), stepText[i], 12.5f, F::SemiBold, ink);
    }
    text (g, Rectangle<float> (0, 428, W, 16), U ("APPLE SILICON + INTEL  \xc2\xb7  macOS 10.15 OR LATER  \xc2\xb7  \xc2\xa9 2026 KALOX AUDIO"),
          10.0f, F::SemiBold, dim, Justification::centred, 0.12f);
    return img;
}

// ------------------------------------------------------------------ Inno Setup images
juce::Image wizardLarge (float scale)
{
    const auto& p = palette();
    const float W = 164, H = 314;
    juce::Image img (juce::Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.setGradientFill (juce::ColourGradient (p.bg0.brighter (0.06f), 0, 0, p.bg1, 0, H, false));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    for (int i = 1; i < 6; ++i) g.fillRect (0.0f, H * i / 6.0f, W, 1.0f);
    logoMark (g, Rectangle<float> (W * 0.5f - 48, 44, 96, 96), p, true);
    text (g, Rectangle<float> (0, 158, W, 26), "BANK", 23.0f, F::Display, p.text, Justification::centred, 0.06f);
    text (g, Rectangle<float> (0, 182, W, 26), "MASTER", 23.0f, F::Display, p.accent, Justification::centred, 0.06f);
    text (g, Rectangle<float> (0, 212, W, 16), "BY KALOX", 9.5f, F::SemiBold, p.textDim, Justification::centred, 0.42f);
    text (g, Rectangle<float> (0, H - 34, W, 14), "4-IN-1 MASTERING", 8.5f, F::SemiBold, p.textMute, Justification::centred, 0.2f);
    return img;
}

juce::Image wizardSmall (float scale)
{
    const float S = 55;
    juce::Image img (juce::Image::RGB, (int) (S * scale), (int) (S * scale), true);
    Graphics g (img);
    g.fillAll (juce::Colours::white);
    logoMark (g, Rectangle<float> (0, 0, S * scale, S * scale).reduced (2 * scale), palette(), true);
    return img;
}

// ------------------------------------------------------------------ brand sheet
juce::Image brandSheet()
{
    const auto& p = palette();
    const float W = 1600, H = 760;
    juce::Image img (juce::Image::ARGB, (int) W, (int) H, true);
    Graphics g (img);
    g.setGradientFill (juce::ColourGradient (p.bg0, 0, 0, p.bg1, 0, H, false));
    g.fillAll();
    text (g, Rectangle<float> (48, 28, 1000, 24), U ("BANKMASTER \xe2\x80\x94 BY KALOX  \xc2\xb7  IDENTITY V2  \xc2\xb7  OBSIDIAN / NEON"), 13.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.16f);
    auto lock = Rectangle<float> (48, 80, 900, 220);
    panel (g, lock, p, 18, true);
    logoMark (g, lock.withWidth (220).reduced (24), p, true);
    wordmark (g, lock.withTrimmedLeft (240).withTrimmedTop (66), p, 62.0f, true);
    auto light = Rectangle<float> (980, 80, 572, 220);
    g.setColour (Colour (0xfff2f3f4)); g.fillRoundedRectangle (light, 18);
    {
        Palette lp = p;
        lp.text = Colour (0xff121416); lp.textDim = Colour (0xff4a5156);
        lp.accent = p.accent.withMultipliedBrightness (0.78f);
        logoMark (g, light.withWidth (180).reduced (26), p, true);
        wordmark (g, light.withTrimmedLeft (190).withTrimmedTop (80), lp, 40.0f, true);
    }
    text (g, Rectangle<float> (48, 330, 900, 20), U ("APP ICON \xe2\x80\x94 macOS \xc2\xb7 Windows \xc2\xb7 DMG \xc2\xb7 SETUP \xe2\x80\x94 REAL PIXEL SIZE"), 12.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.16f);
    float x = 48;
    for (int s : { 256, 128, 64, 48, 32, 24, 16 })
    {
        g.drawImageAt (renderIcon (s), (int) x, (int) (360 + (256 - s)));
        text (g, Rectangle<float> (x, 624, (float) juce::jmax (s, 40), 16), String (s) + " px", 10.0f, F::Medium, p.textMute);
        x += (float) s + 28;
    }
    // module colours
    auto sw = Rectangle<float> (980, 360, 572, 256);
    panel (g, sw, p, 14, false);
    auto in = sw.reduced (24, 20);
    text (g, in.removeFromTop (20), "MODULE COLOURS", 12.0f, F::SemiBold, p.textDim, Justification::centredLeft, 0.16f);
    const char* names[4] = { "EQ", "SATURATION", "COMPRESSOR", "LIMITER" };
    for (int m = 0; m < 4; ++m)
    {
        auto row = in.removeFromTop (50).withTrimmedTop (14);
        g.setColour (p.module[m]); g.fillRoundedRectangle (row.removeFromLeft (60), 6);
        text (g, row.withTrimmedLeft (16), names[m], 14.0f, F::SemiBold, p.text, Justification::centredLeft, 0.1f);
        text (g, row, "#" + p.module[m].toDisplayString (false), 12.0f, F::Medium, p.textMute, Justification::centredRight);
    }
    return img;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s <fontsDir> <repoRoot>\n", argv[0]); return 1; }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    Fonts::get().load (cwd.getChildFile (argv[1]));
    if (! Fonts::get().isLoaded() || Fonts::get().display == nullptr) { std::printf ("fonts not found\n"); return 1; }
    const auto root = cwd.getChildFile (argv[2]);
    const auto assets = root.getChildFile ("installer/assets");

    save (renderIcon (1024), root.getChildFile ("Resources/Icon/icon_1024.png"));
    for (int s : { 16, 24, 32, 48, 64, 128, 256, 512, 1024 })
        save (renderIcon (s), assets.getChildFile ("icon_" + String (s) + ".png"));
    save (dmgBackground (1.0f), assets.getChildFile ("dmg_background.png"));
    save (dmgBackground (2.0f), assets.getChildFile ("dmg_background@2x.png"));
    save (wizardLarge (1.0f), assets.getChildFile ("wizard_large_100.png"));
    save (wizardLarge (2.0f), assets.getChildFile ("wizard_large_200.png"));
    save (wizardSmall (1.0f), assets.getChildFile ("wizard_small_100.png"));
    save (wizardSmall (2.0f), assets.getChildFile ("wizard_small_200.png"));
    save (brandSheet(), root.getChildFile ("docs/screenshots/v2_brand.png"));
    return 0;
}
