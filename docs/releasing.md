# Releases

## Release

Set the version in `project()` in `CMakeLists.txt`, then push a matching tag:

```sh
git tag v0.1.0-alpha.2 && git push origin v0.1.0-alpha.2
```

CI builds, tests, and packages all four platforms, then creates a draft release with the packages and `SHA256SUMS.txt`.
A tag suffix such as `-alpha.2` goes into the file names. CI stops if the tag does not match `CMakeLists.txt`.
Write the release notes on GitHub, then publish the draft.

## Nightly builds

Nightlies run at 03:15 Stockholm time when the default branch has changed since the last successful nightly.
They publish a `nightly-<date>-<commit>` prerelease, and CI keeps the latest seven.
Each nightly includes the platform packages that passed their checks.
Its notes list available platforms, unavailable platforms, and the sanitizer result.
Failed jobs still fail the workflow, but do not discard successful packages from other platforms.

Incomplete nightlies do not prevent another scheduled attempt for the same commit.
Retries on the same day add the run ID and attempt number to the release tag.
If all platforms fail, CI publishes no nightly.
Version-tagged releases still require all four platforms and sanitizers to pass.

To start a nightly now:

```sh
gh workflow run build.yml --ref main -f nightly=true
```

## Local packages

[Build](building.md) with a voice pack, then:

```sh
cpack --config build/cmake/CPackConfig.cmake -B build/package   # macOS, Windows
tools/package_appimage.sh build/cmake                           # Linux
```

The Windows installer needs NSIS on the `PATH` and `-DPERIKOP_MSVC_RUNTIME` set to a `Microsoft.VC143.CRT` redistributable folder.
The AppImage needs `dpkg-dev` on Debian and Ubuntu. Build release AppImages in the CI container: CI rejects dependencies newer than glibc 2.34.

## CI dependency snapshots

CI never downloads from upstream servers. It uses snapshot archives published as `ci-dependencies-<commit>` prereleases and pinned in `ci/dependencies-lock.json`.
To update a dependency:

1. Edit `ci/upstreams.json` and the native dependency declaration, or `ci/linux.Dockerfile`. Commit and push.
2. Prepare a snapshot. Add `-f reuse_environments=true` for a source-only change, or `-f macos_tools_only=true` for macOS tools only.

   ```sh
   gh workflow run dependencies.yml --ref main
   ```

3. Download the new lock file, then commit and push it:

   ```sh
   gh release download ci-dependencies-<commit> --pattern dependencies-lock.json --dir ci --clobber
   ```

### Voice pack

Set `id` to the `id` in the pack's `voice-pack.json`, then upload it on macOS:

```sh
id=kokoro-sv-alice-bjorn-2c7968d-v3
COPYFILE_DISABLE=1 tar --no-mac-metadata -czf "build/$id.tar.gz" -C build/kokoro-pack .
gh release create "$id" "build/$id.tar.gz" --prerelease --title "Voice pack $id" --notes "Prepared Alice and Björn voice pack."
cmake -E sha256sum "build/$id.tar.gz"
```

Then update the `voice` entry in `ci/upstreams.json` and prepare a new dependency snapshot.
