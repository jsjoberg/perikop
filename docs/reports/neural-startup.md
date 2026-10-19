# Historical neural startup investigation

The user subsequently clarified that the active task is making the compact voice
sound good and run efficiently. This investigation is retained as evidence; it
does not define the current direction. See [compact candidate](compact-candidate.md).

## Current user requirements

- Pleasant, good speech is mandatory. Performance and size cannot justify an unacceptable voice.
- Performance and startup delay matter on examples such as ThinkPad X220i and Raspberry Pi 5; improving compact voice quality is the immediate task.
- Thirty MB was an estimate, not a requirement.
- Neural inference is not excluded categorically; candidate components and models must meet the project’s license requirements. Specific acceptable licenses were not enumerated.
- The agent-written starting prompt is not authoritative. Keep verified measurements and listening evidence; reassess architecture independently.

## Verified implementation facts

- The existing neural pack is approximately 179 MB, with a 166 MB acoustic model shared by both voices.
- The voice worker already retains its neural model between readings and has a persistent PCM cache.
- The current packed vocoder uses stored int8 weights that dequantize to floating point. Reduced storage is not evidence of accelerated integer inference.
- The existing buffer gate deliberately waits for more audio when measured synthesis is slower than playback. For long readings this can create substantial startup delay; it is not safe to remove the gate without checking underruns.

## Local worker measurements

The following measurements are from this Apple M4 machine. They do not establish X220i or Pi 5 performance. The real worker, frontend, model, cache and gate run unchanged, with build-copy tracing. Hardware playback is replaced with a headless 24 kHz clock; GUI, driver and physical audio-device latency are excluded. The default four-thread setting is shown.

| Case | Model loading | Generation | Audio queued | First frame above -50 dBFS RMS |
|---|---:|---:|---:|---:|
| cold_model_empty_cache | 0.451 s | 0.884 s | 1.338 s | 2.191 s |
| warm_model_new_text | — | 0.814 s | 0.816 s | 1.629 s |
| cached_replay | — | — | 0.002 s | 0.858 s |

Cached replay PCM was bit-identical to its original generated PCM in every tested thread configuration. No application weights or playback policy were changed. These are single runs, not stable latency percentiles.

An analysis of all 50 existing neural recordings found a median 0.87 seconds before a frame exceeded the defined active-energy threshold. This is **not all digital silence**: the Genesis sample contains nonzero soft signal from roughly 0.20 seconds. It would be unsafe to label the entire 0.87 seconds disposable padding or trim at an energy threshold without checking phonetic content and listening quality.

## Next decisions

1. Measure cold generation, warm generation, cached replay and sustained throughput on the actual X220i/Pi 5. Test thread counts sequentially; four was fastest in this local run, which does not justify a setting for the target machines.
2. Prioritize retaining/preparing the existing voice and PCM ahead of demand where practical. Cache playback preserves the waveform exactly, but background preparation needs careful cancellation and must not delay a newly selected passage.
3. If target hardware cannot synthesize comfortably faster than playback, investigate actual inference acceleration and quality-checked graph/model optimization. Replacing a pleasant voice with an unacceptable lightweight one does not meet the requirement.
4. Inspect the low-energy lead-in as an independent latency component. Preserve initial speech and breath; a lower onset number alone is not a quality improvement.

## Portable probe

`neural-startup-probe-source.zip` contains the probe and the required Perikop sources. It builds against existing ONNX Runtime, Sonic and SQLite. It makes no downloads, opens no audio device and writes only a new cache directory. See `tools/speech/latency/README.md` inside the archive. Target-device execution remains outstanding; no claim of a production speedup is made.
