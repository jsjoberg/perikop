# Phrase flow after the improved but staccato v4 sample

The listener preferred v4 to v3 but still described it as staccato and wanted
more melodic Swedish. This is progress in listening feedback, not acceptance of
the voice. V4 remains frozen as the comparison.

The compact source inserts 140 ms or 320 ms at every punctuation boundary.
In Corinthians, several of those boundaries have no comparable quiet interval
in the neural reference. The source also makes 85 joins between 99 selected
spans, with median span length 160 ms. Its F0 distribution is already similar
to the reference: the source's 10th/50th/90th percentiles are 112/128/153 Hz,
versus 113/127/149 Hz for the reference. These observations motivate testing
phrase continuity, rather than widening pitch range as a proxy for melody.
They do not prove which feature caused the listener's impression.

Swedish prosody research models local word-tone patterns alongside phrase
contours ([Fant and Kruckenberg, 2004](https://www.isca-archive.org/tal_2004/fant04_tal.html))
and studies boundary signals as contributors to synthetic speech fluency
([Bruce, Granström and House, 1990](https://www.isca-archive.org/ssw_1990/bruce90_ssw.html)).
Those papers motivate controlling phrasing and pitch shape together. This
experiment transfers controls from the existing reference; it does not
implement or validate those authors' Swedish prosody models.

## V5 experiment

All waveform material still comes from the reusable compact v2 bank. The neural
reference supplies only offline pitch and pause instructions. The native
renderer, its dependencies and the selected waveform units are unchanged.

V4's spoken-word timing is preserved exactly. Only explicit inserted pauses
change. A 10 ms RMS energy detector finds reference quiet intervals below
0.002 amplitude for at least 30 ms, near each approximate mapped punctuation
boundary. Minor pauses are bounded to 40–180 ms, major pauses to 160–460 ms;
trailing padding stays unchanged. These are experimental bounds, not validated
Swedish pronunciation rules. Quiet consonants and approximate boundaries can
still cause pause-estimation errors.

Reference F0 is interpolated in log frequency and smoothed with a 45 ms Hann
window. Pitch is mapped through the existing approximate phone correspondences
onto the retained word timing. Actual source epochs supply waveform grains;
the target pitch modifies their playback schedule, capped at six semitones
relative to the local source epoch period. Source voicing is retained. Zero
pitch strength reproduces the v4 scheduler exactly.

On the three development passages, every spoken timing-map segment retains
exactly its v4 duration. All new renders have zero uncovered samples and their
native PCM is bit-identical to the Python reference. Twenty-four regression
tests pass, including pause bounds and independent synthetic-signal checks of
pitch control. For Corinthians, REAPER's voiced-frame pitch estimate has median
absolute error 0.17 semitones and 90th-percentile error 1.55 semitones against
the requested contour. This verifies signal control, not naturalness.

The existing unit joins, approximate alignment, pronunciation errors and timbre
remain potential limitations. Pitch shaping cannot restore missing coarticulation.
The listener found v5 partly better, but partly harder to hear. It is not accepted. The next step is full-reference numerical evaluation, rather than another blind synthesis change. Held-out passages are
unused; recognition results are intelligibility guardrails only.

On the same three passages and CPU recognizer settings, v5 has 18 word errors
in 181 reference words (9.94% WER), versus v4's 24 errors (13.26%). All three
passages improve in recognition. This small inspected set cannot establish
naturalness, accent authenticity or performance on unseen passages.

## Reproduce

Use the frozen v2 bank output, v3 phone anchors, v4 word controls, and neural
references documented in the earlier experiment notes:

```sh
python tools/speech/evaluation/phrase_experiment.py \
  --candidate build/compact-candidate-final \
  --word-controls build/prosody-word-control \
  --phone-controls build/prosody-control \
  --teacher build/speech-eval/baseline-v4/audio/neural \
  --output build/phrase-flow-control \
  --ids 1cor-013-004-007 1kgs-019-011-012 1sam-003-009-010
```

Input audio, cached analysis tracks and control reports are checked by hashes.
Playback uses the unchanged native `bjorn-grain-render` with a compact input
waveform and the generated `phrase-flow.grains` plan. The known-corpus control
plans remain an experimental, uncompressed format.
