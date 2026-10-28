"""Inference engine for VOICE STUDIO DA GALAXIA.

Thin, faithful wrapper around the official ``k2-fsa/OmniVoice`` Python API:

* ``OmniVoice.from_pretrained`` loads the real weights (FP16 on CUDA).
* ``OmniVoice.create_voice_clone_prompt`` encodes the reference voice once;
  the resulting ``VoiceClonePrompt`` is cached in memory and on disk
  (Google Drive in Colab) so repeated generations skip that work.
* ``OmniVoice.generate`` does all synthesis, including the engine's native
  long-form chunking (``audio_chunk_duration`` / ``audio_chunk_threshold``).
* FlashInfer (``omnivoice.models.omnivoice_flashinfer.apply_flashinfer``) is
  enabled only when the GPU supports it and a self-test passes; otherwise the
  plain FP16 path is kept (the "T4-safe baseline").

Nothing here re-implements OmniVoice's decoding: the engine only counts calls
to the model's own iterative decoder to report progress and statistics.
"""

import gc
import hashlib
import json
import logging
import os
import re
import threading
import time
import uuid
from dataclasses import dataclass, field
from typing import Callable, Optional

import numpy as np
import soundfile as sf
import torch

logger = logging.getLogger("voice_studio")

DEFAULT_MODEL_ID = "k2-fsa/OmniVoice"

# Preset of the reference studio ("Standard quality preset").
DEFAULTS = {
    "mode": "Voice Clone",
    "language": "Auto",
    "speed": 1.0,
    "num_step": 32,
    "guidance_scale": 2.0,
    "denoise": True,
    "preprocess_prompt": True,
    "postprocess_output": True,
    "normalize_text": False,
    "pad_duration": 0.1,
    "fade_duration": 0.1,
    "seed": 42,
    "audio_chunk_duration": 15.0,
    "audio_chunk_threshold": 30.0,
    "output_filename": "omnivoice_turbo.wav",
}

# Non-verbal tags supported by OmniVoice (README, "Non-Verbal & Pronunciation").
EXPRESSION_TAGS = [
    "[laughter]",
    "[sigh]",
    "[confirmation-en]",
    "[question-en]",
    "[question-ah]",
    "[question-oh]",
    "[question-ei]",
    "[question-yi]",
    "[surprise-ah]",
    "[surprise-oh]",
    "[surprise-wa]",
    "[surprise-yo]",
    "[dissatisfaction-hnn]",
]

# Voice Design attributes, as offered by the official omnivoice-demo.
VOICE_DESIGN_CATEGORIES = {
    "Gender": ["Male", "Female"],
    "Age": ["Child", "Teenager", "Young Adult", "Middle-aged", "Elderly"],
    "Pitch": [
        "Very Low Pitch",
        "Low Pitch",
        "Moderate Pitch",
        "High Pitch",
        "Very High Pitch",
    ],
    "Style": ["Whisper"],
    "English Accent": [
        "American Accent",
        "Australian Accent",
        "British Accent",
        "Chinese Accent",
        "Canadian Accent",
        "Indian Accent",
        "Korean Accent",
        "Portuguese Accent",
        "Russian Accent",
        "Japanese Accent",
    ],
    "Chinese Dialect": [
        "Henan Dialect / 河南话",
        "Shaanxi Dialect / 陕西话",
        "Sichuan Dialect / 四川话",
        "Guizhou Dialect / 贵州话",
        "Yunnan Dialect / 云南话",
        "Guilin Dialect / 桂林话",
        "Jinan Dialect / 济南话",
        "Shijiazhuang Dialect / 石家庄话",
        "Gansu Dialect / 甘肃话",
        "Ningxia Dialect / 宁夏话",
        "Qingdao Dialect / 青岛话",
        "Northeast Dialect / 东北话",
    ],
}


class StudioError(Exception):
    """User-facing error (bad input, missing reference, ...)."""


