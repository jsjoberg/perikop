# Release packages

## Package formats

Each package contains the application, its resources, and the Alice and Björn voices. It works offline.

- macOS: `Perikop-0.1.0-macOS-arm64.dmg`. Drag Perikop to Applications.
- Windows: `Perikop-0.1.0-windows-x64.exe`. The installer puts Perikop in Program Files and adds a Start menu shortcut.
- Linux: `Perikop-0.1.0-x86_64.AppImage`. Make the file executable, then run it.

## Make a package

Prepare the [voice pack](kokoro-voices.md#prepare-the-pack).
Then [build Perikop](building.md).

On macOS or Windows, make the package:

```sh
cpack --config build/cmake/CPackConfig.cmake -B build/package
```

On Windows, the installer needs NSIS on the `PATH`.
ONNX Runtime needs the Visual C++ runtime, so the installer carries it.
Set `-DPERIKOP_MSVC_RUNTIME` to the `Microsoft.VC143.CRT` folder of a Visual Studio redistributable during CMake setup.
For example, use `VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT`.

On Linux, make the AppImage:

```sh
tools/package_appimage.sh build/cmake
```

The script downloads pinned linuxdeploy tools and checks their SHA-256 hashes. They copy GTK and the other shared libraries into the image.
The GTK plugin needs `dpkg-dev` on Debian and Ubuntu.
The AppImage runs on distributions with the glibc of its build system or later. Ubuntu 24.04 needs glibc 2.39.

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

CI makes the packages for version tags `v*` and for manual runs. Manual runs keep them as build artifacts only.
CI keeps compiled objects in a ccache cache between runs. To make the cache usable, it builds wxWidgets without precompiled headers.
It downloads the prepared voice pack from a release of this repository and checks its SHA-256 hash.
To publish a new pack, set `id` to the `id` in its `voice-pack.json`.
Then archive and upload it on macOS:

```sh
id=kokoro-sv-alice-bjorn-2c7968d-v1
COPYFILE_DISABLE=1 tar --no-mac-metadata -czf "build/$id.tar.gz" -C build/kokoro-pack .
gh release create "$id" "build/$id.tar.gz" --prerelease --title "Voice pack $id" --notes "Prepared Alice and Björn voice pack."
cmake -E sha256sum "build/$id.tar.gz"
```

Then set `VOICE_PACK` and `VOICE_PACK_SHA256` in `.github/workflows/build.yml`.
