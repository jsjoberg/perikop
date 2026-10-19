> Experiment as of 2026-10-19. Listening review is still required.

# Fitting a small synthesizer to Björn

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
[bjorn-poc-samples](bjorn-poc-samples/README.md). Each contains the reference,
32-harmonic reconstruction and 64-harmonic reconstruction in that order.

## Initial local result

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

## What this establishes

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

## Provenance

The fitter and renderer are original Perikop MIT code. NumPy is an offline
BSD-3-Clause dependency. The teacher pack is marked Apache-2.0; the generated
audition retains its original license file and complete source manifest.
No implementation was copied from Mattias Gustavsson's library or eSpeak.
