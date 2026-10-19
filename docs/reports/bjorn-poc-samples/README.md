# Björn listening samples

Each lossless Apple Lossless / M4A file contains **Björn reference → 32 harmonics →
64 harmonics**, separated by 0.75 seconds of silence. The PCM is verified identical
after encoding and decoding. Download the files to listen in a local audio player;
these links do not require the development machine or tailnet server.

- [psalm comparison](psalm-comparison.m4a): Björn reference: 0.00 s, 32 harmonics: 7.72 s, 64 harmonics: 15.45 s.
- [sjal comparison](sjal-comparison.m4a): Björn reference: 0.00 s, 32 harmonics: 7.60 s, 64 harmonics: 15.20 s.
- [namn comparison](namn-comparison.m4a): Björn reference: 0.00 s, 32 harmonics: 7.92 s, 64 harmonics: 15.85 s.

These are complete-utterance reconstructions, not a reusable text-to-speech voice.
The samples have not been approved by listening. No eSpeak or GPL speech code was
used. The original renderer and fitting tools are MIT; the Björn teacher pack is
marked Apache-2.0 and its notices are included here. Source revisions, hashes and
measurements are in [report.json](report.json).

To regenerate on macOS after running the fitter:

```sh
python3 tools/speech/bjorn-poc/package_samples.py
```

The encoder is Apple's built-in `afconvert`. It is only used to package this
audition, and is not a dependency of the native synthesizer.
