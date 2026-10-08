# Portable speech selection

Decision date: October 6, 2026.

Use native C++ inference, pinned local weights, one reference voice, and common Unicode preprocessing.
Use miniaudio for PCM output on every platform.
Do not use system speech voices or a cloud service.

Update, October 7, 2026: the reader now reads aloud with the Swedish Kokoro voices Alice and Björn only.
Chatterbox and Greek and English read-aloud were removed, because the synthesis delay made them a poor experience.
See [the Kokoro voice guide](kokoro-voices.md). The rest of this record documents the original selection.

Chatterbox Multilingual v2 was the original voice candidate.
Its MIT license permits use, modification, and redistribution of both code and weights.
It supports Swedish, English, and modern Greek.
The application exposes it as a preview. Swedish pronunciation and reading quality still need listening review.
No available benchmark establishes the best Swedish Scripture voice.

## Compared candidates

| Candidate | Engine license | Weight license | Decision |
| --- | --- | --- | --- |
| [Chatterbox Multilingual](https://huggingface.co/ResembleAI/chatterbox) | MIT | MIT | Current preview candidate; supports all three requested languages. |
| [Piper Swedish NST](https://huggingface.co/rhasspy/piper-voices/blob/main/sv/sv_SE/nst/medium/MODEL_CARD) | Current Piper: GPL-3.0 | Swedish training data: CC0 | Keep for listening comparison. A medium Swedish model exists. |
| [Swedish Kokoro](https://huggingface.co/Joakim/swedish-kokoro) | Apache-2.0 | Apache-2.0 | Keep for Swedish comparison. The author documents residual tones and approximate /ɧ/ pronunciation. |
| [Supertonic 3](https://huggingface.co/Supertone/supertonic-3) | MIT | OpenRAIL-M | Exclude: voice weights impose use restrictions. |
| [OmniVoice](https://huggingface.co/k2-fsa/OmniVoice) | Apache-2.0 | CC-BY-NC | Exclude: pretrained weights prohibit commercial use. |
| [Qwen3-TTS](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-0.6B-Base) | Apache-2.0 | Apache-2.0 | Exclude for this voice: Swedish is outside its documented language set. |

An engine license does not establish the license of its voice weights.
Derived ports can also contain stale license claims. The upstream model card controls this comparison.
Piper's model-card quality class is not a listening comparison with Chatterbox.

## Runtime and package

- [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2): MIT; CPU execution on Windows, macOS, and Linux.
- [Native Chatterbox modules](https://github.com/birdup000/chatterboxcpp/tree/1a9a4fc00a6caa8c8bb9bee533225f40661e4220): MIT; pinned source, with local portability and cancellation fixes.
- [miniaudio](https://github.com/mackron/miniaudio/tree/0.11.22): public-domain option selected; PCM transport only.
- [utf8proc](https://github.com/JuliaStrings/utf8proc/tree/v2.10.0): MIT with Unicode data notices; fixed Unicode normalization across platforms.

The application builds only the required ONNX, tokenizer, normalization, audio, and sampling modules.
It does not build the port's server, downloader, or native checkpoint backend.
CUDA and platform voice discovery are disabled. ONNX telemetry is disabled explicitly.
Builds use ordinary architecture targets. They do not use `-march=native` or `-ffast-math`.
Windows model paths use native wide paths. UTF-8 text-file paths convert explicitly.

The voice pack pins [ONNX weights at revision 452d3f4](https://huggingface.co/onnx-community/chatterbox-multilingual-ONNX/tree/452d3f434aa592098f1eedac9099f33642ab2da5).
It contains a Q4 language model, FP32 encoder and decoder, tokenizer, and fixed reference audio.
The pack needs about 1.55 GB. It installs once in the user-data directory, outside application upgrades.
The installer verifies every file's size and SHA-256 before publishing the directory atomically.
The application checks required file sizes before loading the model.
No runtime download, login, API key, or Python interpreter is required.

The same source, model revision, reference recording, seed, and settings apply on every platform.
This does not promise identical floating-point samples across different CPU architectures.
The cached PCM preserves repeat playback within an installation.

The supplied ONNX 1.23.2 Apple Silicon library requires macOS 13.4.
The application baseline therefore changes from macOS 11 to macOS 13.4.
The specification permits a later baseline when tests require it.
Windows 10 or later, x86-64, and Linux x86-64 are targets. Intel Mac builds use the same runtime version.
Windows, Linux, Intel Mac, and minimum-version execution still need their own validation.

## Playback behavior

Synthesis runs on a worker thread. The interface remains responsive during preparation.
The engine synthesizes each verse separately. Long verses split at sentence or word boundaries before inference.
Every non-whitespace byte stays in order. Displayed Scripture remains unchanged.
The engine rejects token-limit truncation, invalid samples, and silent output.
Generative speech can still omit or repeat words; listening review must check this explicitly.

PCM playback starts as soon as the first chunk is ready.
The worker generates subsequent chunks during playback and keeps at most two chunks in the playback buffer.
Pause holds the playback position and limits generation to the available buffer space.
Initial model loading and first-chunk synthesis still delay uncached playback.
If synthesis runs slower than playback, the output supplies silence until the next chunk is ready.

The audio callback counts consumed PCM frames. Pause and underrun silence do not advance this count.
Playback cues associate these frames with a reading, passage section, source edition, and verse range.
The interface follows these cues, including transitions between readings and omitted verse ranges.
Generated introductions have their own playback cues and do not highlight Scripture.

The active verse has a soft highlight. A margin marker moves between its lines with an eased transition.
Verse boundaries use actual audio offsets. Movement within a verse estimates text position from text weight and audio duration.
The current backend supplies no word timestamps. The interface does not claim exact word alignment.

Pause and buffering hold the marker and page position. Stop removes the marker without returning to the beginning.

Automatic scrolling keeps the marker within a comfortable reading band.
Manual wheel, trackpad, keyboard, and scrollbar input release automatic following.
Följ uppläsningen returns to the current passage and restores automatic following.
The toolbar address field distinguishes initial loading, initial preparation, later buffering, playback, pause, completion, and errors.
Brief audio underruns freeze tracking immediately. Buffering feedback appears after 180 milliseconds to prevent flicker.
The controls use one row in wide windows and two rows in narrow windows.

A separate SQLite database caches completed chunks by engine revision, voice, language, and exact speech text.
The audio cache permits at most 256 MiB of PCM data. Evicted pages remain available for reuse.

Stop clears playback and cancels generation at the next token boundary.
An inference call already inside the decoder can finish before cancellation returns.
Closing during initial model loading can wait for loading to finish.

## Local measurements

The first prototype used ONNX Runtime 1.24.1 on this Apple Silicon Mac.
After portable compile flags and four CPU threads, loading took 18.69 seconds.
Generating the Psalm excerpt took 7.47 seconds for 5.16 seconds of audio.
Maximum resident memory was about 2.09 GB.
These numbers include reference conditioning. They do not establish Swedish voice quality.
The final runtime uses the common 1.23.2 release, which also supplies Intel Mac binaries.
See `docs/validation.md` for final application checks.

Before approving a production voice, review Swedish names, numbers, pronunciation overrides, sentence endings, long readings, and exact word coverage.
Compare the same passages with Piper NST and corrected Swedish Kokoro voices.
Check English and polytonic Greek independently. Modern Greek support does not establish suitable liturgical pronunciation.
Verify startup, cache playback, pause, resume, stop, and offline operation on each target platform.
