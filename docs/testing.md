# Tests

Run the tests after the [build](building.md):

```sh
ctest --test-dir build/cmake --output-on-failure
```

The GUI smoke test needs a desktop session. On Linux without one, prefix the command with `xvfb-run -a`.
For core tests only, configure a separate build with `-DORTHO_BUILD_GUI=OFF`.
For ASan and UBSan, configure a separate build with `-DORTHO_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug`.

Diagnostic modes of the app (on Windows and Linux, use `build/cmake/bin/perikop`). They use a temporary profile:

```sh
build/cmake/bin/Perikop.app/Contents/MacOS/Perikop --smoke-test --screenshot /absolute/path/reader.png
build/cmake/bin/Perikop.app/Contents/MacOS/Perikop --render-benchmark
```

The benchmark reports frame preparation and reading-open times. Compare them only on the same computer and build.

Python checks:

```sh
uv run --locked tests/icon_test.py
python3 tests/macos_tidy_test.py
```

## Raspberry Pi 3, 4 and 5

The shared ARM64 AppImage targets these models on 64-bit Raspberry Pi OS Trixie (Debian 13) with a desktop. The ARMv8-A build baseline is checked by `tests/linux_abi_test.py`; glibc checks also run on packaged libraries. These build checks do not establish physical-device performance or audio compatibility. Pi 3/4 have not yet been validated on hardware.

Before describing a Pi model as hardware-tested, record its exact model, RAM, OS, package version and results for:

1. Confirm `dpkg --print-architecture` reports `arm64`, then launch the published AppImage in the desktop session.
2. Open daily readings and the Bible picker; scroll, resize and change the text size. Check that slower storage or limited RAM does not make reading unusable.
3. Install `libespeak-ng1`; test Swedish voice preview, read-aloud, pause/resume, speed changes and stopping playback through the actual audio output.
4. Install a subsequent full update and delta using a separate test profile. Confirm saved bookmarks, settings, reading progress and positions survive.

The [official Raspberry Pi OS documentation](https://www.raspberrypi.com/documentation/computers/os.html#architecture) identifies Pi 3, 4 and 5 as 64-bit-capable models. This verifies the OS architecture choice, not Perikop's runtime behavior on those boards.
