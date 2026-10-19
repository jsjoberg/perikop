# Naturalness experiment after the rejected compact candidate

The user found compact v2 **understandable, but still unpleasant**. Its 17% WER
does not satisfy the primary requirement. That listening result is a failure,
not an acceptance conditional on size or speed. The production voice is unchanged.

This experiment adds explicit timing and pitch control with time-domain
pitch-synchronous overlap-add. The listener rejected both v3 variants as worse
than v2: rhythm was too uneven and felt Norwegian rather than Swedish; the A/B
difference was almost undetectable. This is a naturalness regression despite
improved recognition scores. Intelligibility measurements are guardrails only.

## Audio provenance

Every waveform sample comes from compact v2, already synthesized from its
reusable bank of other passages. No teacher waveform samples enter either new
renderer. The existing neural reference is analyzed offline to provide predicted
phone timing and, for the second variant, pitch targets. This is teacher-derived
prosody for a known corpus, not an independent general-purpose prosody model.

- **A, rhythm:** target phone durations and pauses, deriving a new pulse clock
  from smoothed source F0. This did not preserve the original pulse intervals.
- **B, rhythm and pitch:** the same timing, with pitch targets from the reference.

Source voicing is retained in both variants, to avoid making timing-alignment
errors turn voiced speech into unvoiced intervals. The prepared plan selects
source pitch epochs, target positions and overlap windows. At playback, the
native renderer applies and normalizes those windows. It performs no inference,
pitch analysis or text processing. The original waveform-bank stage remains.

