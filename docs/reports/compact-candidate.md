# Compact candidate: timing and connected units

The compact prototype now has a materially more intelligible native candidate.
On the 40 development passages, WER falls from 118.20% to 17.03% and CER from
102.83% to 8.30%. The neural reference remains better (5.27% WER, 1.77% CER).
No candidate passage has WER at or above 50%; all 40 baseline passages did.

This does not establish a pleasant voice. Listener acceptance is pending. The
candidate is not enabled in Perikop; corpus-wide instruction packaging and Alice
are unfinished. The ten held-out passages have not been used for candidate
selection or evaluation.

## The concrete defect

The preparation code treated predicted token durations as acoustic boundaries.
A source-only diagnostic compares high-frequency energy at the supposed centers
of fricatives and vowels. Across three fixed source groups, acoustic separation
peaks at offsets of -55, -65 and -65 ms. The original centers often fall in the
following sound. This is evidence of a systematic timing bias; it is not exact
forced alignment of every phone.

Moving cuts 1,500 samples earlier (62.5 ms at 24 kHz), with the original 700
training words, original one-candidate selection and unchanged native renderer,
reduces full development WER to 37.36%, CER to 16.25%. An offset-zero rebuild is
byte-identical to the shipped units, phone inventory, lexicon and revision. The
timing-only comparison therefore isolates a real extraction defect.

Timing alone remains insufficient. The native candidate additionally uses 267
connected verses from other chapters, keeps multiple candidate units, and selects
a path using phonetic context, word boundaries, duration and spectral join costs.
Adjacent selections from the same recording are copied continuously. Other joins
overlap matching phone centers with an offline phase search. No target audio or
target durations enter selection. All 50 benchmark chapters are excluded from
the connected source pool. Original 700-word batches supply rare-pair coverage.

## Results and practical limits

| Condition | Development passages | WER | CER |
|---|---:|---:|---:|
| Shipped compact bank | 40 | 118.20% | 102.83% |
| Same bank preparation with timing correction only | 40 | 37.36% | 16.25% |
| Connected-unit native candidate | 40 | 17.03% | 8.30% |
| Existing neural Björn reference | 40 | 5.27% | 1.77% |

The chapter-cluster bootstrap interval for candidate minus baseline WER is
approximately [-119.72, -87.39] percentage points. This quantifies improvement
over the failed baseline, not a naturalness guarantee or an unbiased estimate
of unseen-corpus quality. Development results were inspected during iteration.

For the three passages the user originally rejected, native candidate WER is
18.87% (1 Samuel 3:9–10), 8.70% (1 Kings 19:11–12), and 15.25%
(1 Corinthians 13:4–7). These are new native assembled waveforms, not the old
neural reference and not same-sentence reconstruction controls.

Playback needs a 90,715,200-byte mu-law source bank plus precomputed plans;
40 plans total 43,904 bytes. This is one experimental Björn bank, not an estimate
for two completed voices or a complete Bible distribution. It does not need an
ONNX model, Python, or neural inference at playback.

On Apple M4, median measured native synthesis time is 0.39 ms per passage,
maximum 1.12 ms; median file-open/setup time is 0.052 ms. These measurements use
local storage and exclude process launch, WAV encoding, GUI and audio hardware.
They do not establish startup latency on a ThinkPad X220i or Raspberry Pi 5.
The renderer seeks to selected spans and never loads the whole bank into memory.

Native output matches the Python reference within one 16-bit PCM unit on every
sample. Recognition was run on the native WAVs. Fifteen regression checks pass,
including timing preservation, punctuation/stress handling, native overlap
duration and invalid-plan rejection. The standalone CMake target builds with
warnings treated as errors on this Mac.

## What remains

Residual pronunciation errors, rhythm and pitch discontinuities remain. The
system preserves source prosody rather than predicting a coherent target contour.
More source data alone does not prove that these problems will disappear. A
listener must assess the actual candidate before product integration or further
size optimization. The next quality work should follow that listening evidence.

The code is original MIT code and uses the existing voice model's source speech,
with its Apache-2.0 license and attribution retained. No third-party synthesis
engine was imported. The existing neural startup investigation is historical.

[Reproduction and format](../../tools/speech/evaluation/unit-selection.md) ·
[Machine-readable measurements](speech-evaluation/compact-candidate.json)
