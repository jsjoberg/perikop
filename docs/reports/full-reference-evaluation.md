# Numerical feedback against the original full-precision voice

The listener found v5 partly better but partly harder to hear. Its improved ASR
score did not establish an overall improvement. Synthesis changes are paused
while a reference-based error profile is established and validated.

## Frozen master

The master is the original Swedish `kokoro_sv.pth`, **327,216,523 bytes**
(312 MiB), at upstream revision
`2c7968d59c2fda1667e9e3ff0dd9967150a53f74`. Its SHA-256 is
`c3447596e1b333eac063b8f15ab242c541392fac1f8974f0429ba473222db660`.
Checkpoint floating tensors and inference parameters are checked to be FP32.
Inference uses the original PyTorch model with complex ISTFT, CPU, evaluation
mode, deterministic operations, seed 42 and the original Björn style tensor.
There is no FP16, INT8, autocast or neural compression in this master.

Forty development recordings are frozen as 24 kHz PCM WAVs. Raw float32 output
is also retained locally. The master applies the upstream production notch
filters at 2.4/4.8/7.2/9.6 kHz; it does not normalize recording peaks.
Existing exact input tokens/chunk splits are reused, rather than decoding a
compressed model's audio. Config, voice, checkpoint, tokens, code, dependency
versions and each waveform are recorded by hash. The ten held-out passages
are unused in metric design or candidate selection.

The earlier reference pack has a 166,237,948-byte ONNX acoustic file. Its export
script quantizes large decoder weights to INT8. That pack is now a candidate
against the FP32 master, not the master itself. Its measured gap also includes
export/ISTFT and stochastic-render differences; it is not a causal estimate of
quantization damage. A second seed of the original FP32 master on three fixed
passages supplies a provisional repeat-variation baseline. This work evaluates
Björn; Alice has not been evaluated by this suite.

## Signals and locations

No weighted score can let a pleasant pitch contour cancel phonetic degradation.
The independent text recognizer remains a separate intelligibility signal.
The new acoustic comparison emits these separate errors:

| Signal | Meaning | Better direction |
|---|---|---|
| Mel spectral convergence | Relative error of linear mel magnitudes; emphasizes energy-bearing spectral structure | Lower |
| STFT spectral convergence | Relative error of fine spectral magnitudes | Lower |
| Log-mel and log-STFT error, dB | Frequency detail, including weaker components | Lower |
| Cepstral envelope distance, dB | Low-order spectral-envelope mismatch | Lower |
| Spectral motion error | Mismatch of frame-to-frame spectral changes | Lower |
| F0 RMSE, semitones | Pitch error on jointly voiced frames | Lower |
| F0 contour correlation | Similarity of the contour's shape | Higher |
| Voicing disagreement | Voiced/unvoiced mismatch | Lower |
| Energy-envelope error, dB | Mismatched prominence/amplitude envelope after one global gain correction | Lower |
| Active-span duration and local timing error | Speech-span rate and uneven local rate relative to the master, excluding outer padding | Lower |
| Quiet-interval counts/duration errors | Extra/missing pauses and duration differences | Lower |

Every comparison also emits one-second reference windows with corresponding
candidate timestamps. This locates spectral, pitch and voicing errors for an
agent to inspect against unit-selection plans. Paired window deltas expose local
regressions even when an utterance-wide mean improves.

Alignment uses acoustic spectra, not predicted synthesis-token boundaries or
pitch targets. Frames use a 1024-sample Hann window, 480-sample hop, 64 triangular
mel bands from 50–11,000 Hz and orthonormal DCT coefficients 1–20. A fixed global
gain offset is measured and removed from spectral comparison. Silence at the
outer edges is trimmed for alignment. The timing decision uses the active-span
duration ratio, not raw total length. Original total duration remains visible
as descriptive metadata; reference edge padding must not penalize a shorter
candidate introduction or outro.
The cepstral number is this specified implementation's envelope distance; it is
not interchangeable with published SPTK/WORLD MCD scores.

Dynamic time warping uses a ±2-second normalized-time band and steps (1,1),
(1,2), (2,1), with a fixed two-dB penalty for non-diagonal steps. It cannot
repeat arbitrary frames to make a deletion disappear. Timing, duration and
quiet-interval errors are reported independently so warping does not excuse
staccato speech. Failures to align or find active audio are explicit failures,
never zero errors. Quiet intervals are energy-based estimates, not phoneme
annotations. REAPER estimates F0 independently from each rendered waveform;
octave/tracking errors remain possible and jointly voiced coverage is reported.

