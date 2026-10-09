#!/bin/sh
set -eu
cmake -S . -B build/cmake "$@"
cmake --build build/cmake --parallel
