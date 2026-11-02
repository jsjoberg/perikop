# Releases

See the [update migration plan](updating.md) for platform validation and user-controlled in-app updates.

## Release

| Where | Assets |
| --- | --- |
| `jsjoberg/perikop` releases | `Perikop-Setup.exe`, `Perikop-Windows-Portable.zip`, `Perikop.pkg`, `Perikop-macOS-Portable.zip`, `Perikop-x86_64.AppImage`, `Perikop-aarch64.AppImage`, the AppImage runtime source, and `SHA256SUMS.txt` |
| `jsjoberg/perikop-assets`, release `updates` | Every channel's `releases.<channel>.json` feed, its full package and recent deltas, and feed entries staged for a release draft |
| `jsjoberg/perikop-assets`, other releases | CI dependency snapshots |

Installed copies read `https://github.com/jsjoberg/perikop-assets/releases/download/updates/releases.<channel>.json` through Velopack's HTTP source, and download packages from the same place. The download names have no version, so `https://github.com/jsjoberg/perikop/releases/latest/download/Perikop-Setup.exe` always gives the newest release.

### One-time setup

Create a fine-grained PAT restricted to **only** `jsjoberg/perikop-assets`, with **Contents: read and write**. Save it in `jsjoberg/perikop` as the Actions secret **`PERIKOP_ASSETS_TOKEN`**. The job's own `GITHUB_TOKEN` cannot write to another repository. Rotate the PAT before it expires; an expired or missing token stops publication. The first publication creates the `updates` release.

### Stage and publish

Keep the version bump in a separate release-preparation commit: update `project()` in `CMakeLists.txt` and the tooling version in `pyproject.toml`/`uv.lock`, then push the matching version tag:

```sh
git tag v0.2.1 && git push origin v0.2.1
```

CI builds, tests and packages Windows x64, Linux x64, Linux ARM64 and macOS Apple Silicon (macOS 15+). Tag builds require all four targets and both sanitizer jobs. CI rejects a tag whose base version differs from `CMakeLists.txt`. The release job then:

1. Uploads the new full packages and deltas to the `updates` release, and the feed entries for the release as `<tag>.releases.<channel>.json`. Installed copies do not read these staged entries.
2. Creates a draft in `jsjoberg/perikop` with the plain-named downloads. Its notes list the files for each system and the Raspberry Pi requirements.

Review the draft and publish it with GitHub's **Publish release** button. Publishing runs the **Publish updates** workflow (`.github/workflows/publish.yml`). It downloads the staged entries and their packages, checks each package's size and SHA-256, and merges the entries into the live feeds. It then deletes the staged entries and every package that no live or staged feed names. Each feed keeps its newest full package and the deltas of the seven newest versions.

`v0.2.1` alone also carries its feeds and full packages and deltas on the `jsjoberg/perikop` release. 0.2.0 copies read releases through Velopack's GitHub source, find 0.2.1 there, and after the update use the new HTTP source. 0.2.0 copies that skip 0.2.1 until it leaves the ten newest releases must be reinstalled once. Remove this one-time copy, marked `shortcut:` in `tools/ci/update_packages.py` and `.github/workflows/build.yml`, after 0.2.1.

