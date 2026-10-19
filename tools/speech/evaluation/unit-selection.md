# Compact speech candidate

This is actual native concatenative synthesis from a reusable waveform bank.
Source recordings exclude all 50 benchmark chapters. Target audio is never used
to select, time or render the candidate. Target phonetic transcriptions are
prepared with the existing frontend. Selection runs offline; playback needs only
the bank, a span plan, and the C++ standard library.

It is an experimental candidate, not integrated into Perikop. The listener found
it understandable but still unpleasant. The bank and plans cover the evaluated passages. A complete corpus
plan index and Alice remain to be built after the voice quality is acceptable.

## What changed

The original extraction assumed that predicted token times were acoustic phone
boundaries. Source-only fricative/vowel spectral separation instead peaks about
55–65 ms earlier. The experiment moves cuts 1,500 samples earlier at 24 kHz.
This is an approximate timing calibration, not forced alignment.

The selector retains multiple occurrences per phone pair. A beam search weighs
neighboring phones, word-boundary compatibility, duration and spectral joins.
Adjacent units from the same recording become one continuous span. At other
joins it overlaps the center of the same phone, optionally choosing phase offline.
It does not resynthesize target pitch or prosody; these can still be unnatural.

The renderer reads only selected waveform spans instead of loading a model or
the entire bank. The selected bank uses the same mu-law coding as the old voice.
The current experimental weights are fixed in the script and recorded in reports.

## Reproduce

Run from the repository root, using Python 3.12 with NumPy 2.3.5. `PACK` is the
existing prepared neural voice pack and `PROBES` contains the existing native
Kokoro probes. Only preparation and phonetic planning use those probes.

```sh
cmake -S tools/speech/evaluation -B build/unit-selection -DCMAKE_BUILD_TYPE=Release
cmake --build build/unit-selection
python tools/speech/evaluation/selection_experiment.py prepare \
  --neural-pack PACK --probes PROBES --output build/connected-sources --count 267
python tools/speech/evaluation/selection_experiment.py select \
  --neural-pack PACK --probes PROBES --sources build/connected-sources \
  --legacy-sources ORIGINAL_700_WORD_CACHE --output build/candidate \
  --alignment-offset -1500 --word-boundary-cost 1.2 --phase-align --limit 40
python tools/speech/evaluation/selection_experiment.py native \
  --neural-pack PACK --probes PROBES --output build/candidate \
  --renderer build/unit-selection/bjorn-unit-render
python tools/speech/evaluation/selection_experiment.py evaluate \
  --neural-pack PACK --probes PROBES --output build/candidate \
  --asr-model ASR_MODEL --cpu
```

For an exact rerun of source selection use `--source-manifest selection.json`
instead of `--sources` and `--legacy-sources`. It verifies source token and audio
hashes, freezes the source set and avoids changing results as preparation grows.
The supplied run uses the first 267 selected verses plus the 23 original cached
word batches. Sources are sorted by filename before bank assembly.

The native command compares every generated sample with the Python reference;
the difference must be at most one signed-16-bit PCM unit. Recognition is then
run on the native files, with no text hints. Development passages are selected
by the script; the held-out set is not used for candidate tuning.

To render an existing plan, no Python or model is needed:

```sh
build/unit-selection/bjorn-unit-render wave-bank.u8 1cor-013-004-007.plan output.wav
```

`BUS1` plans contain a little-endian span count, then bank byte offset (uint64),
sample count (uint32) and overlap (uint32). Offset UINT64_MAX denotes silence
and a clause fade. All audio is mono 24 kHz. Invalid extents, oversized plans,
cross-clause overlaps and trailing bytes are rejected.

The timing-only control can be reproduced with `prepare_voice.py
--alignment-offset -1500`, an isolated output directory and the original cached
700-word batches. With offset zero the rebuilt bank, phone inventory, lexicon
and revision are byte-identical to the shipped files.

Code is original MIT code. Source speech uses the already configured Björn model;
retain its Apache-2.0 license and upstream attribution from the existing voice
pack. No additional engine or model was introduced. Research background:
[Black and Taylor, 1997](https://www.cs.cmu.edu/~awb/papers/ES97units/ES97units.html).
