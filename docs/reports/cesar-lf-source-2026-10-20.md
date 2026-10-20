# Cesar: controlled LF source experiment

2026-10-20. This adds an opt-in source experiment to the compact prosody milestone in `2a5bab6`. The default app voice and its cache revision remain unchanged. Native listening review is still required; this report makes no naturalness claim.

## Question and control

Can a different periodic vocal-fold excitation make the core voice less synthetic while preserving the reading's melody and word shapes?

The primary pair uses the compact melody from the latest listening milestone. A second pair uses the earlier accepted reference melody. Within each pair, source substitution preserves every pitch-control point, sound element, duration and pause position. Both readings retain 347,160 samples at 24 kHz, or 14.465 seconds. Formant frequencies, bandwidths, gains and transitions; noise generation and modulation; flutter; and all existing filters are held fixed.

Only the periodic glottal waveform changes. Listening copies then receive a constant gain to match EBU R128 integrated loudness. No equalization, compression, time stretching or new timing transfer is applied. RMS alone would not fully control the loudness difference caused by reshaping the spectrum.

## Source model

`lf_source.cpp` implements the normalized LF flow derivative, with an exponentially weighted sinusoidal open phase and an exponential return phase. Its shape is expressed directly as Tp, Te and Ta relative to one pitch period; it does not approximate an Rd-to-parameter conversion. Epsilon is solved for continuity of the return phase, and alpha for zero net cycle area. A 2,048-interval table avoids per-sample sine/exponential evaluations. The LF closure peak is normalized to the legacy source's approximate excitation scale; integrated loudness is matched after rendering.

The mathematical basis is the LF model of Fant, Liljencrants and Lin (1985), as restated in [Gobl (2021), equations 4–6](https://www.isca-archive.org/interspeech_2021/gobl21_interspeech.pdf). The first experimental shape uses Tp=0.40, Te=0.65 and Ta=0.04. These are explicit provisional settings, not measurements of Björn or a fitted model of Swedish phonation.

LF is evaluated on the same oversampled pitch-period clock as the legacy polynomial. Both use the existing source filter and four-sample average before output at 24 kHz. This is a practical table-based time-domain implementation, not the aliasing-free frequency-domain formulation in Gobl's paper.

Breath and aspiration noise deliberately retain their previous gating and amplitudes for this first experiment, including the legacy open-quotient control. Consequently this is not yet a coupled LF phonation model. Dynamic source quality, articulation, resonances and noise coupling remain separate later experiments.

## Reproduce

Build the probe, then run the source comparison with Python's standard library and ffmpeg on the PATH:

```sh
cmake --build build/cmake --target ortho-cesar-audio-probe ortho-cesar-voice-tests
uv run --locked tools/speech/cesar_source_comparison.py
```

`build/cesar-source-comparison/current.wav` and `lf.wav` are the level-matched compact-melody pair. `validation.json` records timing/pitch identity, loudness, headroom, LF parameters, hashes and rendering wall time. `.f32` files and `.raw.wav` files are unnormalized source outputs; `.timing.tsv` and `.pitch.tsv` allow independent control inspection. Raw source/filter output reaching the synthesizer limiter invalidates the comparison and makes the script fail.

To repeat against the earlier accepted reference melody:

```sh
uv run --locked tools/speech/cesar_source_comparison.py --reference-melody --output build/cesar-source-reference
```

For other passages, the native probe accepts `--source klatt` or `--source lf`, independently of `--compact` or `--pitch FILE`. The saved listening benchmark and its original WAVs are not modified.

## Initial validation

On the development Mac, both compact-melody listening files measure −21.0 LUFS. Neither source reaches the synthesizer limiter. The raw source outputs differ in level, which is why normalization is applied before listening. Sound timing and pitch traces are byte-identical within each pair, including all nine pause/tail entries.

Native tests cover cycle endpoints, continuity at closure, normalized closure excitation, nonnegative integrated flow returning to zero, invalid shape rejection, deterministic rendering, finite bounded speech, and preservation of sound timing and pitch controls. The original baseline/reference benchmark still reproduces its stored listening recordings within PCM rounding tolerance.

The motivation is supported by [Cabral et al. (2007)](https://www.isca-archive.org/ssw_2007/cabral07_ssw.html), who studied LF excitation as an alternative to a pulse train. Cesar's existing polynomial already has a shaped source, so their listening result does not predict the outcome of this particular comparison. Acceptance depends on native listening, especially whether less buzz comes at the expense of vowel clarity, Swedish character, or consonant audibility.

## Listening follow-up: muffling and clicks

The native listener's first response was “better but very muffled and clicku”. This is an improvement direction, not acceptance of the LF timbre. The initial LF shape and the app's default source remain unchanged so this feedback can be reproduced.

The follow-up comparison adds two independently inspectable variants:

- `lf-bright`: retain Tp=0.40 and Te=0.65, but shorten Ta from 0.04 to 0.008. This restores upper harmonics at the source without post-render EQ.
- `lf-clear`: the same shorter return, plus sample-wise interpolation of articulation targets between the existing 2.5 ms updates. Voicing and breath amplitudes follow those interpolated targets instead of updating only at pulse boundaries. Breath noise receives a sine-squared window over the LF open phase; noise modulation uses the same continuous window, with its depth fading continuously with voicing. Unvoiced frication retains full modulation gain.

The second variant therefore changes articulation envelopes and noise coupling as well as the periodic waveform. It retains the sound sequence and allocated durations, formant target tables, pitch clock, flutter and exact pitch controls. Interpolation takes the preceding control target to the current target over 2.5 ms: this rounds edges within the existing timeline rather than moving sound boundaries or time-stretching audio. These are provisional controls, not fitted Björn parameters.

```sh
uv run --locked tools/speech/cesar_source_comparison.py --refine --output build/cesar-source-refined
```

The output includes the original `current` and `lf` controls, plus `lf-bright` and `lf-clear`, each matched to −21 LUFS. The original two raw float renders remain bit-identical to the first source experiment on this machine. Every variant has 347,160 samples and byte-identical sound-duration/pause and pitch traces. None reaches the synthesizer limiter. The native probe also exposes `--lf-shape TP TE TA` and `--smooth-controls` for explicit reproduction.

As a spectral diagnostic, isolate the middle half of each vowel's allocated samples from the timing trace, filter the complete raw render through fourth-order Butterworth bandpasses at 24 kHz, then compare band energy in that mask to full-band vowel energy. This measures spectral balance, not perceived quality:

| Source | 500–1500 Hz | 1500–3000 Hz | 3000–6000 Hz |
| --- | ---: | ---: | ---: |
| Legacy polynomial | −6.4 dB | −21.4 dB | −31.4 dB |
| Initial LF | −14.0 dB | −38.3 dB | −54.0 dB |
| Shorter LF return | −9.6 dB | −27.5 dB | −40.8 dB |
| Shorter return + smooth controls | −9.6 dB | −27.4 dB | −40.8 dB |

The initial LF has substantially less vowel presence energy; shortening the return recovers about 11–13 dB in the upper two bands. This supports addressing the reported muffling at the excitation stage. It does not establish naturalness or an optimal setting.

The largest adjacent-sample steps in the first LF and legacy renders occur at the same /k/ burst and /s/ locations and have almost identical raw magnitudes. They are not evidence of a new discontinuity unique to LF. LF cycle continuity is also covered separately by native tests. Smoothing addresses abrupt control/noise gating as a plausible additional source of audible roughness, but it does not reduce the passage's maximum sample step. Consonant bursts and the listener's reported clicks still require listening review; this follow-up makes no claim that clicks are eliminated.
