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
