# Perikop

Perikop is an offline Orthodox lectionary reader for Windows, macOS, and Linux.
It has a Swedish interface, Scripture in Swedish, Greek, and English, and Swedish read-aloud.
Daily readings follow the North American Antiochian Greek tradition.
The application is a prototype.

## Build

Use CMake 3.24 or later and a C++23 compiler with `std::expected`:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
```

See the [build guide](docs/building.md) for platform requirements, the voice pack, and run commands.

## Documentation

- [User guide](docs/using-perikop.md)
- [Documentation index](docs/README.md): development, releases, data sources, and known limits.
- [Third-party notices](THIRD-PARTY-NOTICES.md)