Linux channels retain `-glibc239`. Never publish a new AppImage to the older Linux feeds; earlier retired-channel builds require a one-time download on a supported host. See [the compatibility boundary](updating.md#versions-and-channels).

### Recovery and verification

A missing package, feed or installer, a wrong hash, or a failed upload stops publication. A rerun of the release job reuses the uploaded packages only when their bytes match; it never replaces a package, because a live feed may already name it. If the bytes differ, delete the draft. On the `updates` release, delete the `<tag>.releases.*.json` entries and that version's `.nupkg` files, then rerun. Never delete a package that a live `releases.<channel>.json` names. If the **Publish updates** workflow fails, fix the cause and run it again:

```sh
gh workflow run publish.yml --repo jsjoberg/perikop -f tag=v0.2.1
```

Replacing a feed deletes and uploads it again; a check in that moment fails and succeeds when the user tries again.

Before first deployment, test an installed `0.2.0` copy against the published 0.2.1 release, then an update from 0.2.1 through the HTTP source. Local fixtures do not establish either. See [update validation](updating.md#packaging-and-validation).

## Nightly builds

Nightlies run at 03:15 Stockholm time when the default branch has changed since the last successful nightly.
They publish a `nightly-<date>-<commit>` prerelease, and CI keeps the latest seven.
Each nightly includes the platform packages that passed their checks.
Its notes list available platforms, unavailable platforms, and separate macOS and Linux sanitizer results.
Failed jobs still fail the workflow, but do not discard successful packages from other platforms.

Incomplete nightlies do not prevent another scheduled attempt for the same commit.
Retries on the same day add the run ID and attempt number to the release tag.
If all platforms fail, CI publishes no nightly.
Version-tagged releases and manual publication checks require all four targets and both sanitizer jobs to pass.

Every successful platform contributes its downloads to the nightly release, and its full package, optional delta and feed entry to the `updates` release, together. A platform whose packaging or update test fails contributes none of them. A nightly makes its feed entries live as soon as its release exists; there is no draft. CI then deletes all but the seven newest nightly releases. The `updates` release keeps the packages, so this removes downloads only.

Before offline compilation/packaging, `tools/ci/update_packages.py restore` downloads the newest earlier full package in the same channel and verifies its SHA-256 and size against the feed. It reads the `updates` release first, then the `jsjoberg/perikop` releases, which held the packages before 0.2.1. Packing uses that base with `--delta BestSpeed`. The first package in a channel has no delta. Restoring the base is an explicit network step; builds and packing remain offline. macOS packing also needs `zstd` on the runner.

Nightly copies installed before 0.2.1 read nightlies through the GitHub source, so they no longer receive updates: install a new nightly once.

To start a nightly now:

```sh
gh workflow run build.yml --ref main -f nightly=true
```

For an unpublished packaging check on a branch, run `gh workflow run build.yml --ref <branch>` without `nightly=true`. It uploads CI artifacts and creates no GitHub release.

## Local packages

[Build](building.md), then:

Every package is a Velopack package. They need .NET 8 and `vpk`, which `dependencies.py restore` unpacks to `build/ci/tools/vpk`.

On Linux, `tools/prepare_appdir.sh build/cmake` prepares the AppDir, and `tools/package_velopack.sh build/cmake <version> linux-<arch>-glibc239-<track> <output>` packs it as the AppImage and update package. Packing also needs `squashfs-tools`.

On Windows, stage with `cmake --install build/cmake --prefix build/stage`, then run `vpk pack` as the Windows job in `.github/workflows/build.yml` does.
On macOS, stage with `cmake --install build/cmake --prefix build/stage --strip`, then run
`python3 tools/package_velopack_macos.py build/stage/Perikop.app <version> arm64 osx-arm64-<releases|nightly> <output>`.
It needs `zstd` for deltas. Native CI creates an ARM64 [portable update validation kit](updating.md#packaging-and-validation). The macOS 15 baseline update check must pass on the macOS 15 runner before release; results from newer macOS do not establish the minimum. Pass `--delta BestSpeed` after seeding the output directory with the previous same-channel full package and feed. The Linux packer accepts `PERIKOP_VELOPACK_DELTA=BestSpeed` for the same purpose.
The AppImage needs `dpkg-dev` on Debian and Ubuntu. Build release AppImages in the Debian 13 CI container: CI rejects dependencies newer than glibc 2.39, the version in Enterprise Linux 10.

## CI dependency snapshots

Application dependencies never fall back to upstream servers during ordinary CI. They use snapshot archives published as `ci-dependencies-<commit>` prereleases in [jsjoberg/perikop-assets](https://github.com/jsjoberg/perikop-assets) and pinned in `ci/dependencies-lock.json`.
The separate Python tooling bootstrap downloads pinned uv/Python before the offline build.
Snapshots are kept out of `jsjoberg/perikop`, whose ten newest releases 0.2.0 copies read.
To update a dependency:

1. Edit `ci/upstreams.json` and the native dependency declaration, or `ci/linux.Dockerfile`. Commit and push.
2. Prepare a snapshot from a commit or branch. Add `-f reuse_environments=true` for a source-only change, or `-f macos_tools_only=true` for macOS tools only. Every new snapshot must include the Windows cross-build environment; reuse modes need it in the pinned snapshot.
   The workflow in `jsjoberg/perikop-assets` runs this repository's `dependencies.yml` and publishes the snapshot there.

   ```sh
   gh workflow run dependencies.yml --repo jsjoberg/perikop-assets -f ref=main
   ```

3. Download the new lock file, then commit and push it:

   ```sh
   gh release download ci-dependencies-<commit> --repo jsjoberg/perikop-assets --pattern dependencies-lock.json --dir ci --clobber
   ```

