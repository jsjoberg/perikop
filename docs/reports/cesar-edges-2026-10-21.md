# Cesar pulse and control edges

2026-10-21. Native feedback clarifies the references: Alice and Björn are better sound-quality references than Piper; Swedish eSpeak is clearer than Cesar and lacks the clicking the listener hears in Cesar. The description of the clicks is sufficient to motivate a synthesis-level comparison. Their cause has not yet been established.

The earlier peak limiter changed maximum amplitude but did not establish a clear audible benefit. Lower peaks are not a substitute for removing artifacts within speech. This experiment retains the clear original polynomial source and compact Swedish melody and tests two specific edge mechanisms, rather than returning to the muffled LF timbre.

## Inspectable mechanisms

The polynomial source recurrence ends its open phase at approximately `-(8960 + 26880/open)`, then resets to zero. That is an actual discontinuity in the excitation. The experimental `smooth_closure` option adds a small polynomial step residual around that closure, centered between adjacent oversampled samples. It extends one output sample on each side of the edge. The correction has opposing positive/negative area; the original source waveform away from closure is retained.

The background technique is polynomial BLEP correction around waveform discontinuities, discussed by [Välimäki, Pekonen and Nam (2012)](https://mac.kaist.ac.kr/pubs/ValimakiPeknenNam-jasa2012.pdf). This is a short approximate edge correction combined with the existing four-times oversampling and source filter, not an alias-free oscillator or evidence that the listener's particular clicks were aliasing.

Separately, the old render path updates formant coefficients and gains every 2.5 ms, and updates voice amplitudes at pitch-period boundaries. The existing opt-in `smooth_controls` experiment interpolates physical targets at each output sample. It now supports the original polynomial source as well as LF. Breath noise and voiced noise modulation use a continuous sine-squared open-phase window instead of a rectangular gate. For the original source, that window follows its original open fraction; no LF waveform is used.

Physical control interpolation moves from the preceding target to the current target within the existing 2.5 ms slot. This alters articulation envelopes; identical sound allocation does not imply sample-identical acoustic onset times. The experiment does preserve all allocated phone durations, pauses and pitch controls.

Both options remain off by default. `smooth_closure` rejects LF rather than silently applying a polynomial-specific correction to it. The earlier LF control renders remain bit-identical locally, including the existing LF/smooth-controls combination.

## Controlled listening comparison

```sh
cmake --build build/cmake --target ortho-cesar-audio-probe ortho-cesar-voice-tests
uv run --locked tools/speech/cesar_edge_comparison.py
```

The script renders four conditions for all seven Swedish test cases: original, closure correction alone, control smoothing alone, and both (`edges`). It checks finite output, no source/filter safety-clamp samples, unchanged sample count, byte-identical duration/pause and pitch traces, and matched integrated loudness. A constant gain produces −21 LUFS listening copies. No output limiter, EQ, resampling, time stretching, changed vowel target or neural acoustic model is involved.

The unfamiliar-prose pair is `build/cesar-edge-comparison/prose/{original,edges}.wav`. The same filenames occur under `psalm`, `quantity`, `swedish-sounds`, `accent`, `names` and `forced-phone`. The Psalm remains 347,160 samples (14.465 seconds). The original condition reproduces the preceding clear compact render bit for bit as raw float audio. The earlier baseline and accepted reference-melody WAVs also continue to reproduce within their established PCM rounding tolerance.

## Diagnostics and limits

Using the same middle-vowel band-energy diagnostic as the source experiments:

| Psalm condition | 500–1500 Hz | 1500–3000 Hz | 3000–6000 Hz |
| --- | ---: | ---: | ---: |
| Original clear source | −6.42 dB | −21.37 dB | −31.43 dB |
| Closure correction only | −6.43 dB | −21.54 dB | −32.11 dB |
| Control smoothing only | −6.41 dB | −21.34 dB | −31.43 dB |
| Both | −6.42 dB | −21.50 dB | −32.10 dB |

The combined change loses less than 0.7 dB in these presence bands, rather than the large losses of the rejected initial LF source. This checks against repeating that specific muffling problem; it is not proof of unchanged perceived clarity.

For unfamiliar prose, the maximum raw adjacent-sample step falls from 0.1146 to 0.1038 (about 9%). For the Psalm it instead rises from 0.0964 to 0.1006; the largest steps still occur in consonant regions. Neither statistic identifies the listener's clicks or demonstrates their removal. Native listening remains necessary.

On this Mac the 14.465-second combined rendering took about 0.034 seconds including probe-process overhead after initial loading. The added controls remain a tiny, non-neural experiment. This is not a new cross-platform performance benchmark.

All 14 non-GUI tests, native formatting, changed-file clang-tidy and the application build passed. Native tests cover deterministic edge rendering, finite bounded output, preservation of pitch/timing and rejection of an incompatible LF/closure-control combination. Original default and reference fixtures and all four preceding source controls are unchanged. The [diagnostic record](data/cesar-edges-2026-10-21.json) records the ablations, hashes and checks. This candidate is unreviewed; it is not the app's default voice or a claimed fix for the audible clicks.