def build_instruct(selected, extra: Optional[str] = None) -> Optional[str]:
    """Turn Voice Design dropdown values into an OmniVoice ``instruct`` string.

    Dialects are passed in Chinese (the model has no English name for them);
    everything else in English. Validation and language unification are done
    by OmniVoice itself (``_resolve_instruct``).
    """
    parts = []
    for value in selected:
        if not value or value == "Auto":
            continue
        if " / " in value:
            parts.append(value.split(" / ", 1)[1].strip())
        else:
            parts.append(value.strip().lower())
    if extra and extra.strip():
        parts.extend(p.strip() for p in re.split(r"[,，]", extra) if p.strip())
    return ", ".join(parts) if parts else None


def language_choices():
    """``["Auto", <600+ display names>]`` exactly like the official demo."""
    from omnivoice.utils.lang_map import LANG_NAMES, lang_display_name

    return ["Auto"] + sorted(lang_display_name(n) for n in LANG_NAMES)


def _language_to_engine(display: Optional[str]) -> Optional[str]:
    if not display or display == "Auto":
        return None
    from omnivoice.utils.lang_map import LANG_NAMES, lang_display_name

    for name in LANG_NAMES:
        if lang_display_name(name) == display:
            return name
    return display


def _sha1_file(path: str) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def _safe_stem(filename: Optional[str]) -> str:
    stem = os.path.basename((filename or "").strip())
    stem = re.sub(r"\.(wav|wave)$", "", stem, flags=re.IGNORECASE)
    stem = re.sub(r"[^A-Za-z0-9._-]+", "_", stem).strip("._")
    return stem or os.path.splitext(DEFAULTS["output_filename"])[0]


def _fmt_gb(num_bytes: float) -> str:
    return f"{num_bytes / 1024**3:.2f} GB"


@dataclass
class CachedVoice:
    key: str
    prompt: object  # omnivoice.VoiceClonePrompt
    ref_text: str
    encode_seconds: float
    source: str  # "encoded" | "drive"


@dataclass
class GenerationResult:
    path: str
    filename: str
    sample_rate: int
    audio_seconds: float
    generation_seconds: float
    internal_calls: int
    voice_prompt: str
    peak_vram: Optional[float]
    current_vram: Optional[float]
    notes: list = field(default_factory=list)


