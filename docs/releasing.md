# Release packages

## Package formats

Each package contains the application, its resources, and the Alice and Björn voices. It works offline.

- macOS: `Perikop-0.1.0-macOS-universal.dmg`. One app for Intel and Apple Silicon. Drag Perikop to Applications.
- Windows: `Perikop-0.1.0-windows-x64.exe`. The installer puts Perikop in Program Files and adds a Start menu shortcut.
- Linux: `Perikop-0.1.0-x86_64.AppImage`. Make the file executable, then run it.

## Make a package

Prepare the [voice pack](kokoro-voices.md#prepare-the-pack).
Then [build Perikop](building.md).

On macOS or Windows, make the package:

```sh
cpack --config build/cmake/CPackConfig.cmake -B build/package
```

On Windows 10 or later, x86-64, the installer needs NSIS on the `PATH`.
ONNX Runtime needs the Visual C++ runtime, so the installer carries it.
Set `-DPERIKOP_MSVC_RUNTIME` to the `Microsoft.VC143.CRT` folder of a Visual Studio redistributable during CMake setup.
For example, use `VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT`.

Windows product versions include the full release suffix, such as `0.1.0-alpha.2`.
The executable, installer, and Installed Apps entry use the same version as the About page.
Windows also stores a numeric version, such as `0.1.0.0`, for system comparisons.
The executable embeds its ICO and the interface's PNG. Installed resources have no icons folder.
macOS keeps its ICNS in the application bundle for the operating system.

On Linux, make the AppImage:

```sh
tools/package_appimage.sh build/cmake
```

The script downloads pinned linuxdeploy tools and checks their SHA-256 hashes. They copy GTK and the other shared libraries into the image.
The GTK plugin needs `dpkg-dev` on Debian and Ubuntu.
Linux CI uses a saved Rocky Linux 9 environment with GCC Toolset 15 and a glibc 2.34 baseline.
The AppImage carries GCC's C++ runtime. Users need no compiler or separate GCC runtime update.
It uses the host's glibc. CI rejects ELF dependencies above glibc 2.34 in the build and assembled AppDir.
This includes libraries and AppRun added by linuxdeploy.
Packages built on newer local systems can fail this check. Use the CI container for release builds.
Enterprise Linux 9 and Ubuntu 22.04 or later are compatibility targets.
Desktop and read-aloud tests on those systems still need validation.

The packages go to `build/package`.
The macOS application has an ad-hoc signature only. The Windows installer has no signature.
The [user guide](using-perikop.md#installation) describes first launch.

## Release

Set the version in `project()` in `CMakeLists.txt`, then push a tag with the same version:

```sh
git tag v0.1.0-alpha.2 && git push origin v0.1.0-alpha.2
```

CI builds and tests all three platforms and makes the packages.
It then creates a draft release with the packages and `SHA256SUMS.txt`.
A suffix in the tag, such as `-alpha.2`, is added to the file names. The About page shows the tag version.
CI stops if the tag does not match the version in `CMakeLists.txt`.
Edit the release notes on GitHub, then publish the draft. It becomes the latest release on the repository page.

## CI packages

CI makes the packages for version tags `v*`, manual runs, and nightly runs.
Ordinary manual runs keep them as build artifacts only.
Manual runs with the `nightly` option use the scheduled nightly process, including publication and cleanup.

To start that process now, run:

```sh
gh workflow run build.yml --ref main -f nightly=true
```

Nightly builds start at 05:00 Stockholm time, with automatic daylight saving changes.
They use the latest commit on the default branch.
GitHub can delay the start when its runners are busy.
Before the builds, CI compares this commit with the last successful nightly.
If the commit is unchanged, CI skips all three builds. Failed builds retry on the next night.

After all three platforms pass their checks, CI publishes a dated prerelease, such as `nightly-2026-10-11-abcdef0`.
The commit suffix permits multiple builds per day without replacing an earlier download.
Each prerelease contains the packages, their SHA-256 hashes, and a link to the source commit.
Nightlies do not replace the latest stable release.
The schedule starts after the workflow reaches the default branch.

After publication, CI keeps the latest seven successful nightlies and deletes older nightly releases, their packages, and their tags.
This count gives seven versions for comparison, even across weeks without changes.
Cleanup leaves version releases, other prereleases, drafts, and the voice pack intact.
Temporary nightly artifacts expire after one day. Other runs use the repository's default artifact retention.

CI keeps compiled objects in a ccache cache between runs. To make the cache usable, it builds wxWidgets without precompiled headers.

## CI dependency snapshots

Ordinary CI builds use GitHub runners and dependencies stored in this repository's releases.
They do not contact Chocolatey, Homebrew, distribution mirrors, SQLite, or other upstream download servers.
The snapshot includes native sources, voices, Windows GCC and NSIS, macOS analysis tools, Linux system libraries, and AppImage tools and runtime.
The runner supplies its preinstalled operating system, Xcode or Visual C++ runtime, CMake, Python, Git, and archive tools.

`ci/dependencies-lock.json` pins each snapshot archive by SHA-256.
CI checks both cached archives and downloads before it uses them.
If the cache is empty or evicted, CI downloads the same pinned archive from this repository's release.
It never falls back to upstream servers.
Missing or damaged dependencies stop the build.
Linux disables container networking. macOS denies network access during compilation, tests, analysis, and packaging.

Dependency preparation is a separate, manually triggered workflow. It is the only CI workflow that contacts upstream servers.
For a dependency update, edit `ci/upstreams.json` and the corresponding native dependency declaration.
For a Linux environment update, edit `ci/linux.Dockerfile`.
Commit and push those changes before preparation.

Start preparation:

```sh
gh workflow run dependencies.yml --ref main
```

For a macOS analysis-tool update only, reuse the pinned sources and Linux environment:

```sh
gh workflow run dependencies.yml --ref main -f macos_tools_only=true
```

For a source or packaging-input update, reuse both tool environments:

```sh
gh workflow run dependencies.yml --ref main -f reuse_environments=true
```

After all preparation jobs pass, the workflow publishes a `ci-dependencies-<commit>` prerelease.
It contains all snapshot archives and a generated lock file.
Dependency releases do not expire with the nightly retention policy.

Download the generated lock file:

```sh
gh release download ci-dependencies-<commit> --pattern dependencies-lock.json --dir ci --clobber
```

Commit and push `ci/dependencies-lock.json` to activate the snapshot.
Then start a manual package build to check all three platforms.

### Update the voice pack

To publish a new pack, set `id` to the `id` in its `voice-pack.json`.
Then archive and upload it on macOS:

```sh
id=kokoro-sv-alice-bjorn-2c7968d-v3
COPYFILE_DISABLE=1 tar --no-mac-metadata -czf "build/$id.tar.gz" -C build/kokoro-pack .
gh release create "$id" "build/$id.tar.gz" --prerelease --title "Voice pack $id" --notes "Prepared Alice and Björn voice pack."
cmake -E sha256sum "build/$id.tar.gz"
```

Then update the `voice` entry in `ci/upstreams.json` and prepare a new dependency snapshot.
