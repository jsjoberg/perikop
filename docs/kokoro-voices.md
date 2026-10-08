# Alice and Björn voice pack

The pack contains Alice and Björn from the Swedish Kokoro model.
The reader uses them for all playback. Read-aloud is Swedish only.
Select the voice in **Uppläsning → Alice** or **Björn**. The settings database stores the choice.
A voice change applies from the next reading.

## Prepare the pack

Python and PyTorch are preparation tools managed by `uv`. The distributed application must remain independent of Python and `uv`.
`src/speech/kokoro_text.cpp` is the native pronunciation stage. It reproduces the pinned upstream text path:
number spelling, NST lexicon lookup with custom overrides, the neural model for unknown words, word fixes, and the Kokoro symbol remap.
ONNX Runtime runs the exported pronunciation encoder and decoder.
The reader adds three changes. Its own number spelling uses standard words, such as `fyrtio` and `ettusen`.
A capitalized word with a genitive -s uses the lexicon pronunciation of its stem, such as `Sauls`.
Text in `⟦…⟧` gives exact phonemes from a pronunciation entry. See [the pronunciation review guide](pronunciation-review.md).
`src/speech/kokoro_audio.cpp` turns the token IDs into audio.
The preparation script pins the source weights and their revisions. It checks the large source files with SHA-256.

```sh
uv run --locked --group voice-prep tools/speech/prepare_kokoro.py --output build/kokoro-pack
```

The build bundles the prepared pack in the application's resources, under `voices/` and the pack identifier.
`-DPERIKOP_VOICE_PACK=/path/to/pack` selects another pack directory. The installed program never downloads anything.
The prepared pack occupies approximately 228 MB. It contains one acoustic model, two pronunciation models, a lexicon, and two voice tensors.
The acoustic model stores the vocoder weights as 8-bit integers with one scale per channel. ONNX Runtime restores them to 32-bit floats at load.
All other weights stay 32-bit floats, because rounding them changes durations and prosody.
Each voice tensor contains 510 styles. Select the style with the phoneme count, as the upstream model requires.

## Export checks and samples

The script compares the pronunciation models with the original PyTorch Transformer at different sequence lengths.
The check compares encoder outputs, decoder logits, and predicted phoneme IDs.
It also checks acoustic output at different sequence lengths for finite, non-silent audio.
The compressed acoustic model must predict the same durations as the 32-bit export.

For a comparison with the upstream pronunciation engine, use its pinned checkout:

```sh
git clone https://github.com/joakimeriksson/kokoro-sv.git build/kokoro-sv-source
git -C build/kokoro-sv-source checkout 42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f
uv run --locked --group voice-prep tools/speech/prepare_kokoro.py --output build/kokoro-pack --reference-source build/kokoro-sv-source
```

This command writes samples and timing results into `build/kokoro-audition`.
It compares the acoustic duration predictions with PyTorch for both voices.
The sample filter uses the four notch filters from the upstream inference code.

The October 6, 2026 measurements used ONNX Runtime 1.23.2 with four CPU threads on an Apple M4.
The samples used the upstream Python pronunciation engine and the exported ONNX acoustic model.
These measurements exclude full application startup, lexicon loading, audio-device startup, and the player's buffer delay.

| Measurement | Alice | Björn |
|---|---:|---:|
| Generate “Mose.” | 0.37 s | 0.38 s |
| Word audio duration | 2.08 s | 2.20 s |
| Generate the Psalm passage | 1.26 s | 1.54 s |
| Passage audio duration | 6.83 s | 6.98 s |

The shared acoustic model loaded in 0.27 seconds. These results support a smaller playback buffer after native integration.
They do not establish the reader's time to first audible speech.

The native C++ acoustic probe loaded the model and generated the word sample in 0.85 seconds.
It generated the Björn Psalm sample in 1.28 seconds after a 0.34-second load.
Both measurements exclude the pronunciation engine and audio playback.
The native notch filters match the upstream SciPy filters within a maximum absolute sample error of 0.000000034.

Build the native acoustic probe with this command:

```sh
cmake --build build/cmake --target ortho-kokoro-audio-probe --parallel 4
```

The probe consumes a file of integer phoneme token IDs.
The `speech_voice` preference accepts `alice` and `bjorn`. Alice is the default.

## Native pronunciation check

The text probe prints the token IDs for each input line:

```sh
cmake --build build/cmake --target ortho-kokoro-text-probe --parallel 4
build/cmake/bin/ortho-kokoro-text-probe build/kokoro-pack < lines.txt
```

On October 6, 2026 the probe and the pinned upstream Python front end received all 35,515 Swedish 1917 verses.
The token IDs matched for every verse before the reader's three changes.
After the changes, 207 verses differ, all because of the genitive rule. The verses with digits still match.
These counts exclude the corpus pronunciation entries, which apply before the front end.
The native stage processed all verses in 59 seconds, including the lexicon load.
The reader's Psalm sample with Alice has the same length as the upstream sample. The sample correlation is 0.997.

## Sources and limits

The [voice model](https://huggingface.co/Joakim/kokoro-sv-voices) uses Apache 2.0.
Its training data comes from NST and Swedish LibriVox under CC0.
The [upstream code](https://github.com/joakimeriksson/kokoro-sv) and [pronunciation model](https://huggingface.co/Joakim/kokoro-sv-g2p) document the required front end.

The author reports residual decoder artifacts and soft male voices. Biblical names and older Swedish still require listening review.
The revised startup buffer remains incomplete. The engine measures synthesis speed and adapts its buffer after the first chunk.
