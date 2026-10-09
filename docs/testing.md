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
It uses a temporary user database. It does not change personal settings.

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

The benchmark uses a temporary user database and does not change personal settings.
It reports median and 99th-percentile frame preparation times for Swedish text and two parallel modes.
Warm measurements compare direct text drawing with cached tiles at the same two scroll positions.
First-visit measurements include layout and raster cache misses.
The benchmark also reports cache construction counts, retained pixel bytes, and settled pause activity.

The measurements use an offscreen bitmap at the window display scale.
They exclude screen presentation, compositor delay, input latency, and speech synthesis.
They do not measure end-to-end frame rate or fan activity.
Compare timings on the same computer, build, viewport, and display scale.
Timings are diagnostic results, not fixed pass thresholds.

## Offline speech probe

Generate real offline Swedish audio with Alice without audio playback:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --speech-probe /absolute/path/voice.wav
```

Use the corresponding executable on Windows or Linux.
This test requires the pinned local voice pack. It rejects token-limit truncation, invalid samples, and silent audio.

## Resource checks and results

The [corpus guide](corpus.md#rebuild-and-check-resources) describes resource preparation and resource checks.
The [validation record](validation.md) contains dated local results and outstanding platform checks.

The icon checks use only Python's standard library:

```sh
uv run --locked tests/icon_test.py
```

They check the ICO and ICNS sizes, PNG payloads, and rejection of invalid PNG headers.
