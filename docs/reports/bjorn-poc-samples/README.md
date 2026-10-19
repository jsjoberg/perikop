# Björn listening samples

Each lossless Apple Lossless / M4A file contains **Björn reference → first harmonic prototype → Björn Bas text-to-speech**, separated by 0.75 seconds of silence. The PCM is verified identical
after encoding and decoding. Download the files to listen in a local audio player;
these links do not require the development machine or tailnet server.

- [psalm comparison](psalm-comparison.m4a): Björn reference: 0.00 s, First harmonic prototype: 7.72 s, Björn Bas: text-to-speech: 15.45 s.
- [sjal comparison](sjal-comparison.m4a): Björn reference: 0.00 s, First harmonic prototype: 7.60 s, Björn Bas: text-to-speech: 15.20 s.
- [namn comparison](namn-comparison.m4a): Björn reference: 0.00 s, First harmonic prototype: 7.92 s, Björn Bas: text-to-speech: 15.85 s.

The last segment is synthesized from text using reusable units, not fitted to the complete recording. Its duration can differ from the reference. Listening review on 2026-10-19 found it largely unintelligible; the names clip was relatively better. This is a failed research experiment, unsuitable as a fallback. No eSpeak or GPL speech code was
used. The original renderer and fitting tools are MIT; the Björn teacher pack is
marked Apache-2.0 and its notices are included here. Source revisions, hashes and
measurements are in [report.json](report.json).

To regenerate on macOS after running the fitter:

```sh
python3 tools/speech/bjorn-poc/package_samples.py --compact-probe build/cmake/bin/ortho-compact-voice-probe
```

The encoder is Apple's built-in `afconvert`. It is only used to package this
audition, and is not a dependency of the native synthesizer.
