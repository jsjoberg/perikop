# Tests

## Native tests

Run the core, storage, speech, and GUI smoke tests after the [build](building.md):

```sh
ctest --test-dir build/cmake --output-on-failure
```

The GUI smoke test needs a graphical desktop session.
If Linux has no desktop session, use Xvfb:

```sh
xvfb-run -a ctest --test-dir build/cmake --output-on-failure
```

For core tests without a GUI, use `-DORTHO_BUILD_GUI=OFF` in a separate build directory.

## GUI smoke test

The GUI smoke test loads the fonts and draws each theme and parallel mode.
It uses a temporary profile for settings, pronunciation decisions, and the speech cache.
It does not use personal overrides or change personal settings.

To save reader images, add `--screenshot /absolute/path/reader.png` to the application smoke-test command:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --smoke-test --screenshot /absolute/path/reader.png
```

On Windows or Linux, use the corresponding executable.
The paragraph test compares 39 native layouts with an exhaustive word-boundary oracle.
It also checks a layout that differs from greedy wrapping.
The smoke test compares cached text with direct text at two display scales.
It also checks tile reuse, the memory limit, and the absence of repeated frames during settled buffering.

## Reader benchmark

Run the reader benchmark on the target computer:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --render-benchmark
```

On Windows, run the corresponding executable:

```powershell
.\build\cmake\bin\perikop.exe --render-benchmark
```

The benchmark uses a temporary profile for every writable database.
It does not use personal overrides or change personal settings.
It reports median and 99th-percentile frame preparation times for Swedish text and two parallel modes.
Warm measurements compare direct text drawing with cached tiles at the same two scroll positions.
First-visit measurements include layout and raster cache misses.
The benchmark also reports cache construction counts, retained pixel bytes, and settled pause activity.
It times opening a reading and separates first-visit text fetching and fragment preparation from paragraph typesetting.
It also times opening Psalms, Jeremiah, and Isaiah with Swedish and Greek panes.
It times speech queue preparation for all of John without loading a voice model or playing audio.
The fetching measurement includes verse mapping and string construction; it does not isolate SQLite execution.

The measurements use an offscreen bitmap at the window display scale.
They exclude screen presentation, compositor delay, input latency, and speech synthesis.
They do not measure end-to-end frame rate or fan activity.
Compare timings on the same computer, build, viewport, and display scale.
Timings are diagnostic results, not fixed pass thresholds.

The speech worker tests hold audio device initialization while exercising pause and progress polling.
They also check paused preparation, resume, cancellation, and worker delivery of pronunciation results.
GUI checks cover late pronunciation results after another word, clearing the panel, and destroying the panel.

## Offline speech probe

Generate real offline Swedish audio with Alice and Björn without audio playback:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --speech-probe /absolute/path/voice.wav
```

Use the corresponding executable on Windows or Linux.
This test requires the pinned local voice pack. It rejects token-limit truncation, invalid samples, and silent audio.
The probe generates a short phrase and text longer than one voice context for each voice.
The supplied file contains Alice's short phrase. Three adjacent WAV files contain the other results.
The output records the pack identity, architecture, audio duration, and generation time.
Release checks use the assembled package resources on each platform.

When a voice pack is available, CTest also runs `speech-cache-recovery` with real inference and a simulated audio device.
It injects cache opening, reading, writing, and lock failures and checks that personal data remains intact.

## Sanitizers

For ASan and UBSan, configure a separate native build with `-DORTHO_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug`.
Run the complete CTest suite from that directory.
The flags cover SQLite, core code, Kokoro adapters, the speech worker, Sonic, tempo tests, layout code, and the native UI.
The prebuilt ONNX Runtime and third-party GUI libraries remain outside this instrumentation.
The release workflow runs these checks on macOS arm64 before publication.

## Resource checks and results

The [corpus guide](corpus.md#rebuild-and-check-resources) describes resource preparation and resource checks.
The [validation record](validation.md) contains dated local results and outstanding platform checks.

The icon checks use only Python's standard library:

```sh
uv run --locked tests/icon_test.py
```

They check the ICO and ICNS sizes, PNG payloads, and rejection of invalid PNG headers.