The [Festival documentation](https://www.cstr.ed.ac.uk/projects/festival/manual/festival_21.html)
describes the established combination of diphones, duration/F0 targets and pitch
marks. This implementation is original MIT code; none of Festival's code is used.
[Google REAPER](https://github.com/google/REAPER) supplies offline epoch and F0
analysis, pinned at `1d6e9b95e6b08b500fccbc9a043989dbda747276`, under
[Apache-2.0](https://github.com/google/REAPER/blob/master/LICENSE). It is not a
playback dependency. The existing teacher model's attribution is retained.

## Checks

Eleven development passages were used: the three originally rejected clips and
the first eight development passages. The held-out set is unused. On this small
set, v2 WER is 14.98%; both new variants yield 12.26%. CER is 7.74% for v2,
5.82% for A, and 5.17% for B. These are inspected development results, not proof
of naturalness or an unbiased unseen-corpus estimate.

For 1 Corinthians 13:4–7, v2 WER is 15.25%, A 5.08%, B 10.17%. For 1 Kings
19:11–12, both new variants yield 1.45%. The recognizer cannot choose the pleasant
voice from these values. Subsequent listening rejected both variants.

The original identity control reconstructs the entire compact input waveform, including
boundaries, within floating-point roundoff before PCM encoding. Boundary grains
are explicitly included so that the pitch track's omitted endpoints cannot
silently discard the start of speech. Modified renders have no uncovered samples.
That control used the source epochs directly; it did not exercise the smoothed
F0 clock used for A. It therefore missed a scheduling defect in the modified path.
Native results match the independent Python implementation within one PCM unit.
The tests also verify a 100-to-120 Hz pitch change without changing duration and
reject invalid native grain extents. Eighteen regression tests pass.

The measured native grain-processing stage is approximately 1 ms for the
21.65-second Corinthians clip on this Apple M4, plus approximately 1 ms loading
the intermediate PCM. These times exclude WAV writing, process launch and audio
hardware; target X220i and Pi 5 hardware remains unmeasured. Bulk PCM reads and
cached window tables avoid per-sample I/O and repeated trigonometric work.

## Reproduce

Build REAPER separately from the pinned source above. Use the existing frozen
compact candidate and benchmark neural references. Python needs only the already
used NumPy dependency. From the repository root:

```sh
cmake -S tools/speech/evaluation -B build/unit-selection -DCMAKE_BUILD_TYPE=Release
cmake --build build/unit-selection
python tools/speech/evaluation/prosody_experiment.py \
  --candidate build/compact-candidate-final \
  --teacher build/speech-eval/baseline-v4/audio/neural \
  --neural-pack PACK --reaper REAPER_EXECUTABLE --output build/prosody-control \
  --ids 1cor-013-004-007 1kgs-019-011-012 1sam-003-009-010
python tools/speech/evaluation/selection_experiment.py evaluate \
  --neural-pack PACK --probes PROBES --output build/prosody-control \
  --asr-model ASR_MODEL --cpu
```

The experiment driver performs native/reference verification and writes the
evaluation input. Runtime reproduction needs only the native executable, compact
input PCM and binary controls:

```sh
build/unit-selection/bjorn-grain-render compact.wav rhythm.grains result.wav
```

`BGP1` plans contain little-endian sample rate, output sample count and grain
count, followed by source position (uint32), target position (uint32), left and
right window radii (uint16 each), and gain (float32). Counts, extents and gains
are validated before use. Current proof plans enumerate grains; they are not a
final compressed corpus format.

The current timing-map driver handles single-chunk reference generations.
Two benchmark passages use multiple neural chunks and are not supported by this
experimental driver yet. Residual wrong phones and source timbre are inherited
from v2. Improving timing and pitch does not guarantee that those defects vanish.

[Measurements and provenance](speech-evaluation/compact-prosody.json)

## Bounded word-duration correction after v3 rejection

The v3 dense timing maps allowed local source/output time ratios from 0.125 to
8.25 across the three original problem passages. These intervals include pauses,
not just speech: fixed compact punctuation pauses were being mapped to very
short predicted teacher punctuation durations. Teacher token durations are not
reliable acoustic phone boundaries. V3 also synthesized a new pulse clock from
smoothed F0 even in its source-pitch variant. Its direct-epoch identity control
could not catch that modified-path discrepancy.

The next experiment, v4, uses the same compact input with these narrower rules:

- One uniform duration scale per approximate word; no per-phone timing targets.
- At most 10% duration change, rounded to the nearest sample. Words shorter than
  120 ms stay unchanged, as do the time-map gaps between words and clauses.
- The pulse scheduler uses actual source epoch intervals, repeating or skipping
  epochs as duration changes require. It does not use smoothed F0 or teacher
  pitch targets.

Word extents still come from approximate predicted timing, mapped into the
compact input. They are not forced-aligned boundaries. Source timbre, phonetic
errors and abrupt changes between selected units remain possible. Preserving
source pitch does not establish a Swedish accent or pleasantness.

On the three development passages, the actual scheduler's identity map
reconstructs every source sample within 1.12e-16 float error. All new renders
have zero uncovered samples and native output is bit-identical to the Python
reference PCM. Twenty tests pass, including irregular-epoch scheduler identity,
bounded whole-word duration, unchanged gaps, native equivalence and explicit
error return codes for malformed plans. Native unit tests now use the CMake
output path; a stale legacy binary was being killed and could falsely satisfy
tests that previously accepted any nonzero error code.

Generate the new controls without changing the frozen v3 results:

```sh
python tools/speech/evaluation/prosody_experiment.py \
  --candidate build/compact-candidate-final \
  --teacher build/speech-eval/baseline-v4/audio/neural \
  --neural-pack PACK --reaper REAPER_EXECUTABLE \
  --output build/prosody-word-control --timing-policy bounded-word \
  --anchor-report build/prosody-control/report.json \
  --ids 1cor-013-004-007 1kgs-019-011-012 1sam-003-009-010
```

The optional anchor cache validates both input audio hashes and records the
cache's own hash. Without it the anchors are recomputed from the frozen selection
sources. Playback still uses the same native grain renderer with prepared
controls. Subsequent listening found v4 better than v3, but still staccato and
insufficiently melodic. The follow-up is described in
[the phrase-flow experiment](compact-phrase-flow.md).
