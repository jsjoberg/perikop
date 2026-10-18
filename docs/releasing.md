# Releases

See the [update migration plan](updating.md) for platform validation and user-controlled in-app updates.

## Release

Set the version in `project()` in `CMakeLists.txt`, then push a matching tag:

```sh
git tag v0.1.0-alpha.2 && git push origin v0.1.0-alpha.2
```

CI builds, tests, and packages five targets: Windows x64, Linux x64, Linux ARM64, macOS Apple Silicon, and macOS Intel.
It then creates a draft release containing the existing five native downloads, Velopack installers/portable packages, a full update package and channel feed for each target, deltas where a previous package exists, and `SHA256SUMS.txt` covering every uploaded file.
Releases with AppImages also carry `AppImage-runtime-source-<commits>.tar.gz`, listed in `SHA256SUMS.txt`: the corresponding source of the AppImage runtime, which statically links libfuse (LGPL-2.1). Both Linux jobs make it with `tools/appimage_runtime.py source` from pinned inputs, and `update_packages.py assemble` refuses AppImages without it; see [the report](reports/appimage-runtime-lgpl-2026-10-18.md).
A tag suffix such as `-alpha.2` goes into the file names. CI stops if the tag does not match `CMakeLists.txt`.
Write the release notes on GitHub, then publish the draft.
The updater cannot see a draft. Release-channel feeds use `*-releases`; nightly feeds use `*-nightly`. The release channel excludes GitHub prereleases.

## Nightly builds

Nightlies run at 03:15 Stockholm time when the default branch has changed since the last successful nightly.
They publish a `nightly-<date>-<commit>` prerelease, and CI keeps the latest seven.
Each nightly includes the platform packages that passed their checks.
Its notes list available platforms, unavailable platforms, and the sanitizer result.
Failed jobs still fail the workflow, but do not discard successful packages from other platforms.

Incomplete nightlies do not prevent another scheduled attempt for the same commit.
Retries on the same day add the run ID and attempt number to the release tag.
If all platforms fail, CI publishes no nightly.
Version-tagged releases still require all five targets and sanitizers to pass.

Every successful platform contributes its native download, Velopack download, full package, optional delta, and `releases.<channel>.json` feed together. A platform whose packaging or update test fails contributes neither a native download nor a feed. Full packages remain available for first upgrades, interrupted delta chains, and recovery.

Before offline compilation/packaging, `tools/ci/update_packages.py restore` downloads the newest earlier full package in the same channel from published GitHub releases and verifies its SHA-256 and size against the feed. Packing uses that base with `--delta BestSpeed`. The first package in a channel has no delta. Restoring the base is an explicit network step; builds and packing remain offline. macOS packing also needs `zstd` on the runner.

Each published feed includes the current full package, its new delta, and the previous full package's metadata. Previous full packages are not uploaded again by each build. Historical deltas are excluded from the new feed, because the pinned SDK combines feeds from multiple releases and would otherwise try the same patch twice.

Cleanup reads every feed in the seven retained nightlies. If a referenced package exists only on an older release, it downloads and hash-checks that package, uploads it to the new nightly, updates `SHA256SUMS.txt`, and confirms every checksummed file is present before deleting old nightly releases and tags. Missing or conflicting feed/checksum data and failed package or checksum uploads stop cleanup before any release is deleted. An interrupted attempt that uploaded a base without its checksum is verified and repaired on retry. Cleanup does not delete version releases, drafts, or dependency snapshots. Keeping seven nightlies preserves room for release discovery within the SDK's ten-release API window.

To start a nightly now:

```sh
gh workflow run build.yml --ref main -f nightly=true
```

For an unpublished packaging check on a branch, run `gh workflow run build.yml --ref <branch>` without `nightly=true`. It uploads CI artifacts and creates no GitHub release.

## Local packages

[Build](building.md) with a voice pack, then:

```sh
cpack --config build/cmake/CPackConfig.cmake -B build/package   # macOS, Windows
tools/package_appimage.sh build/cmake                           # Linux
```

On Linux, `tools/package_velopack.sh build/cmake <version> linux-<arch>-<track> <output>` then packs the same AppDir as a Velopack AppImage and update package. It needs .NET 8, `squashfs-tools`, and `vpk`, which `dependencies.py restore` unpacks to `build/ci/tools/vpk`.

The Windows installer needs NSIS on the `PATH` and `-DPERIKOP_MSVC_RUNTIME` set to a `Microsoft.VC143.CRT` redistributable folder.
The macOS DMGs are separate `macOS-arm64.dmg` (Apple Silicon) and `macOS-x86_64.dmg` (Intel) downloads.
For Velopack, stage with `cmake --install build/cmake --prefix build/stage --strip`, then run
`python3 tools/package_velopack_macos.py build/stage/Perikop.app <version> <arm64|x86_64> osx-<arm64|x64>-<releases|nightly> <output>`.
It needs .NET 8, `zstd` for deltas, and the restored `vpk`. Native CI also creates a [portable update validation kit](updating.md#phases) for each architecture; the macOS 13 baseline check remains pending. Pass `--delta BestSpeed` after seeding the output directory with the previous same-channel full package and feed. The Linux packer accepts `PERIKOP_VELOPACK_DELTA=BestSpeed` for the same purpose.
The AppImage needs `dpkg-dev` on Debian and Ubuntu. Build release AppImages in the CI container: CI rejects dependencies newer than glibc 2.34.

## CI dependency snapshots

CI never downloads from upstream servers. It uses snapshot archives published as `ci-dependencies-<commit>` prereleases in [jsjoberg/perikop-assets](https://github.com/jsjoberg/perikop-assets) and pinned in `ci/dependencies-lock.json`.
Snapshots and voice packs are kept out of this repository's releases because the in-app updater reads only the newest releases here.
To update a dependency:

1. Edit `ci/upstreams.json` and the native dependency declaration, or `ci/linux.Dockerfile`. Commit and push.
2. Prepare a snapshot from a commit or branch. Add `-f reuse_environments=true` for a source-only change, or `-f macos_tools_only=true` for macOS tools only.
   The workflow in `jsjoberg/perikop-assets` runs this repository's `dependencies.yml` and publishes the snapshot there.

   ```sh
   gh workflow run dependencies.yml --repo jsjoberg/perikop-assets -f ref=main
   ```

3. Download the new lock file, then commit and push it:

   ```sh
   gh release download ci-dependencies-<commit> --repo jsjoberg/perikop-assets --pattern dependencies-lock.json --dir ci --clobber
   ```

### Voice pack

Set `id` to the `id` in the pack's `voice-pack.json`, then upload it on macOS:

```sh
id=kokoro-sv-alice-bjorn-2c7968d-v3
COPYFILE_DISABLE=1 tar --no-mac-metadata -czf "build/$id.tar.gz" -C build/kokoro-pack .
gh release create "$id" "build/$id.tar.gz" --repo jsjoberg/perikop-assets --prerelease --title "Voice pack $id" --notes "Prepared Alice and Björn voice pack."
cmake -E sha256sum "build/$id.tar.gz"
```

Then update the `voice` entry in `ci/upstreams.json` and prepare a new dependency snapshot.
