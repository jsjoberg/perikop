# Björn evaluation, development only

This benchmark measures the PR #32 experiment. It changes no synthesizer,
production dependency, voice pack, distribution, or runtime architecture.
The fixed Bible corpus can still supply precomputed pronunciation and prosody.
Large evaluation models belong on the development machine.

The subsequent [compact synthesis experiment](unit-selection.md) corrects source
timing, selects connected units offline and renders native speech without a neural
runtime. It is separate from the original baseline evaluator described below.

## Setup and run

Run from the repository root. Python 3.12+, NumPy 2.3.5, FFmpeg and
[whisper.cpp](https://github.com/ggml-org/whisper.cpp) 1.9.1 are used for the
recorded baseline (Metal backend on Apple M4). Install whisper.cpp and FFmpeg separately. CPU mode works
without Metal/CUDA; other operating systems need their own executions verified.
The evaluator accepts local files and makes no implicit network requests.

```sh
cmake -S tools/speech/bjorn-poc -B build/bjorn-standalone -DCMAKE_BUILD_TYPE=Release
cmake --build build/bjorn-standalone --target bjorn-say
# Prepare the existing neural pack and build the native Kokoro probes as described
# in docs/reports/kokoro-voices.md. No new model export is needed if they exist.
python3 tools/speech/evaluation/fetch_asr.py build/models/kb-whisper-small.bin
uv run --script tools/speech/evaluation/run.py \
  --output build/speech-eval/baseline-001 \
  --asr-model build/models/kb-whisper-small.bin --cpu
```

The explicit model downloader pins KBLab revision
`3564d61a42fc210ceaa55a22a96dd64478959c78` and verifies SHA-256
`de6911330cbdc131362f7a955682b65c8a5a2394caba73e7ea821a9822efb8c6`.
KB-Whisper Small is an independent Swedish ASR model, trained separately from
Kokoro; its [model card](https://huggingface.co/KBLab/kb-whisper-small) describes
training data and licenses. It is not a pronunciation model or forced aligner.
The ASR weights are Apache-2.0; retain the model card/license if redistributing
weights. They are not committed or bundled with Perikop.

`--neural-pack`, `--text-probe`, `--audio-probe`, `--compact-pack`,
`--compact-probe`, `--whisper`, and `--ffmpeg` select local paths. A typical neural
pack is `build/kokoro-pack`, probes `build/cmake/bin/ortho-kokoro-*-probe`.
The command supports `--generate-only`, `--no-legacy`, and `--no-controls`.
The default includes all controls and legacy comparisons.

Use a new output directory for each experiment. Interrupted generation can
resume with the same command: per-file receipts check input identity and audio
hashes. Completed reports are immutable and cannot be overwritten. Failed ASR
commands are recorded as technical failures and return exit code 2; they are
never silently scored as successful recognition. Empty *successful* ASR outputs
remain scored as deletions. To retry a completed incomplete report, use a new
output directory; successful ASR results remain cached.

## What the command produces

1. Generate neural Björn and compact Björn Bas for all 50 fixed passages.
   The neural adapter mirrors `PortableSpeech`'s 510-token split at token 16
   (a word gap) and concatenates the resulting PCM. Short inputs use one model
   call. The WAV writer clamps float samples exactly as the reader does, without
   gain normalization; newly rendered chunks record pre-clamp overloads. Model token durations are retained as **unverified teacher predictions**.
   Native frontend input is the frozen corpus text. Reader-level personal/phrase
   pronunciation overrides are not applied. Compact's shipped lexicon includes
   corpus corrections; lexical differences remain a potential confound.
2. Decode the three committed lossless comparison clips and extract their
   neural, 64-harmonic and Bas segments using committed timestamps/checksums.
   Exclude the 0.75-second separators. These are legacy diagnostic controls,
   outside the 50-passage development/heldout scores. Harmonic reconstruction
   fits the complete original recording and is not a generalization benchmark.
3. Generate seven deterministic corruptions for the first five development
   passages: silence, clipping, a three-second internal pause, phase/DC hard
   joins, alternating resampling rates, 100 ms noise replacements, and extensive
   overlaps of adjacent 120 ms chunks. Noise replacement is only a coarse proxy
   for corrupted phones, not verified phoneme substitution. Resampling changes
   pitch as well as timing; extensive overlaps change duration/content as well
   as joins. These are stress tests, not isolated causal interventions.
4. Transcribe **all neural passages first**, then the other conditions. The ASR
   interface receives opaque audio only. Swedish language is fixed; no prompt,
   expected transcript, grammar, hotword, corpus lexicon or pronunciation hint is
   supplied. Each file starts a fresh recognizer process with context disabled,
   beam/best-of 5, temperature zero and fallback disabled. VAD is disabled so
   inserted silence is not silently removed. Audio is resampled to 16 kHz only
   for ASR; acoustic analysis uses the original PCM.
5. Produce `report.json`, `generation.json`, `provenance.json`, audio, token and
   duration files, and incremental `partial-results.json`. Cache keys include
   the audio content hash, model/executable hashes and decoding options.
   Each report retains raw recognizer output and per-passage error alignments.

```sh
# Human-readable tables; use the same NumPy environment as the runner.
uv run --no-project --python 3.12 --with numpy==2.3.5 python \
  tools/speech/evaluation/summarize.py build/speech-eval/baseline-001/report.json \
  --output build/speech-eval/baseline-001/summary.md
# Sensitivity report for the actually transcribed corruption controls.
uv run --no-project --python 3.12 --with numpy==2.3.5 python \
  tools/speech/evaluation/validate.py build/speech-eval/baseline-001/report.json \
  --output build/speech-eval/baseline-001/validation.json
# Read-only audit of stored unit lengths, coverage and source provenance.
uv run --no-project --python 3.12 --with numpy==2.3.5 python \
  tools/speech/evaluation/audit_pack.py resources/voices/bjorn-lite-v1 \
  --output build/compact-pack-audit.json
```

## Dataset and independence

`dataset.json` freezes exact Swedish 1917 corpus strings, references, source
attribution/license identifier and corpus SHA-256. It has 40 development and 10
chapter-disjoint heldout passages, plus three legacy clip transcripts. Selection
is curated for sentence length, narrative/dialogue, names, archaic vocabulary,
Swedish rounded vowels, sje/tje spellings, retroflex spellings and consonant
clusters. These are **orthographic tags**, not measured phonetic coverage.
`build_dataset.py` reproduces the file from `resources/corpus/corpus.db`; do not
regenerate or alter selection to improve a candidate's scores.

Neither split was used to fit evaluation thresholds to Bas results. The Bas
lexicon and unit selection used the known Bible vocabulary, so this heldout split
is **held out from subsequent tuning**, not novel vocabulary or an uncontaminated
training/test split. Do not repeatedly inspect heldout errors to guide changes.
Use dev for iteration, heldout for a chosen candidate's confirmation; repeated
release decisions eventually require a fresh sealed set and human transcription.
Do not reuse these ten heldout passages indefinitely as a development target.

## Metrics and calibration

Primary WER is corpus micro-averaged Levenshtein word error rate. CER uses Unicode
characters after normalization, with spaces removed. Normalize both reference
and recognition with NFC, case folding and punctuation-to-space conversion;
retain å/ä/ö and digits. Whisper control markers such as `<|nospeech|>` are
removed from recognized text before scoring; raw decoder output is retained. Archaic forms and names are not silently modernized or
corrected. WER may exceed 100% through insertions. Reports include substitutions,
deletions, insertions, empty recognition and failed passages (WER >= 50%).
Word categories report substitutions/deletions per expected category word;
insertions are unassigned and categories can overlap. There is no phoneme-error
rate: inferring phoneme errors using the synthesizer's own G2P would be circular.

A predeclared **provisional** reference gate requires full neural coverage, no
empty neural transcripts and neural WER <= 25%, separately per split. This is a
recognizer suitability check, not a human intelligibility threshold. Inspect its
per-passage/category errors even when it passes. Names and archaic vocabulary can
inflate WER; CER and side-by-side transcripts provide context without excusing
candidate errors. If it fails, comparisons report uncertainty; choose a stronger
independent ASR and rerun **both** baseline and candidate with that model.
ASR can recover remembered Bible phrases from poor audio. Controls and human
transcriptions are essential to detect this limitation.

The acoustic profile contains duration, level/clipping, silence fraction/internal
pause runs, autocorrelation pitch/periodicity, voiced runs/transitions, spectral
centroid/flux and waveform/spectral/pitch jump candidates. Candidate timestamps
support inspection; they are not automatically defects. A stop release may
produce a valid spectral jump. Pitch octaves can be estimation errors; voiced
runs are not phoneme durations. Low voicing can reflect noise or legitimate
unvoiced consonants. A smooth waveform can still contain the wrong phonemes.
Clipping is near-rail sample occupancy, not proof of an earlier float overload.

The conservative click test requires a sample step > 0.2 full scale and > 6 times
local RMS. It detects isolated spikes but misses moderate phase/DC joins in a
controlled sine test. Never use zero detected clicks as a quality gate. Silence
uses 40 ms RMS frames, a 10 ms hop and max(-50 dBFS, 1% of peak frame RMS). Pitch
uses 40 ms frames, a 65–450 Hz autocorrelation search and periodicity >= 0.65.
Spectral candidates use adjacent active-frame cosine distance > 0.35; pitch
candidates use > 6 semitones between consecutive voiced frames. None of these
thresholds was validated as a perceptual acceptability threshold.

There is **no combined quality score**. Compare each feature against the same
passage's neural reference and examine the corruption controls. Useful metrics
must respond to intended degradations; failures are evidence about the evaluator.

## Compare an experiment

Run the full command with a new output directory and optionally
`--baseline PATH/TO/BASELINE/report.json`, or compare saved reports:

```sh
uv run --no-project --python 3.12 --with numpy==2.3.5 python \
  tools/speech/evaluation/compare.py \
  build/speech-eval/baseline-001/report.json \
  build/speech-eval/candidate-001/report.json --output build/comparison-001.json
```

Comparison requires identical corpus, scoring implementation and ASR identity,
complete passage coverage, successful reference gates and no technical failures.
It resamples paired **chapter clusters** 4,000 times (seed 1917), aggregating error
counts/denominators to preserve micro weighting. It reports a probable improvement
only if the entire 95% WER-delta interval is below -2 percentage points and the
CER interval agrees (upper endpoint <= 0). Deterioration uses the symmetric rule.
Otherwise the verdict is **uncertain**, including identical runs. Results for dev
and heldout remain separate and include per-passage changes. The two-point margin
is a declared practical decision rule, not a validated perceptual threshold.
The intervals describe passage sampling only; they omit model uncertainty,
corpus memorization, repeated-testing bias and listener disagreement.
A favorable decision authorizes a listening check, not a release.

## Validate the evaluation implementation

```sh
uv run --no-project --python 3.12 --with numpy==2.3.5 python -m unittest \
  discover -s tools/speech/evaluation -p '*_test.py' -v
```

Tests cover known transcript errors, Swedish normalization, unbounded WER,
acoustic control sensitivity/known misses, blinded ASR invocation and caching,
coverage protection, paired uncertainty, incompatibility and conservative
verdicts. They establish implementation behavior. They do **not** establish
speech quality. The recorded full neural/Bas/legacy/control benchmark and human
listening evidence provide the separate evaluation validation.
