"""Engine-level test against a real OmniVoice checkpoint (tiny or full).

Usage:
    python tests/make_tiny_model.py /tmp/tiny_omnivoice
    python tests/test_engine.py /tmp/tiny_omnivoice [workdir]
"""

import os
import sys
import tempfile

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from voice_studio.engine import StudioEngine, StudioError, build_instruct  # noqa: E402

TRANSCRIPT = "A used RAV4 hybrid is supposed to be the easy decision."


def check(cond, msg):
    print(("ok  : " if cond else "FAIL: ") + msg)
    if not cond:
        raise SystemExit(1)


def main(model, work):
    os.makedirs(work, exist_ok=True)
    data = os.path.join(work, "data")
    sr = 24000
    t = np.arange(4 * sr) / sr
    ref = os.path.join(work, "ref.wav")
    sf.write(ref, (0.3 * np.sin(2 * np.pi * 180 * t) * (0.5 + 0.5 * np.sin(2 * np.pi * 3 * t))).astype(np.float32), sr)

    e = StudioEngine(model_id=model, data_dir=data).load()
    print("acceleration:", e.acceleration_label, "|", e.flashinfer_note)

    r = e.generate(mode="Auto Voice", text="Hello world, this is auto voice.")
    check(r.audio_seconds > 0 and r.internal_calls == 1, f"auto voice ({r.audio_seconds:.2f}s)")
    r = e.generate(mode="Voice Design", text="Hello there.",
                   instruct=build_instruct(["Female", "Auto", "Low Pitch", "Auto", "British Accent", "Auto"]))
    check(r.audio_seconds > 0, "voice design")
    r = e.generate(mode="Voice Clone", text="Clone test.", ref_audio=ref, ref_text=TRANSCRIPT)
    check(r.voice_prompt.startswith("encoded now"), "clone: first run encodes the reference")
    r = e.generate(mode="Voice Clone", text="Second sentence.", ref_audio=ref, ref_text=TRANSCRIPT)
    check(r.voice_prompt.startswith("cache hit"), "clone: second run is a cache hit")

    long_text = " ".join(f"This is sentence number {i} of a long narration, which keeps going." for i in range(60))
    expected = e.estimate_internal_calls(
        {"text": long_text, "language": None, "voice_clone_prompt": e.prepare_voice(ref, TRANSCRIPT)[0].prompt, "speed": 1.0},
        __import__("omnivoice").OmniVoiceGenerationConfig(num_step=4))
    r = e.generate(mode="Voice Clone", text=long_text, ref_audio=ref, ref_text=TRANSCRIPT, num_step=4)
    check(r.internal_calls > 1 and r.internal_calls == expected,
          f"long-form native chunking: {r.internal_calls} decoder calls (expected {expected}), {r.audio_seconds:.1f}s audio")

    a = sf.read(e.generate(mode="Auto Voice", text="Seed check.", seed=42).path)[0]
    b = sf.read(e.generate(mode="Auto Voice", text="Seed check.", seed=42).path)[0]
    check(a.shape == b.shape and np.allclose(a, b), "same seed -> identical audio")
    info = sf.info(r.path)
    check((info.samplerate, info.subtype, info.channels) == (24000, "PCM_16", 1), "WAV is 24 kHz 16-bit mono")

    e2 = StudioEngine(model_id=model, data_dir=data).load()
    check(e2.is_voice_cached(ref, TRANSCRIPT, True), "voice cache persisted on disk (new engine)")
    check(e2.generate(mode="Voice Clone", text="x y z.", ref_audio=ref, ref_text=TRANSCRIPT).voice_prompt.startswith("cache hit"),
          "persisted voice is reused after restart")
    check(e2.clear_voice_cache() == 1 and not e2.is_voice_cached(ref, TRANSCRIPT, True), "clear voice cache")
    print(e2.clear_gpu_caches())
    for kw, name in (({"mode": "Voice Design", "instruct": "femal, lowpitch"}, "invalid voice design"),
                     ({"mode": "Voice Clone", "ref_audio": None}, "clone without reference"),
                     ({"mode": "Auto Voice", "text": "  "}, "empty text")):
        kw.setdefault("text", "hi")
        try:
            e2.generate(**kw)
            check(False, name + " should be rejected")
        except StudioError as ex:
            check(True, f"{name} rejected: {str(ex).splitlines()[0][:70]}")
    print("ALL ENGINE TESTS PASSED")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="vs_test_"))
