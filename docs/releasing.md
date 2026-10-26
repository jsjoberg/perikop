# Releases

See the [update migration plan](updating.md) for platform validation and user-controlled in-app updates.

## Release

From 0.2.1, downloads and updates use separate public repositories:

| Repository | Assets |
| --- | --- |
| `jsjoberg/perikop` | Versioned installers/AppImages, `SHA256SUMS.txt`, and compatibility packages when older updaters need them |
| `jsjoberg/perikop-updates` | Velopack feeds, full packages, deltas, portable archives, corresponding AppImage runtime source archives, and checksums |
| `jsjoberg/perikop-assets` | CI dependency snapshots only |

The pinned [Velopack 1.2.161 GitHub source](https://github.com/velopack/velopack/blob/1.2.161/src/lib-rust/src/sources/github.rs) requests only ten releases, **before** filtering prereleases. Ten dependency snapshots can hide every application feed. Sharing the assets repository would require coordinated snapshot and application retention or a different discovery source; that redesign is not part of the current publication implementation. Keep both application repositories free of dependency snapshots and unrelated releases; publication refuses unrelated tags.

### One-time setup

1. Create **public** `jsjoberg/perikop-updates`, initialized with a README on a `main` branch. Clients use anonymous GitHub access; a private repository will not work. Do not move or remove any `v0.2.0` assets.
2. Create a fine-grained PAT restricted to **only** `jsjoberg/perikop-updates`, with **Contents: read and write** and the default metadata permission. Save it in `jsjoberg/perikop` as the Actions secret **`PERIKOP_UPDATES_TOKEN`**. An expired/missing token fails publication. Rotate it before expiry; never embed credentials in the application. The main repository still uses its job-scoped `GITHUB_TOKEN` with `contents: write`; that token cannot publish across repositories.
3. Allow this credential to create tags/releases and Git commit objects in the update repository. Each update tag gets a new commit recording its source SHA, using the README tree without changing `main`. This avoids tying every storage release to the date of the repository's initial commit.
4. Run an unpublished packaging check, then publish a migration nightly and `v0.2.1` with the procedure below. A complete migration nightly establishes the bridge on all four nightly feeds. Partial nightlies establish it only for platforms that have successfully built the new updater; later publications carry these feeds forward.

### Stage and publish

Keep the version bump in a separate release-preparation commit: update `project()` in `CMakeLists.txt` and the tooling version in `pyproject.toml`/`uv.lock`, then push the matching version tag. The migration starts at `0.2.1`:

```sh
git tag v0.2.1 && git push origin v0.2.1
```

CI builds, tests and packages Windows x64, Linux x64, Linux ARM64 and macOS Apple Silicon (macOS 15+). Tag builds require all four targets and both sanitizer jobs. It creates **verified drafts in both repositories**. Main download names include the version, for example:

- `Perikop-win-x64-0.2.1-Setup.exe`
- `Perikop-osx-arm64-0.2.1-Setup.pkg`
- `Perikop-linux-x64-glibc239-0.2.1.AppImage`
- `Perikop-linux-arm64-glibc239-0.2.1.AppImage`

The ARM64 AppImage is shared by Raspberry Pi 3, 4 and 5 running **64-bit Raspberry Pi OS Trixie (Debian 13) with a desktop**, with glibc 2.39 or later. Its filename and update channel stay model-independent. Generated release and nightly notes state the OS requirement, separate eSpeak NG installation, and that Pi 3/4 hardware performance and audio remain unverified. See the [Pi validation checklist](testing.md#raspberry-pi-3-4-and-5).

`releases` remains an internal Velopack channel name. The program version is embedded during compilation, and CI rejects a tag whose base version differs from `CMakeLists.txt`. Renaming the download leaves the embedded updater metadata intact.

Review the draft notes, preserving the generated download names and hidden migration marker. Publish the pair **with this command**. The separate promotion workflow shares the publication concurrency group with nightly staging and uses the configured scoped credentials:

```sh
gh workflow run publish.yml --repo jsjoberg/perikop --ref main -f tag=v0.2.1
```

Do not publish the main draft using GitHub's Publish button: it bypasses paired verification. Promotion downloads every remote asset again, checks checksum coverage and hashes, verifies every feed's referenced package, and checks platform coverage and compatibility-feed visibility. It publishes the update release first and the main release last. Main stable releases are marked Latest; internal releases are not. Paired publication is not an atomic GitHub operation: nightly cleanup runs before promotion, so if the last step fails, the main release remains a draft while the complete update release is available and obsolete nightlies have already been removed. Retained feeds and their referenced packages remain available. Rerun the same promotion command after fixing the failure; public assets are never overwritten.

The initial main `0.2.1` release necessarily includes four full packages and four release feeds for installed `0.2.0` clients. These feeds contain only their newest full package: migration has a full fallback even if every intermediate delta was skipped. New clients use the separate update repository. Removing the bridge after one release would strand users who update later. Main stable bridges are therefore refreshed at least every third stable release, leaving room for seven nightlies inside the SDK's ten-release window; other stable releases normally have the four primary downloads plus checksums. Nightly bridges are refreshed when their feeds would leave the retained window. The notes explain which files ordinary users should choose. Keep generated compatibility assets and markers until a separately designed migration retires the old SDK source.

Linux channels retain `-glibc239`. Never publish a new AppImage to the older Linux feeds. The actual `v0.2.0` release already uses the supported feeds and can update normally. Earlier retired-channel builds require a one-time download on a supported host; see [the compatibility boundary](updating.md#versions-and-channels).

### Recovery and verification

Missing packages, feeds, source archives, platform installers, mismatched hashes or failed uploads stop publication. A partial upload stays in a draft. Rerunning staging repairs the same draft when its expected files match; unexpected draft assets require inspection instead of silent deletion. A failed nightly gets a unique retry tag, including when only the update draft exists. If a stable draft waits long enough for its bridge references to age out, promotion refuses it: rerun the tag workflow to restage it before promoting. If refreshed feeds change the draft asset set, inspect and remove only unexpected draft assets before rerunning; the tool refuses to delete them silently. Never delete a published full package to repair a feed; restore the original hash-verified bytes or publish a corrected higher version. Never modify `v0.2.0` during this migration.

Before first deployment, run the four-platform packaging workflow and test an actual installed `0.2.0` copy against a reviewed migration publication, then test a subsequent version with `0.2.1`. Local publication fixtures model GitHub discovery and downloaded bytes; they do not establish a native end-to-end migration. See [update validation](updating.md#packaging-and-validation).

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

Every successful platform contributes its download to the main repository and its full package, optional delta, and `releases.<channel>.json` feed to the update repository together. A platform whose packaging or update test fails contributes none of them. Full packages remain available for first upgrades, interrupted delta chains, and recovery.

Before offline compilation/packaging, `tools/ci/update_packages.py restore` downloads the newest earlier full package in the same channel from the update repository and the read-only legacy main repository, and verifies its SHA-256 and size against the feed. Packing uses that base with `--delta BestSpeed`. The first package in a channel has no delta. Restoring the base is an explicit network step; builds and packing remain offline. macOS packing also needs `zstd` on the runner.

Each published feed includes the current full package, its new delta, and the previous full package's metadata. The verified previous full package is included in the new update release too, so a feed never depends on a base outside the SDK's discovery window. Historical deltas are excluded from the new feed, because the pinned SDK combines feeds from multiple releases and would otherwise try the same patch twice.

Before promoting a new nightly, cleanup keeps six existing nightlies in each repository, leaving room for the new seventh without a temporary eleven-release window. Bridge planning uses that same projected retention. Cleanup reads every nightly feed in retained releases and rescues hash-verified packages needed by those feeds before deleting old releases/tags. It updates and checks `SHA256SUMS.txt` before deletion. Missing or conflicting feed/checksum data and failed rescue uploads stop deletion; interrupted rescues can be repaired on retry. Version releases, drafts and dependency snapshots are never deleted by nightly cleanup.

Partial nightlies carry the most recent full feed/package for missing platforms in internal storage; they do not claim those platforms passed the current build. Feeds carried from earlier builds omit historical deltas to avoid applying a patch twice when the SDK combines feeds. Release and nightly feed names remain separate. Corresponding runtime source archives are verified and carried forward too, so deleting an old nightly does not remove its only source copy. Main release notes link to their source availability in the update repository; older runtime revisions remain downloadable there.

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
Snapshots are kept out of both application release repositories because installed updaters read only ten releases.
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