Spectral convergence and log-magnitude loss are established components of
waveform synthesis objectives, for example
[Parallel WaveGAN](https://arxiv.org/abs/1910.11480). Here they are single-resolution,
time-aligned diagnostics, not a reproduction of its training loss or a MOS
predictor. Separating spectral, F0 and timing distortion follows the distinction
studied in [Inukai et al., 2013](https://www.phontron.com/paper/inukai13ssw.pdf).
Simple spectral distances cannot be assumed to predict pleasantness:
[Ijima et al., 2016](https://www.isca-archive.org/interspeech_2016/ijima16_interspeech.html)
demonstrate limitations of mel-cepstral distortion as a subjective-quality proxy.

## Validation before optimization

Five fixed development masters supply 70 controlled comparisons: identity,
polarity reversal, −6 dB gain, 30/15 dB SNR noise, 1.5/3.5 kHz lowpass, inserted
300 ms silence, repeated staccato gaps, a muted active window, an actual
three-semitone pitch shift, flattened pitch, silence and different text spoken
by the same voice. Fixed expectations check physical invariance, sensitivity
and direction. These are signal controls, not invented human quality labels.

The numerical review reports paired passage-bootstrap deltas and provisional
repeat-variation allowances. A better pitch score cannot cancel a spectral
regression. Local spectral regressions and ASR regressions are retained. A
default five-passage minimum is an explicit workflow policy, not a power
guarantee. Three repeat renders provide provisional noise allowances, not
perceptual just-noticeable differences. No candidate is certified pleasant by
this gate; eligibility means only that broader numerical evaluation is justified.

The suite is also run on the shipped compact voice, v2, rejected v3 variants,
improved-but-staccato v4 and mixed-result v5. Human feedback is kept separately
and used to audit whether the signals reveal the observed tradeoffs, not to
retrofit an arbitrary composite score.

The current human-preference audit finds an important disagreement: several
mean errors favour rejected v3 over listener-preferred v4. These distances are
not a validated naturalness ranking. The v4→v5 comparison does expose a tradeoff:
better average spectral errors and three local spectral regressions. An initial
total-duration warning was corrected to exclude the master's outer silence;
the decision uses active speech spans instead. This is diagnostic evidence, not proof that those windows
caused the listener's complaint. No blind synthesis change is justified by a
single mean distance. A complete perceptual ranking remains an open calibration
problem, while the separate component errors can already locate deviations.

## Reproduce and use for the next iteration

`master_reference.py` requires the existing export environment's PyTorch,
Kokoro and SciPy. Model files are explicit local paths and offline mode can be
enabled with `HF_HUB_OFFLINE=1`. Run it with `--help` for the original checkpoint,
config, original voice tensor, frozen token receipts and output arguments.
`master.json` and per-recording receipts support exact-input resumption.

Subsequent comparisons need only NumPy and the already pinned REAPER executable.
Prepare a JSON manifest with a `pairs` list; each entry supplies `id`, `condition`,
`audio` (absolute or relative to the working directory) and `audio_sha256`.
Optional `asr_wer`/`asr_identity` fields carry already measured intelligibility.

```sh
python tools/speech/evaluation/reference_compare.py \
  --master build/master-fp32 --manifest build/reference-experiments.json \
  --output build/full-reference-evaluation --reaper ../reaper-build/reaper \
  --pair compact-v4 compact-v5
python tools/speech/evaluation/reference_gate.py \
  --report build/full-reference-evaluation/report.json \
  --before compact-v4 --after compact-v5 \
  --output build/full-reference-evaluation/v4-v5-review.json
python tools/speech/evaluation/reference_validation.py prepare \
  --master build/master-fp32 --reaper ../reaper-build/reaper \
  --output build/reference-validation-controls
python tools/speech/evaluation/reference_compare.py \
  --master build/master-fp32 --manifest build/reference-validation-controls/manifest.json \
  --output build/full-reference-validation --reaper ../reaper-build/reaper
python tools/speech/evaluation/reference_validation.py check \
  --comparison build/full-reference-validation/report.json \
  --output build/full-reference-validation/validation.json
```

`signals.csv` is the iteration table; `error-windows.csv` locates defects;
`report.json` retains full provenance, alignments, summaries and paired deltas.
Features and pair results are cached by input, code and extractor identities.
The optional figure in `reference_report.py --plot` additionally needs Matplotlib;
the JSON, CSV and Markdown reports do not.
For each new trial, change one synthesis mechanism, compare the same passages,
inspect the error dimensions and their worst windows, then retain or revert the
trial. Broaden promising trials before requesting another listening assessment.
