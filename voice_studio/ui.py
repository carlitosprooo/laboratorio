"""Gradio interface of VOICE STUDIO DA GALAXIA.

Layout, labels, defaults and ranges follow the reference "OmniVoice Turbo
Studio" screenshots; the only identity change is the title.
"""

import html
import inspect
import logging
import os
import traceback

import gradio as gr

from .engine import (
    DEFAULTS,
    EXPRESSION_TAGS,
    VOICE_DESIGN_CATEGORIES,
    StudioEngine,
    StudioError,
    build_instruct,
    language_choices,
)

logger = logging.getLogger("voice_studio")

TITLE = "🌌 VOICE STUDIO DA GALAXIA"
PAGE_TITLE = "VOICE STUDIO DA GALAXIA"
SUBTITLE = "600+ languages · voice cloning · native long-form narration"
MODES = ["Voice Clone", "Voice Design", "Auto Voice"]
_INSERTED = "1"

CSS_PATH = os.path.join(os.path.dirname(__file__), "theme.css")

# Forces the dark palette of the reference, whatever the OS/browser setting.
FORCE_DARK_JS = """
() => {
  document.documentElement.classList.add('dark');
  document.body.classList.add('dark');
  document.querySelectorAll('gradio-app, .gradio-container').forEach(el => el.classList.add('dark'));
}
"""

# Inserts the selected expression tag at the caret of the text box.
INSERT_TAG_JS = """
(text, tag, flag) => {
  if (!tag) return [text, tag, ''];
  const s = text ?? '';
  const ta = document.querySelector('#vs-text textarea');
  let start = s.length, end = s.length;
  if (ta && ta.value === s && typeof ta.selectionStart === 'number') {
    start = ta.selectionStart; end = ta.selectionEnd;
  }
  const before = s.slice(0, start), after = s.slice(end);
  const left = before && !/\\s$/.test(before) ? ' ' : '';
  const right = after && !/^\\s/.test(after) ? ' ' : '';
  return [before + left + tag + right + after, tag, '1'];
}
"""

READY_STATUS = "### 🟣 Ready\n\nEnter text, pick a mode and press **Generate Audio**."
TIP_MD = (
    "**For maximum repeat-generation speed:** Prepare/cache the reference once, "
    "then keep the same reference while changing only the target text."
)


def _counter(text):
    text = text or ""
    return f"**{len(text):,} characters · {len(text.split()):,} words**"


def _voice_status(cached, encoded_s=None, source=None):
    if not cached:
        return (
            "### ⚪ Voice not cached\n\n"
            "Press **Prepare / Cache Voice** to encode the reference once "
            "(Generate Audio also caches it automatically)."
        )
    if source == "drive":
        detail = "Reference restored from the Drive cache. Future generations with this same reference skip that work."
    elif encoded_s is not None:
        detail = f"Reference encoded in **{encoded_s:.2f}s**. Future generations with this same reference skip that work."
    else:
        detail = "Future generations with this same reference skip that work."
    return f"### 🟢 Voice cached\n\n{detail}"


def _finished_md(engine, res, steps, cfg):
    preset = "Standard quality preset" if int(steps) == 32 and float(cfg) == 2.0 else "Custom quality preset"
    rtf = res.generation_seconds / res.audio_seconds if res.audio_seconds else 0.0
    speed_x = res.audio_seconds / res.generation_seconds if res.generation_seconds else 0.0
    lines = [
        f"**Acceleration:** {engine.acceleration_label}",
        f"**Quality:** {preset} · **Steps:** {int(steps)} · **CFG:** {float(cfg):.1f}",
        f"**Voice prompt:** {res.voice_prompt}",
        f"**Internal generation calls:** {res.internal_calls}",
        f"**Audio:** {res.audio_seconds:.1f}s · **Generation:** {res.generation_seconds:.2f}s",
        f"**RTF:** {rtf:.3f} · **Throughput:** {speed_x:.1f}× realtime",
    ]
    if res.peak_vram is not None:
        lines.append(
            f"**Peak VRAM:** {res.peak_vram / 1024**3:.2f} GB · "
            f"**Current VRAM:** {res.current_vram / 1024**3:.2f} GB"
        )
    lines.append(f"**File:** `{res.filename}`")
    lines.extend(f"⚠️ {n}" for n in res.notes)
    return "### ✅ Finished\n\n" + "  \n".join(lines)


