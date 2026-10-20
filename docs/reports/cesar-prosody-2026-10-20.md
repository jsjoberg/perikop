# Cesar: listening benchmark and compact prosody prototype

2026-10-20. Baseline: `bd6ae3c`. This records a listening experiment and the first compact model; it does not establish that the prototype sounds idiomatically Swedish.

## What the listener accepted

The native listener described the original melody as repeated pulses of emphasis, with unidiomatic Swedish flow. Transferring both Björn's predicted token timing and measured pitch made things worse: pauses disappeared perceptually, the reading rushed, “fattas” sounded like “faat-tass”, and “skull” ended with a sustained l.

The transfer lengthened the first a in “fattas” from 129 to 200 ms and the final l in “skull” from 89 to 450 ms, while cutting most internal pauses from 112 to 50 ms. Inspection of Björn's waveform shows much of the purported final-l interval fading towards silence. Token allocation in a neural decoder is not an acoustic phone annotation. Treating the whole interval as a held Klatt element was a mistake.

The listener accepted the next, pitch-only version as better and worth preserving. It retains every original element, duration and pause. Both baseline and accepted reading have 347,160 samples at 24 kHz: 14.465 seconds. This is a relative preference over the baseline, not a claim of perfect Swedish.

`tests/fixtures/cesar-prosody` preserves the exact listening WAVs, text, IPA, reference pitch control, provenance and hashes. The accepted control came from a pre-rendered Björn reading, with approximate alignment. Earlier phone-aligned pitch correlations inherit that uncertainty and must not be used to establish naturalness or reliable phone boundaries.

## Reproduce the comparison

```sh
cmake --build build/cmake --target ortho-cesar-audio-probe ortho-cesar-voice-tests
uv run --locked tools/speech/cesar_benchmark.py
ctest --test-dir build/cmake -R '^cesar-voice$' --output-on-failure
```

The benchmark needs no Python ML libraries or voice pack. It renders baseline, accepted reference melody and the new compact candidate into `build/cesar-benchmark`, checks identical sound timing and pause positions, and compares the two reproduced listening controls with the stored WAVs. Whole-utterance loudness is matched for listening; frame-by-frame loudness is not changed. These are reproducibility checks, not automated quality ratings.

For other text, the native probe uses only the existing pronunciation pack:

```sh
build/cmake/bin/ortho-cesar-audio-probe build/kokoro-pack passage.txt candidate.wav --compact --trace build/candidate
```

`--ipa` accepts an IPA file directly, with `-` in the pack position. `--pitch FILE` replays a two-column seconds/Hz contour. `.timing.tsv` records actual per-element sample counts, including zero-duration elements; `.pitch.tsv` records the control clock. The synthesizer rounds each nonzero element to 2.5 ms, so cumulative ideal durations must not be confused with waveform offsets.

## What to take from the literature

**Word accent, prominence and phrasing are separate.** Fant and Kruckenberg's [2000 prominence model](https://www.isca-archive.org/icslp_2000/fant00_icslp.html) represents clause contours and word/syllable prominence separately, with context and position affecting the result. For Cesar this argues for explicit linguistic inputs instead of giving every dictionary stress equal emphasis.

**Use a Swedish tonal representation and a perceptual pitch scale.** Their [2004 synthesis study](https://www.isca-archive.org/tal_2004/fant04_tal.pdf) describes distinct accent-1 and accent-2 anchor patterns, with an additional prominence component, on a semitone scale. In compounds, later stress matters for the rise. Phrase reset depends on pausing. This supplies a model structure, not permission to label all multisyllabic words accent 2. The paper's fitted numerical model has not been reproduced here.

**Pitch correctness is insufficient.** [Bruce and Granström, 1989](https://www.speech.kth.se/qpsr/1989/1989_30_1_017-021.pdf), report that mismatched duration/segment structure can make an otherwise plausible F0 contour unacceptable, particularly when phrasing and focal accent disagree. Their use of labelled acoustic material is a better experimental basis than our decoder-token boundaries. This closely resembles the failed full transfer above.

**Keep symbolic planning and physical realization inspectable.** [Beskow's Veiron/RULSYS description](https://www.speech.kth.se/~beskow/papers/fon98veiron.html) separates control-point generation from filtering into continuous parameter tracks, and permits independent editing and resynthesis. Cesar's timing and pitch traces support that kind of controlled comparison. The longer-term model also needs articulation, intensity and voice-source control; changing F0 alone is a useful experiment, not a complete speech model.

**Inspect other systems without adopting their defaults as Swedish truth.** eSpeak NG's [Swedish language definition](https://github.com/espeak-ng/espeak-ng/blob/master/espeak-ng-data/lang/gmq/sv) selects separate tunes, while its [intonation code](https://github.com/espeak-ng/espeak-ng/blob/master/src/libespeak-ng/intonation.c) represents stress, emphasis and clause endings separately. These are useful architectural comparisons. No eSpeak implementation or parameter table has been copied into Cesar.

## Compact prototype

`swedish_prosody.cpp` now plans a native, reference-free melody from phrase spans, vowel anchors and word metadata. It uses smooth semitone gestures over a declining phrase base, partial continuation resets, variable prominence, and separate explicit accent-1/accent-2 patterns. Secondary stress can anchor a later accent-2 rise. Timing and sound tables remain unchanged, so melody experiments cannot repeat the accidental held-l or missing-pause changes.

The current front end has primary/secondary stress but no reliable lexical accent-1/accent-2 labels or syntactic/focus analysis. The prototype therefore treats multisyllabic accent identity as unknown. It uses a conservative unlabelled contour, an inspectable small weak-word fallback, and a provisional final-content-word focus heuristic. Monosyllabic accent 1 can be assigned structurally. Callers can supply explicit word accent, prominence and focus metadata; tests demonstrate that identical syllable counts can receive different tonal patterns.

These coefficients and heuristics are provisional implementation choices, not a validated reimplementation of KTH's fitted rules. The prototype is available through `--compact` and the diagnostic C++ API. The app's default melody and cache identity remain unchanged until native review accepts a general model on unseen passages. The accepted reference waveform is preserved separately and is not replaced by the prototype.

Next work:

1. Obtain and audit lexical accent and compound-stress information, preserving uncertainty when it is absent.
2. Add a text-level phrasing/prominence representation that can distinguish lexical stress, focus, weak words and question types.
3. Review minimal contrasts and held-out prose with a native listener, comparing one controlled change at a time against the accepted benchmark.
4. Introduce quantity, coarticulation, intensity and voice-source changes only with acoustic evidence and matched listening. Preserve the failed transfer as a caution against interpreting token times as phone durations.

Speech-recognition accuracy and pitch correlation are diagnostics, not optimization targets for Swedishness. Native judgments decide acceptance.

The next isolated timbre experiment is recorded in [Cesar: controlled LF source experiment](cesar-lf-source-2026-10-20.md). It preserves these timing and melody controls while changing only the periodic excitation waveform.
