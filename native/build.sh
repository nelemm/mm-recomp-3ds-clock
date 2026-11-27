#!/bin/sh
# Builds the native library on Linux (clock3ds_textures.so) or macOS (clock3ds_textures.dylib, universal).
# Output in build/, to copy next to the .nrm in the mods folder.
# "build.sh test" also builds build/clock3ds_textures_test (writes every piece as raw RGBA).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../build"
mkdir -p "$OUT"
CXX="${CXX:-c++}"
FLAGS="-O2 -std=c++17 -fvisibility=hidden -Wall"

case "$(uname -s)" in
    Darwin)
        ARCHS="-arch arm64 -arch x86_64 -mmacosx-version-min=11.0"
        $CXX $FLAGS $ARCHS -dynamiclib "$HERE/clock3ds_textures.cpp" -o "$OUT/clock3ds_textures.dylib"
        [ "$1" = "test" ] && $CXX $FLAGS $ARCHS -DCLOCK3DS_TEST "$HERE/clock3ds_textures.cpp" -o "$OUT/clock3ds_textures_test"
        ;;
    *)
        # libstdc++ is linked statically so the library doesn't depend on the system's version.
        $CXX $FLAGS -fPIC -shared -static-libstdc++ -static-libgcc "$HERE/clock3ds_textures.cpp" -o "$OUT/clock3ds_textures.so"
        [ "$1" = "test" ] && $CXX $FLAGS -DCLOCK3DS_TEST "$HERE/clock3ds_textures.cpp" -o "$OUT/clock3ds_textures_test"
        ;;
esac
echo OK
