> Experiment as of 2026-10-19. Listening review is still required.

# Fitting a small synthesizer to Björn

## Current: a reusable voice in the reader

**Björn Bas (experimentell)** now performs real Swedish text-to-speech. Select it
in Settings → Röst för uppläsning. The existing playback worker provides caching,
pause/resume, stop, tempo changes and verse cues. If the neural pack is absent,
read-aloud automatically uses Björn Bas. The application's speech probe also
works with source resources containing only this voice.

The committed pack occupies 6,293,055 bytes: a 5.5 MB waveform bank, a roughly
712 kB corpus lexicon, and metadata/notices. It contains 2,139 reusable
diphone/monophone units and 85 stress/length phoneme variants extracted offline
from 700 selected words. It stores no complete verses. New word combinations
are assembled from these units; missing transitions use monophone halves.
Unknown out-of-lexicon words are spelled as Swedish letter names. Numbers use
the same normalizer as Kokoro, and exact `⟦IPA⟧` overrides are supported.

### Addressing the chorus/echo feedback

The first prototype overlapped independently fitted harmonic waves and rendered
their remaining energy as randomized noise. Those are plausible sources of a
chorus-like sound. The new path preserves the master's actual short waveforms,
aligns their voiced joins by correlation, and reduces the overlap to 5 ms when
different pitches do not correlate well. A long crossfade between different
pitches can produce beating even if their initial phases match.

Regression tests cover both equal-pitch waves with different phases and waves
with different pitches. The different-pitch test failed before the adaptive
overlap change and passes with it. This checks a concrete cancellation artifact;
the actual listening result still needs review. Source-unit timing, intonation,
coarticulation and approximate model-derived boundaries remain limitations.

### End-to-end checks

- The standalone `bjorn-say` build links only the system C++/C libraries. It reads
  text or stdin and writes a WAV without ONNX, Python, NumPy, network access or a
  neural pack. Loading takes a few milliseconds locally; several seconds of
  speech render in milliseconds. See the committed sample report for observations.
- Native tests cover new sentence combinations, numbers, word fixes, unknown
  words, exact phonemes, cancellation, deterministic finite audio, phase joins,
  malformed UTF-8 and truncated voice data.
- The production speech worker is tested with the neural pack removed: automatic
  fallback, actual synthesis and cache persistence, voice preferences, pause,
  changed playback speed, verse-cue advancement and completion all pass.
- Existing core/storage, neural cache-recovery, GUI smoke and profile-isolation
  checks pass. The source-resource app speech probe is checked separately.
- Resource hashes and targeted clang-tidy/static-analyzer checks pass.

The main application still links ONNX Runtime for Alice/Björn. This change adds
the experimental fallback and permits omitting their weights; it does not add
an in-app voice downloader or change the default voice. The original MIT C11
harmonic experiment remains available for comparison. No eSpeak or GPL speech
implementation was introduced; teacher data retains its original notices.

The [downloadable comparisons](bjorn-poc-samples/README.md) now contain
**Björn reference → first harmonic prototype → Björn Bas from text**. Their last
segments are not fits of the complete reference utterances.

## Original complete-utterance experiment

The following measurements describe the initial harmonic-only prototype.

The standalone experiment in [tools/speech/bjorn-poc](../../tools/speech/bjorn-poc/README.md)
generates three Björn references using the existing pack and reconstructs them
with an original C harmonic/noise synthesizer. It uses no eSpeak or GPL speech
code. The reconstruction executable links only the system C/math library; it
does not load ONNX, Python, NumPy, Kokoro or the teacher voice tensor.

The fitter estimates pitch and then optimizes pitch, pitch slope, harmonic
amplitudes and phases against short windows of the master recording. Sixteen
noise bands approximate the remaining signal. Fitting is offline. Generated
WAV files, source hashes and all measurements are in `build/bjorn-poc/`.
Downloadable lossless comparisons are committed in
[bjorn-poc-samples](bjorn-poc-samples/README.md). Initially these contained the
reference and two harmonic reconstructions; the current downloads compare the
reference, 64-harmonic reconstruction and reusable voice.

### Initial local result

Measured on this Apple Silicon Mac, with Apple Clang and `-O2`, NumPy 2.3.5 for
offline analysis, and the existing `kokoro-sv-alice-bjorn-2c7968d-v3` pack:

| Reference | Audio duration | 32-harmonic parameters | 64-harmonic parameters |
| --- | ---: | ---: | ---: |
| “Herren är min herde…” | 6.975 s | 58,744 bytes | 103,480 bytes |
| “Han vederkvicker min själ…” | 6.850 s | 57,652 bytes | 101,556 bytes |
| “Abraham, Isak och Jakob…” | 7.175 s | 60,424 bytes | 106,440 bytes |

The native executable occupies 50,992 bytes locally. In the final generation run,
the six reconstructions took approximately 0.26–0.37 CPU seconds each, including
native WAV writes but excluding process startup and audio-device playback. These are single-run local
observations, not benchmarks on low-end hardware; earlier runs on this machine
took 0.05–0.08 CPU seconds, so the observed timings vary substantially. Its audio
accumulation uses a fixed ring buffer; memory also includes the full parameter track.

Every reconstruction has the reference's exact sample count and no clipping.
The magnitude-spectrum error at a 1024-sample window is approximately 4.5–4.8 dB
over bins within 60 dB of the reference's maximum. Doubling the harmonic count
improves that metric only slightly. Residual/noise modelling, pitch tracking and
transients deserve investigation before adding more harmonics. These metrics
do not establish intelligibility or preserved speaker identity.

The six signal/integration tests pass: stationary harmonics with estimated
pitch, a pitch sweep, shaped noise, silence and very short signals, deterministic
rendering, and malformed-input rejection. Synthetic tone checks compare actual
waveforms, while noise checks compare energy and frequency-band retention.
The C renderer builds independently of the application.
Four additional HTTP tests pass for the optional audition server, but the
tailnet page still reported playback errors in iPhone Safari after byte-range
support was added. The downloadable Apple Lossless / M4A comparisons avoid that
development server; playback on the remote iPhone has not been confirmed.

### What the initial prototype established

We can fit waves from Björn and reconstruct the utterances at very low CPU cost
without a neural runtime. Whether the approximation is comfortable to listen
to must be judged using the generated reference/reconstruction audio page.

This does **not** yet produce a reusable voice. It stores a trajectory per
utterance, including individual harmonic phases. Uncompressed parameters scale
to roughly 30 MB/hour for 32 harmonics and 53 MB/hour for 64, so bundling a whole
Bible this way would miss the small-download goal. The next experiment, after
listening approval, would extract reusable phonetic transitions and learn or
curate their timing and pitch controls. A much smaller formant-only model could
lose additional voice detail and needs its own comparison.

The reference generator does not apply the app's separate corpus or personal
pronunciation corrections. Any mistakes in those references carry through.
The default reader and its distribution have not been changed by this experiment.

### Provenance

The fitter and renderer are original Perikop MIT code. NumPy is an offline
BSD-3-Clause dependency. The teacher pack is marked Apache-2.0; the generated
audition retains its original license file and complete source manifest.
No implementation was copied from Mattias Gustavsson's library or eSpeak.
