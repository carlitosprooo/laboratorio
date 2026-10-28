"""Build a tiny, randomly initialised OmniVoice checkpoint for offline tests.

It uses the *official* classes (``OmniVoice``, ``OmniVoiceConfig``, Qwen3,
``HiggsAudioV2TokenizerModel``) with very small dimensions, so the whole
studio pipeline (voice prompt encoding, iterative decoding, long-form
chunking, decoding to 24 kHz audio, Gradio callbacks) runs on a CPU without
downloading anything. The audio it produces is noise: this checks plumbing,
not voice quality.

Usage: python tests/make_tiny_model.py /path/to/out_dir
"""

import os
import sys

import torch
from tokenizers import Tokenizer, decoders, models, pre_tokenizers, trainers
from transformers import (
    DacFeatureExtractor,
    HiggsAudioV2TokenizerConfig,
    HiggsAudioV2TokenizerModel,
    PreTrainedTokenizerFast,
    Qwen3Config,
)

from omnivoice import OmniVoice, OmniVoiceConfig

SPECIAL = [
    "<|denoise|>",
    "<|lang_start|>",
    "<|lang_end|>",
    "<|instruct_start|>",
    "<|instruct_end|>",
    "<|text_start|>",
    "<|text_end|>",
]
CORPUS = [
    "Hello, this is a test of zero-shot voice cloning.",
    "A used RAV4 hybrid is supposed to be the easy decision.",
    "Olá, esta é uma narração longa em português do Brasil.",
    "male, female, low pitch, british accent, whisper, elderly, child",
    "[laughter] [sigh] [question-en] [surprise-oh] 0123456789",
]


def build_text_tokenizer():
    tok = Tokenizer(models.BPE())
    tok.pre_tokenizer = pre_tokenizers.ByteLevel(add_prefix_space=False)
    tok.decoder = decoders.ByteLevel()
    trainer = trainers.BpeTrainer(
        vocab_size=600,
        special_tokens=SPECIAL,
        initial_alphabet=pre_tokenizers.ByteLevel.alphabet(),
    )
    tok.train_from_iterator(CORPUS, trainer)
    fast = PreTrainedTokenizerFast(tokenizer_object=tok)
    fast.add_special_tokens({"additional_special_tokens": SPECIAL})
    return fast


def build_audio_tokenizer():
    cfg = HiggsAudioV2TokenizerConfig(
        # 24 kHz / 960 = 25 frames per second, 8 codebooks (like OmniVoice)
        target_bandwidths=[0.5, 1, 1.5, 2],
        acoustic_model_config={
            "encoder_hidden_size": 8,
            "downsampling_ratios": [8, 5, 4, 2, 3],
            "upsampling_ratios": [8, 5, 4, 2, 3],
            "decoder_hidden_size": 32,
            "hidden_size": 32,
        },
        semantic_model_config={
            "hidden_size": 32,
            "num_hidden_layers": 1,
            "num_attention_heads": 2,
            "intermediate_size": 64,
            "conv_dim": [16] * 7,
            "num_conv_pos_embeddings": 16,
            "num_conv_pos_embedding_groups": 4,
        },
        codebook_dim=8,
    )
    assert cfg.frame_rate == 25 and cfg.num_quantizers == 8, (cfg.frame_rate, cfg.num_quantizers)
    return HiggsAudioV2TokenizerModel(cfg).eval()


def main(out_dir):
    torch.manual_seed(0)
    os.makedirs(out_dir, exist_ok=True)
    text_tok = build_text_tokenizer()
    llm_cfg = Qwen3Config(
        # text embeddings are also looked up at audio positions (ids <= 1024)
        vocab_size=max(len(text_tok), 1100),
        hidden_size=64,
        intermediate_size=128,
        num_hidden_layers=2,
        num_attention_heads=2,
        num_key_value_heads=1,
        head_dim=32,
        max_position_embeddings=8192,
    )
    cfg = OmniVoiceConfig(llm_config=llm_cfg.to_dict())
    model = OmniVoice(cfg)
    model.save_pretrained(out_dir)
    text_tok.save_pretrained(out_dir)

    at_dir = os.path.join(out_dir, "audio_tokenizer")
    build_audio_tokenizer().save_pretrained(at_dir)
    DacFeatureExtractor(sampling_rate=24000, hop_length=960).save_pretrained(at_dir)
    print("tiny OmniVoice checkpoint written to", out_dir)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "tiny_omnivoice")
