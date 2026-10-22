# BANKMASTER — BY KALOX · Design V2

**Status:** concept A (OBSIDIAN / NEON) approved and implemented in BANKMASTER 2.0.

| Where | What |
|---|---|
| `Source/Plugin/UI/DesignSystem.h` | the design system: palette, type scale, knobs, toggles, segmented controls, selectors, icons, meters, logo and wordmark. Pure JUCE drawing code, the same on macOS and Windows |
| `Source/Plugin/UI/Controls.*` | parameter-bound controls (knob, toggle, segmented, selector, drag value) and the look-and-feel |
| `Source/Plugin/UI/Displays.*` | real-time displays: EQ analyser + curves + nodes, saturation transfer + harmonics, compressor transfer + history, limiter history + loudness |
| `Source/Plugin/UI/Pages.*`, `Chrome.*` | the four module pages, header, signal-flow navigation, global bar, popovers |
| `Resources/Fonts/` | Inter + Montserrat (SIL Open Font License 1.1), embedded in the binary |
| `Resources/Icon/icon_1024.png` | app icon master (`ICON_BIG`: .icns on macOS, .ico in the Windows exe) |
| `Design/Art/` | art tool: draws the icon and the installer artwork with the same design system |
| `Design/mockups/` | the mock-ups presented for approval (concepts A and B), kept for reference |

## Regenerating the artwork

```bash
cmake -S Design/Art -B build-art && cmake --build build-art
build-art/bankmaster_art_artefacts/Release/bankmaster_art Resources/Fonts .   # icon + PNG renders
python3 installer/assets/build_assets.py                                     # .ico + Inno Setup bitmaps
```

## Refinements applied after approval

1. Logo: machined rim, glass highlight, neon bloom behind the knee, gradient curve, double ceiling rule and
   haloed knee point; simplified drawing at 16–40 px. Always "BANKMASTER — BY KALOX" (uppercase).
2. Palette: obsidian / graphite / neon green; module colours EQ green, SATURATION amber, COMPRESSOR
   turquoise, LIMITER coral.
3. Type: smallest text 10.5 px at 100 % (9.4 px at the minimum window size); brighter secondary text
   (`textDim` #ADB6BB, `textMute` #76818A); numbers in Inter Medium.
4. Knobs: brighter track and value arc, line pointers on every size (white on small knobs), thicker arcs
   on secondary knobs, soft contact shadow.
5. Limiter display: input fill 22 %, output fill 20 % → 3 %, output envelope line and gain reduction in front.
6. Window: 1080 × 700 minimum, 1200 × 780 default, 2000 × 1300 maximum; the layout scales proportionally
   and is centred, nothing is cut at any size (tested).