def _header_html(engine):
    return (
        '<div id="vs-header">'
        f"<h1>{html.escape(TITLE)}</h1>"
        f'<p class="vs-subtitle">{html.escape(SUBTITLE)}</p>'
        f'<span class="vs-badge">{html.escape(engine.badge_text)}</span>'
        "</div>"
    )


def _footer_md(engine):
    return (
        f"**Engine:** official latest `k2-fsa/OmniVoice` · "
        f"**Acceleration:** `{engine.acceleration_label}` "
        f"**GPU:** `{engine.gpu_name.replace('NVIDIA ', '')}` · "
        f"**Sample rate:** `{engine.sampling_rate:,} Hz`\n\n"
        "Progress shown above is tied to calls of OmniVoice's own iterative decoder. "
        "It does **not** replace or alter OmniVoice's native long-form generation algorithm."
    )


def build_demo(engine: StudioEngine, restore_settings: bool = True, **blocks_kwargs) -> gr.Blocks:
    s = engine.load_settings() if restore_settings else dict(DEFAULTS)
    langs = language_choices()
    if s["language"] not in langs:
        s["language"] = "Auto"
    if s["mode"] not in MODES:
        s["mode"] = DEFAULTS["mode"]

    with gr.Blocks(title=PAGE_TITLE, analytics_enabled=False, **blocks_kwargs) as demo:
        gr.HTML(_header_html(engine))

        with gr.Row(equal_height=False, elem_id="vs-main"):
            # ============================ LEFT ============================
            with gr.Column(scale=7, min_width=420):
                with gr.Group(elem_classes="vs-panel"):
                    mode = gr.Radio(MODES, value=s["mode"], label="Mode", elem_id="vs-mode")
                    text = gr.Textbox(
                        label="Text to synthesize",
                        lines=12,
                        max_lines=40,
                        placeholder=(
                            "Paste short speech or a long narration here. OmniVoice will "
                            "automatically use its native long-form chunking."
                        ),
                        elem_id="vs-text",
                    )
                counter = gr.Markdown(_counter(""), elem_id="vs-counter")

                with gr.Group(elem_classes="vs-panel"):
                    with gr.Row():
                        language = gr.Dropdown(langs, value=s["language"], label="Language", filterable=True)
                        speed = gr.Slider(
                            0.5, 1.5, value=s["speed"], step=0.05,
                            label="Speech speed",
                            info=">1 faster speech · <1 slower speech",
                        )

                with gr.Row(equal_height=False):
                    with gr.Column(scale=5, min_width=200):
                        tag = gr.Dropdown(EXPRESSION_TAGS, value=None, label="Optional expression tag", elem_id="vs-tag")
                    with gr.Column(scale=2, min_width=120):
                        insert_btn = gr.Button("Insert Tag", variant="secondary", elem_id="vs-insert")
                insert_flag = gr.Textbox(visible=False)

                # ---------------- Voice Clone ----------------
                with gr.Group(visible=s["mode"] == "Voice Clone", elem_classes="vs-panel vs-section") as clone_group:
                    gr.Markdown("### 🎤 Voice Clone", elem_classes="vs-section-title")
                    ref_audio = gr.Audio(
                        label="Reference voice (3–10 seconds recommended)",
                        type="filepath",
                        sources=["upload", "microphone"],
                        elem_id="vs-ref-audio",
                    )
                    ref_text = gr.Textbox(
                        label="Exact reference transcript",
                        lines=3,
                        elem_id="vs-ref-text",
                        placeholder=(
                            "Type exactly what is spoken in the reference audio. "
                            "Leave empty to auto-transcribe with Whisper."
                        ),
                    )
                    with gr.Row():
                        prepare_btn = gr.Button("🚀 Prepare / Cache Voice", variant="secondary")
                        clear_voice_btn = gr.Button("Clear Voice Cache", variant="secondary")
                    voice_status = gr.Markdown(_voice_status(False), elem_id="vs-voice-status")

                # ---------------- Voice Design ----------------
                with gr.Group(visible=s["mode"] == "Voice Design", elem_classes="vs-panel vs-section") as design_group:
                    gr.Markdown("### 🎨 Voice Design", elem_classes="vs-section-title")
                    design_dropdowns = []
                    cats = list(VOICE_DESIGN_CATEGORIES.items())
                    for row_start in range(0, len(cats), 3):
                        with gr.Row():
                            for cat, choices in cats[row_start:row_start + 3]:
                                info = {
                                    "English Accent": "Only effective for English speech.",
                                    "Chinese Dialect": "Only effective for Chinese speech.",
                                }.get(cat)
                                design_dropdowns.append(
                                    gr.Dropdown(
                                        ["Auto"] + choices, value="Auto", label=cat, info=info,
                                        elem_id="vs-design-" + cat.lower().replace(" ", "-"),
                                    )
                                )
                    design_extra = gr.Textbox(
                        label="Voice attributes (optional, comma-separated)",
                        placeholder="e.g. female, low pitch, british accent",
                    )

            # ============================ RIGHT ===========================
            with gr.Column(scale=5, min_width=320):
                gr.Markdown("### 🔊 Output", elem_id="vs-output-title")
                audio_out = gr.Audio(
                    label="Generated audio",
                    type="filepath",
                    interactive=False,
                    elem_id="vs-audio-out",
                )
                file_out = gr.File(label="Download WAV", interactive=False, elem_id="vs-file-out")
                status = gr.Markdown(READY_STATUS, elem_id="vs-status")
                gr.Markdown(TIP_MD, elem_id="vs-tip")

        # ======================= FULL WIDTH ===============================
        with gr.Accordion("📚 Long-form Narration", open=True):
            gr.Markdown(
                "These are OmniVoice's **native long-form controls**. The model itself "
                "performs the text segmentation and audio joining."
            )
            with gr.Row():
                chunk_dur = gr.Slider(
                    8, 25, value=s["audio_chunk_duration"], step=1,
                    label="Target internal chunk duration (seconds)",
                    info="15s is the official default.",
                )
                chunk_thr = gr.Slider(
                    15, 60, value=s["audio_chunk_threshold"], step=1,
                    label="Activate long-form chunking above (seconds)",
                    info="30s is the official default.",
                )

        with gr.Accordion("⚙️ Quality / Advanced Settings", open=False):
            gr.Markdown(
                "**Keep 32 steps and CFG 2.0 if you do not want to trade quality for speed.** "
                "FlashInfer, prompt caching and the newer inference path provide the speedup "
                "without lowering these."
            )
            with gr.Row():
                num_step = gr.Slider(
                    4, 64, value=s["num_step"], step=1, label="Inference steps",
                    info="32 = standard. Lower values can be faster but may change quality.",
                    elem_id="vs-steps",
                )
                cfg = gr.Slider(0.0, 4.0, value=s["guidance_scale"], step=0.1, label="Guidance scale (CFG)")
            with gr.Row():
                denoise = gr.Checkbox(value=s["denoise"], label="Denoise")
                preprocess = gr.Checkbox(value=s["preprocess_prompt"], label="Preprocess reference voice")
                postprocess = gr.Checkbox(value=s["postprocess_output"], label="Postprocess generated audio")
                normalize = gr.Checkbox(
                    value=s["normalize_text"], label="Normalize text",
                    info="Useful for numbers/dates when supported.",
                )
            with gr.Row():
                pad = gr.Slider(0.0, 0.5, value=s["pad_duration"], step=0.01, label="Edge padding (seconds)")
                fade = gr.Slider(0.0, 0.5, value=s["fade_duration"], step=0.01, label="Fade duration (seconds)")
                seed = gr.Number(value=s["seed"], precision=0, label="Seed")
            out_name = gr.Textbox(value=s["output_filename"], label="Output filename", max_lines=1)
            clear_gpu_btn = gr.Button("🧹 Clear GPU / Graph Caches", variant="secondary")

        with gr.Row(equal_height=False):
            generate_btn = gr.Button("✨ Generate Audio", variant="primary", size="lg", scale=7, elem_id="vs-generate")
            clear_btn = gr.Button("Clear Text / Output", variant="secondary", size="lg", scale=2, elem_id="vs-clear")

        gr.Markdown("---")
        gr.Markdown(_footer_md(engine), elem_id="vs-footer")

        # ======================= CALLBACKS ================================
        text.change(_counter, text, counter, queue=False, show_progress="hidden")

        def on_mode(m):
            return gr.update(visible=m == "Voice Clone"), gr.update(visible=m == "Voice Design")

        mode.change(on_mode, mode, [clone_group, design_group], queue=False, show_progress="hidden")

        def on_insert(t, chosen, flag):
            if not chosen:
                gr.Info("Choose an expression tag first.")
                return t
            if flag == _INSERTED:
                return t
            t = t or ""
            return (t + (" " if t and not t.endswith((" ", "\n")) else "") + chosen)

        insert_btn.click(
            on_insert, [text, tag, insert_flag], text, js=INSERT_TAG_JS, queue=False, show_progress="hidden"
        )

        def refresh_voice(audio_path, transcript, pre):
            try:
                return _voice_status(engine.is_voice_cached(audio_path, transcript, pre))
            except OSError:
                return _voice_status(False)

        for comp in (ref_audio, preprocess):
            comp.change(refresh_voice, [ref_audio, ref_text, preprocess], voice_status, show_progress="hidden")
        ref_text.blur(refresh_voice, [ref_audio, ref_text, preprocess], voice_status, show_progress="hidden")

        def on_prepare(audio_path, transcript, pre):
            try:
                voice, hit = engine.prepare_voice(audio_path, transcript, pre)
            except StudioError as e:
                gr.Warning(str(e))
                return _voice_status(False), gr.update()
            except Exception as e:
                logger.error("Prepare failed:\n%s", traceback.format_exc())
                return f"❌ **Voice preparation failed**\n\n`{type(e).__name__}: {e}`", gr.update()
            md = _voice_status(True, voice.encode_seconds, voice.source)
            new_text = gr.update(value=voice.ref_text) if not (transcript or "").strip() else gr.update()
            return md, new_text

        prepare_btn.click(on_prepare, [ref_audio, ref_text, preprocess], [voice_status, ref_text])

        def on_clear_voice():
            n = engine.clear_voice_cache()
            gr.Info(f"Voice cache cleared ({n} item(s)).")
            return _voice_status(False)

        clear_voice_btn.click(on_clear_voice, None, voice_status)

        def on_clear_gpu():
            msg = engine.clear_gpu_caches()
            gr.Info("GPU / graph caches cleared.")
            return f"### 🧹 Caches cleared\n\n{msg}"

        clear_gpu_btn.click(on_clear_gpu, None, status)

        def on_generate(
            m, t, lang, spd, r_audio, r_text, *rest, progress=gr.Progress()
        ):
            n = len(design_dropdowns)
            design_values, rest = rest[:n], rest[n:]
            (d_extra, c_dur, c_thr, steps, g_cfg, dn, pre, post, norm, p_pad, p_fade, sd, name) = rest
            settings = dict(
                mode=m, language=lang, speed=spd, num_step=steps, guidance_scale=g_cfg,
                denoise=dn, preprocess_prompt=pre, postprocess_output=post, normalize_text=norm,
                pad_duration=p_pad, fade_duration=p_fade, seed=sd,
                audio_chunk_duration=c_dur, audio_chunk_threshold=c_thr, output_filename=name,
            )
            engine.save_settings(settings)
            no_change = gr.update()
            try:
                res = engine.generate(
                    mode=m, text=t, language=lang, speed=spd,
                    ref_audio=r_audio, ref_text=r_text,
                    instruct=build_instruct(design_values, d_extra),
                    num_step=steps, guidance_scale=g_cfg, denoise=dn,
                    preprocess_prompt=pre, postprocess_output=post, normalize_text=norm,
                    pad_duration=p_pad, fade_duration=p_fade,
                    audio_chunk_duration=c_dur, audio_chunk_threshold=c_thr,
                    seed=sd, output_filename=name,
                    progress=lambda frac, desc: progress(frac, desc=desc),
                )
            except StudioError as e:
                gr.Warning(str(e))
                return no_change, no_change, f"### ⚠️ Check the inputs\n\n{e}", no_change, no_change
            except Exception as e:
                logger.error("Generation failed:\n%s", traceback.format_exc())
                hint = ""
                if "out of memory" in str(e).lower():
                    hint = "\n\nTry **Clear GPU / Graph Caches** or a shorter chunk duration."
                return no_change, no_change, f"### ❌ Generation failed\n\n`{type(e).__name__}: {e}`{hint}", no_change, no_change

            voice_md, transcript_update = no_change, no_change
            if m == "Voice Clone":
                voice_md = _voice_status(True)
                try:
                    voice = engine.lookup_voice(engine.voice_key(r_audio, r_text, pre))
                    voice_md = _voice_status(True, voice.encode_seconds, voice.source)
                    if not (r_text or "").strip():
                        transcript_update = gr.update(value=voice.ref_text)
                except Exception:
                    pass
            return res.path, res.path, _finished_md(engine, res, steps, g_cfg), voice_md, transcript_update

        generate_btn.click(
            on_generate,
            [mode, text, language, speed, ref_audio, ref_text, *design_dropdowns, design_extra,
             chunk_dur, chunk_thr, num_step, cfg, denoise, preprocess, postprocess, normalize,
             pad, fade, seed, out_name],
            [audio_out, file_out, status, voice_status, ref_text],
            concurrency_limit=1,
        )

        clear_btn.click(
            lambda: ("", None, None, READY_STATUS),
            None,
            [text, audio_out, file_out, status],
            queue=False,
        )

    return demo


def make_theme():
    return gr.themes.Soft(primary_hue="indigo", secondary_hue="indigo", neutral_hue="gray")


def launch(
    engine: StudioEngine,
    *,
    share: bool = False,
    server_name: str = "0.0.0.0",
    server_port: int = 7860,
    restore_settings: bool = True,
    prevent_thread_lock: bool = False,
    quiet: bool = False,
):
    with open(CSS_PATH, encoding="utf-8") as f:
        css = f.read()
    style = dict(theme=make_theme(), css=css, js=FORCE_DARK_JS)
    # Gradio 6 takes theme/css/js in launch(); Gradio 5 in the Blocks constructor.
    style_in_launch = "theme" in inspect.signature(gr.Blocks.launch).parameters
    demo = build_demo(engine, restore_settings=restore_settings, **({} if style_in_launch else style))
    demo.queue(default_concurrency_limit=1, max_size=32)
    demo.launch(
        share=share,
        server_name=server_name,
        server_port=server_port,
        allowed_paths=[engine.outputs_dir],
        prevent_thread_lock=prevent_thread_lock,
        quiet=quiet,
        show_error=True,
        inline=False,
        **(style if style_in_launch else {}),
    )
    return demo
