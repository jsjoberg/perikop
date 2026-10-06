# Alice and Björn voice pack

The optional pack contains Alice and Björn from the Swedish Kokoro model.
Chatterbox remains the reader's active engine. The reader does not yet offer these voices in its menu.
An automatic content filter blocked the native pronunciation-engine code write during this task.
The completed work covers pack preparation, export checks, offline installation, audio samples, and the native acoustic stage.
The settings database stores the Swedish voice choice. The menu and playback connection remain incomplete.

## Prepare the pack

Python and PyTorch are preparation tools. The native acoustic stage consumes token IDs from the upstream pronunciation engine.
The upstream pronunciation engine still requires Python. A bundled runtime changes the project's native-only requirement.
The application keeps its current playback engine until the user chooses an acceptable integration path.
The preparation script pins the source weights and their revisions. It checks the large source files with SHA-256.

```sh
uv venv --python 3.12 build/kokoro-export-env
uv pip install --python build/kokoro-export-env/bin/python -r tools/speech/kokoro-export-requirements.txt
HF_HUB_DISABLE_XET=1 build/kokoro-export-env/bin/python tools/speech/prepare_kokoro.py --output build/kokoro-pack
python3 tools/speech/install_voice.py --pack kokoro --source build/kokoro-pack
python3 tools/speech/install_voice.py --pack kokoro --verify
```

The installer checks every file before it exposes the new directory. It preserves the existing Chatterbox pack.
The prepared pack occupies approximately 387 MB. It contains one acoustic model, two pronunciation models, a lexicon, and two voice tensors.
Each voice tensor contains 510 styles. Select the style with the phoneme count, as the upstream model requires.

## Export checks and samples

The script compares the pronunciation models with the original PyTorch Transformer at different sequence lengths.
The check compares encoder outputs, decoder logits, and predicted phoneme IDs.
It also checks acoustic output at different sequence lengths for finite, non-silent audio.

For a comparison with the upstream pronunciation engine, use its pinned checkout:

```sh
git clone https://github.com/joakimeriksson/kokoro-sv.git build/kokoro-sv-source
git -C build/kokoro-sv-source checkout 42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f
HF_HUB_DISABLE_XET=1 build/kokoro-export-env/bin/python tools/speech/prepare_kokoro.py --output build/kokoro-pack --reference-source build/kokoro-sv-source
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

The probe consumes a file of integer phoneme token IDs. It does not process words or normalize numbers.
The `speech_voice` preference accepts `alice` and `bjorn`. Alice is the default for a future Kokoro playback engine.

## Sources and limits

The [voice model](https://huggingface.co/Joakim/kokoro-sv-voices) uses Apache 2.0.
Its training data comes from NST and Swedish LibriVox under CC0.
The [upstream code](https://github.com/joakimeriksson/kokoro-sv) and [pronunciation model](https://huggingface.co/Joakim/kokoro-sv-g2p) document the required front end.

The author reports residual decoder artifacts and soft male voices. Biblical names and older Swedish still require listening review.
The native pronunciation stage, voice menu, playback connection, and revised startup buffer remain incomplete.