class StudioEngine:
    def __init__(
        self,
        model_id: str = DEFAULT_MODEL_ID,
        data_dir: str = "./voice_studio_data",
        device: Optional[str] = None,
        flashinfer: str = "auto",
        cuda_graphs: bool = False,
    ):
        self.model_id = model_id
        self.data_dir = os.path.abspath(data_dir)
        self.outputs_dir = os.path.join(self.data_dir, "outputs")
        self.voices_dir = os.path.join(self.data_dir, "voices")
        self.settings_path = os.path.join(self.data_dir, "settings.json")
        for d in (self.outputs_dir, self.voices_dir):
            os.makedirs(d, exist_ok=True)

        if device is None:
            from omnivoice.utils.common import get_best_device

            device = get_best_device()
        self.device = device
        self.device_type = torch.device(device).type
        self.dtype = torch.float16 if self.device_type in ("cuda", "mps", "xpu") else torch.float32
        self.flashinfer_mode = flashinfer
        self.cuda_graphs = cuda_graphs

        self.model = None
        self.sampling_rate = 24000
        self.flashinfer_active = False
        self.flashinfer_note = ""
        self.load_seconds = 0.0

        self._lock = threading.RLock()
        self._voices: dict = {}
        self._calls = 0
        self._steps = 0
        self._progress_hook: Optional[Callable[[int, int], None]] = None

    # ------------------------------------------------------------------
    # Loading
    # ------------------------------------------------------------------

    def _load_model(self):
        from omnivoice import OmniVoice

        device_map = "cuda:0" if self.device == "cuda" else self.device
        self.model = OmniVoice.from_pretrained(
            self.model_id, device_map=device_map, dtype=self.dtype
        )
        self.model.eval()
        self.sampling_rate = int(self.model.sampling_rate)

    def load(self):
        t0 = time.perf_counter()
        self._load_model()
        self.flashinfer_active, self.flashinfer_note = self._maybe_enable_flashinfer()
        self._install_instrumentation()
        self.load_seconds = time.perf_counter() - t0
        logger.info("Model ready in %.1fs (%s)", self.load_seconds, self.acceleration_label)
        return self

    def _smoke_test(self):
        audio = self.model.generate(text="Hello, this is a quick self test.", num_step=8)[0]
        if audio.size == 0 or not np.isfinite(audio).all():
            raise RuntimeError("self-test produced empty or non-finite audio")

    def _maybe_enable_flashinfer(self):
        mode = (self.flashinfer_mode or "auto").lower()
        if mode == "off":
            return False, "disabled"
        if self.device_type != "cuda":
            return False, "requires an NVIDIA GPU"
        if self.dtype != torch.float16:
            return False, "requires FP16"
        cc = torch.cuda.get_device_capability(0)
        if mode == "auto" and cc < (8, 0):
            return False, f"skipped on sm_{cc[0]}{cc[1]} (auto mode enables it on sm_80+)"
        try:
            import flashinfer  # noqa: F401
            from omnivoice.models.omnivoice_flashinfer import apply_flashinfer
        except Exception as e:  # not installed / incompatible build
            return False, f"unavailable ({type(e).__name__})"
        try:
            apply_flashinfer(self.model, enable_cuda_graph=self.cuda_graphs)
            self._smoke_test()
            return True, "self-test passed"
        except Exception as e:
            logger.warning("FlashInfer failed (%s); reloading the baseline model.", e)
            self.model = None
            gc.collect()
            torch.cuda.empty_cache()
            self._load_model()
            return False, f"self-test failed, fell back to baseline ({type(e).__name__})"

    def adopt(self, other: "StudioEngine") -> "StudioEngine":
        """Take over the model already loaded by ``other`` (notebook re-runs
        that re-import this module) instead of loading the weights again."""
        self.model = other.model
        self.sampling_rate = other.sampling_rate
        self.flashinfer_active = other.flashinfer_active
        self.flashinfer_note = other.flashinfer_note
        self.load_seconds = other.load_seconds
        self.cuda_graphs = other.cuda_graphs
        if other.model_id == self.model_id:
            self._voices.update(other._voices)
        self._install_instrumentation()
        return self

    def _install_instrumentation(self):
        """Count calls of OmniVoice's own iterative decoder (and its forward
        steps on the baseline path) for progress and statistics only.
        Idempotent: re-installing replaces the previous wrapper and hook."""
        model = self.model
        inner = getattr(model, "_vs_inner_generate_iterative", None) or model._generate_iterative
        model._vs_inner_generate_iterative = inner
        engine = self

        def counted_generate_iterative(task, gen_config):
            engine._calls += 1
            engine._steps = 0
            engine._report()
            return inner(task, gen_config)

        model._generate_iterative = counted_generate_iterative

        def forward_hook(module, args, output):
            engine._steps += 1
            engine._report()

        previous = getattr(model, "_vs_hook_handle", None)
        if previous is not None:
            previous.remove()
        model._vs_hook_handle = model.register_forward_hook(forward_hook)

    def _report(self):
        if self._progress_hook is not None:
            try:
                self._progress_hook(self._calls, self._steps)
            except Exception:  # progress must never break generation
                pass

    # ------------------------------------------------------------------
    # Labels / environment
    # ------------------------------------------------------------------

    @property
    def gpu_name(self) -> str:
        if self.device_type == "cuda":
            return torch.cuda.get_device_name(0)
        return self.device_type.upper()

    @property
    def gpu_total_gb(self) -> Optional[float]:
        if self.device_type == "cuda":
            return torch.cuda.mem_get_info(0)[1] / 1024**3
        return None

    @property
    def acceleration_label(self) -> str:
        precision = "FP16" if self.dtype == torch.float16 else "FP32"
        if self.flashinfer_active:
            label = f"OmniVoice {precision} + FlashInfer"
            if self.cuda_graphs:
                label += " + CUDA graphs"
            return label
        if self.device_type == "cuda" and "T4" in self.gpu_name:
            return f"OmniVoice {precision} — T4-safe baseline"
        return f"OmniVoice {precision} — baseline"

    @property
    def badge_text(self) -> str:
        parts = [self.gpu_name.replace("NVIDIA ", "")]
        if self.gpu_total_gb is not None:
            parts.append(f"{self.gpu_total_gb:.1f} GB")
        parts.append(self.acceleration_label)
        return " · ".join(parts)

    def vram(self):
        if self.device_type != "cuda":
            return None, None
        return torch.cuda.max_memory_allocated(0), torch.cuda.memory_allocated(0)

    # ------------------------------------------------------------------
    # Settings persistence
    # ------------------------------------------------------------------

    def load_settings(self) -> dict:
        settings = dict(DEFAULTS)
        try:
            with open(self.settings_path, encoding="utf-8") as f:
                saved = json.load(f)
            settings.update({k: v for k, v in saved.items() if k in DEFAULTS})
        except (OSError, ValueError):
            pass
        return settings

    def save_settings(self, settings: dict):
        data = {k: settings[k] for k in DEFAULTS if k in settings}
        tmp = self.settings_path + ".tmp"
        try:
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            os.replace(tmp, self.settings_path)
        except OSError as e:
            logger.warning("Could not save settings: %s", e)

    # ------------------------------------------------------------------
    # Voice prompt cache
    # ------------------------------------------------------------------

    def voice_key(self, ref_audio: str, ref_text: Optional[str], preprocess: bool) -> str:
        transcript = (ref_text or "").strip() or "<auto-transcribe>"
        raw = f"{_sha1_file(ref_audio)}|{transcript}|{int(bool(preprocess))}|{self.model_id}"
        return hashlib.sha1(raw.encode("utf-8")).hexdigest()[:20]

    def lookup_voice(self, key: str) -> Optional[CachedVoice]:
        if key in self._voices:
            return self._voices[key]
        pt = os.path.join(self.voices_dir, f"{key}.pt")
        if not os.path.isfile(pt):
            return None
        from omnivoice import VoiceClonePrompt

        try:
            t0 = time.perf_counter()
            prompt = VoiceClonePrompt.load(pt, map_location="cpu")
            meta = {}
            meta_path = os.path.join(self.voices_dir, f"{key}.json")
            if os.path.isfile(meta_path):
                with open(meta_path, encoding="utf-8") as f:
                    meta = json.load(f)
        except Exception as e:
            logger.warning("Ignoring unreadable cached voice %s: %s", pt, e)
            return None
        voice = CachedVoice(
            key=key,
            prompt=prompt,
            ref_text=prompt.ref_text,
            encode_seconds=float(meta.get("encode_seconds", time.perf_counter() - t0)),
            source="drive",
        )
        self._voices[key] = voice
        return voice

    def is_voice_cached(self, ref_audio: Optional[str], ref_text: Optional[str], preprocess: bool) -> bool:
        if not ref_audio or not os.path.isfile(ref_audio):
            return False
        key = self.voice_key(ref_audio, ref_text, preprocess)
        return key in self._voices or os.path.isfile(os.path.join(self.voices_dir, f"{key}.pt"))

    def prepare_voice(self, ref_audio: Optional[str], ref_text: Optional[str], preprocess: bool = True):
        """Return ``(CachedVoice, hit: bool)``; encodes and persists on a miss."""
        if not ref_audio or not os.path.isfile(ref_audio):
            raise StudioError("Upload a reference voice first (3–10 seconds recommended).")
        with self._lock:
            key = self.voice_key(ref_audio, ref_text, preprocess)
            voice = self.lookup_voice(key)
            if voice is not None:
                return voice, True
            transcript = (ref_text or "").strip() or None
            t0 = time.perf_counter()
            try:
                prompt = self.model.create_voice_clone_prompt(
                    ref_audio=ref_audio, ref_text=transcript, preprocess_prompt=bool(preprocess)
                )
            except ValueError as e:
                raise StudioError(str(e)) from e
            if self.device_type == "cuda":
                torch.cuda.synchronize()
            seconds = time.perf_counter() - t0
            voice = CachedVoice(key, prompt, prompt.ref_text, seconds, "encoded")
            self._voices[key] = voice
            try:
                prompt.save(os.path.join(self.voices_dir, f"{key}.pt"))
                with open(os.path.join(self.voices_dir, f"{key}.json"), "w", encoding="utf-8") as f:
                    json.dump(
                        {
                            "ref_text": prompt.ref_text,
                            "user_transcript": transcript,
                            "encode_seconds": seconds,
                            "source_file": os.path.basename(ref_audio),
                            "preprocess_prompt": bool(preprocess),
                            "model_id": self.model_id,
                            "created": time.strftime("%Y-%m-%d %H:%M:%S"),
                        },
                        f,
                        indent=2,
                        ensure_ascii=False,
                    )
            except OSError as e:
                logger.warning("Voice cached in memory only (disk write failed: %s)", e)
            return voice, False

    def clear_voice_cache(self) -> int:
        with self._lock:
            keys = set(self._voices)
            self._voices.clear()
            for name in os.listdir(self.voices_dir):
                if name.endswith((".pt", ".json")):
                    try:
                        os.remove(os.path.join(self.voices_dir, name))
                        keys.add(os.path.splitext(name)[0])
                    except OSError:
                        pass
            return len(keys)

    # ------------------------------------------------------------------
    # GPU / graph caches
    # ------------------------------------------------------------------

    def clear_gpu_caches(self) -> str:
        with self._lock:
            graphs = 0
            cache = getattr(self.model, "_fi_graph_cache", None)
            if cache:
                graphs = len(cache)
                cache.clear()
            gc.collect()
            if self.device_type == "cuda":
                torch.cuda.synchronize()
                torch.cuda.empty_cache()
                torch.cuda.ipc_collect()
                torch.cuda.reset_peak_memory_stats(0)
                reserved = torch.cuda.memory_reserved(0)
                allocated = torch.cuda.memory_allocated(0)
                return (
                    f"Released cached GPU memory. Captured CUDA graphs cleared: {graphs}. "
                    f"Allocated: {_fmt_gb(allocated)} · Reserved: {_fmt_gb(reserved)}"
                )
            return f"Python garbage collected (device: {self.device_type}, no GPU memory to release)."

    # ------------------------------------------------------------------
    # Generation
    # ------------------------------------------------------------------

    def estimate_internal_calls(self, kw: dict, gen_config) -> Optional[int]:
        """How many times OmniVoice's iterative decoder will run, mirroring
        ``OmniVoice.generate`` / ``_generate_chunked`` (read-only)."""
        try:
            from omnivoice.utils.text import chunk_text_punctuation

            task = self.model._preprocess_all(
                text=kw["text"],
                language=kw.get("language"),
                voice_clone_prompt=kw.get("voice_clone_prompt"),
                instruct=kw.get("instruct"),
                preprocess_prompt=gen_config.preprocess_prompt,
                speed=kw.get("speed"),
                normalize_text=kw.get("normalize_text", False),
            )
            frame_rate = self.model.audio_tokenizer.config.frame_rate
            target = task.target_lens[0]
            if target <= int(gen_config.audio_chunk_threshold * frame_rate):
                return 1
            text = task.texts[0]
            chunk_len = int(gen_config.audio_chunk_duration * frame_rate / (target / len(text)))
            return max(1, len(chunk_text_punctuation(text=text, chunk_len=chunk_len, min_chunk_len=3)))
        except Exception:
            return None

    def generate(
        self,
        *,
        mode: str,
        text: str,
        language: str = "Auto",
        speed: float = 1.0,
        ref_audio: Optional[str] = None,
        ref_text: Optional[str] = None,
        instruct: Optional[str] = None,
        num_step: int = 32,
        guidance_scale: float = 2.0,
        denoise: bool = True,
        preprocess_prompt: bool = True,
        postprocess_output: bool = True,
        normalize_text: bool = False,
        pad_duration: float = 0.1,
        fade_duration: float = 0.1,
        audio_chunk_duration: float = 15.0,
        audio_chunk_threshold: float = 30.0,
        seed: Optional[int] = 42,
        output_filename: str = DEFAULTS["output_filename"],
        progress: Optional[Callable[[float, str], None]] = None,
    ) -> GenerationResult:
        from omnivoice import OmniVoiceGenerationConfig
        from omnivoice.utils.common import fix_random_seed

        text = (text or "").strip()
        if not text:
            raise StudioError("Please enter the text to synthesize.")
        if self.model is None:
            raise StudioError("The model is not loaded yet.")

        gen_config = OmniVoiceGenerationConfig(
            num_step=int(num_step),
            guidance_scale=float(guidance_scale),
            denoise=bool(denoise),
            preprocess_prompt=bool(preprocess_prompt),
            postprocess_output=bool(postprocess_output),
            audio_chunk_duration=float(audio_chunk_duration),
            audio_chunk_threshold=float(audio_chunk_threshold),
            pad_duration=float(pad_duration),
            fade_duration=float(fade_duration),
        )
        notes = []
        kw = dict(
            text=text,
            language=_language_to_engine(language),
            speed=float(speed),
            generation_config=gen_config,
        )

        with self._lock:
            voice_prompt = "not used (" + mode + ")"
            if mode == "Voice Clone":
                if progress:
                    progress(0.0, "Preparing reference voice…")
                voice, hit = self.prepare_voice(ref_audio, ref_text, preprocess_prompt)
                kw["voice_clone_prompt"] = voice.prompt
                if hit:
                    voice_prompt = "cache hit ✅"
                else:
                    voice_prompt = f"encoded now in {voice.encode_seconds:.2f}s, cached for next runs"
            elif mode == "Voice Design":
                if not instruct:
                    raise StudioError("Voice Design needs at least one voice attribute (gender, age, pitch, style, accent or dialect).")
                kw["instruct"] = instruct

            if normalize_text:
                try:
                    from omnivoice.utils.text import normalize_text as _nt

                    _nt("1", kw["language"])
                    kw["normalize_text"] = True
                except ImportError:
                    notes.append("Normalize text skipped: install `omnivoice[tn]` (WeTextProcessing).")

            total_calls = self.estimate_internal_calls(kw, gen_config) or 1
            steps = max(1, gen_config.num_step)

            def hook(calls, step):
                if progress:
                    done = (max(calls - 1, 0) + min(step, steps) / steps) / total_calls
                    progress(min(done, 0.99), f"OmniVoice decoder call {calls}/{max(total_calls, calls)}")

            if seed is not None and str(seed).strip() not in ("", "-1"):
                fix_random_seed(int(seed))
            if self.device_type == "cuda":
                torch.cuda.synchronize()
                torch.cuda.reset_peak_memory_stats(0)

            self._calls, self._steps = 0, 0
            self._progress_hook = hook
            t0 = time.perf_counter()
            try:
                audio = self.model.generate(**kw)[0]
            except ValueError as e:  # e.g. invalid voice-design attributes
                raise StudioError(str(e)) from e
            finally:
                self._progress_hook = None
            if self.device_type == "cuda":
                torch.cuda.synchronize()
            gen_seconds = time.perf_counter() - t0
            calls = self._calls
            peak, current = self.vram()

        audio = np.clip(np.asarray(audio, dtype=np.float32), -1.0, 1.0)
        filename = f"{_safe_stem(output_filename)}_{uuid.uuid4().hex[:7]}.wav"
        path = os.path.join(self.outputs_dir, filename)
        sf.write(path, audio, self.sampling_rate, subtype="PCM_16")
        return GenerationResult(
            path=path,
            filename=filename,
            sample_rate=self.sampling_rate,
            audio_seconds=audio.shape[-1] / self.sampling_rate,
            generation_seconds=gen_seconds,
            internal_calls=calls,
            voice_prompt=voice_prompt,
            peak_vram=peak,
            current_vram=current,
            notes=notes,
        )
