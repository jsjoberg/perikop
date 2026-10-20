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
