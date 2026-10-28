"""VOICE STUDIO DA GALAXIA — Gradio studio for the official OmniVoice TTS engine."""

from .engine import DEFAULTS, StudioEngine, StudioError

__all__ = ["DEFAULTS", "StudioEngine", "StudioError"]
__version__ = "1.0.0"
