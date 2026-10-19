# Neural speech startup probe

Goal: retain the pleasant existing voice and measure startup/throughput separately.
Thirty MB is not a requirement. Target examples are ThinkPad X220i and Raspberry
Pi 5. Local Apple M4 results do not predict either device's performance.

The probe runs the **real portable speech worker**, text frontend, neural model,
cache and buffer gate. It replaces only the hardware output with a simulated
24 kHz playback clock, emitting no sound. It exercises cold model/empty cache,
warm model/new text and cached replay. It prints separate loading, generation,
cache-write, buffer and signal-onset timestamps.

No application source or voice resources are modified. CMake instruments copies
in its build directory and fails if expected source anchors change. The model's
intra-operation thread count can be varied experimentally in the copied source;
the application default remains four. All other model settings and weights are
unchanged. Thread-count changes are not guaranteed to produce identical floats.

## Build using existing dependencies

Use the same ONNX Runtime and Sonic sources as the application, and SQLite >=
3.37. The C++23 compiler requirement is the application's existing requirement.
No new speech engine, model or download is introduced by this build.

```sh
cmake -S tools/speech/latency -B build/speech-latency \
  -DCMAKE_BUILD_TYPE=Release \
  -DORT_ROOT=/path/to/onnxruntime \
  -DSONIC_ROOT=/path/to/sonic-source
cmake --build build/speech-latency --parallel 2
build/speech-latency/speech-latency /path/to/resources/voices \
  build/latency-4-001 4 > build/latency-4-001.jsonl
python3 tools/speech/latency/analyze.py build/latency-4-001.jsonl \
  --output build/latency-4-001-summary.json
```

The voices directory contains `kokoro-sv-alice-bjorn-2c7968d-v3/`, with the
existing prepared pack. Use fresh cache directories: existing directories are
never removed. Capture stderr separately when diagnosing a failed run.
The output executable is `speech-latency.exe` on Windows.

Repeat sequentially with 2 and 1 threads and new directories, **not concurrently**.
Repeat measurements to estimate variation. A new directory empties the application
PCM cache, not the operating system's file cache. A "cold model" here means a new
inference session, not a cold disk or fresh OS boot.

The probe saves appended PCM as `case-1.f32`, `case-2.f32`, `case-3.f32` in the run
cache directory. Case 1 and cached replay (case 3) should be bit-identical. These
files permit external quality checks without timing file writes during generation.
The in-memory capture copy and tracing add a small amount of measurement overhead.

## Interpretation and limits

- `append` is queued audio; it is not necessarily released by the gate.
- `gate_released` means the stream can drain; it is not physical audibility.
- `first_nonzero_gt_1e-5` includes very soft signal/noise.
- `first_frame_rms_gt_minus50dbfs` is a 10 ms energy threshold, not a perceptually
  validated speech-onset detector. Do not trim audio automatically at this point.
- Audio-device opening, hardware buffering, the GUI and click handling are absent.
  The probe measures the worker, not complete button-to-speaker latency.
- The two short sentences do not test long-reading prebuffer policy, sustained
  thermal throttling, battery behavior or memory pressure. Slower-than-realtime
  inference can legitimately require a large lead for uninterrupted long playback.
- Cached replay is a real PCM-cache hit, not another neural generation. The
  working voice already retains its model between readings; this is not a proposed
  new optimization.

Profile the intended machines before selecting production thread counts or
changing the gate. Preserve listening quality as the acceptance requirement.
Quantization must be distinguished from compressed weight storage: the current
vocoder's stored int8 weights are dequantized to float for inference, so their
smaller file representation does not itself establish integer-compute speedups.
