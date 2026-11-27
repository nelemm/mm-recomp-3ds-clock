"""Checks that every platform's library cut the same pieces from the test atlas.

usage: python compare_pieces.py <reference folder> <other folder>...
Each folder holds the NN_WxH.rgba files written by clock3ds_textures_test. Sizes must match exactly;
pixels may differ by a few levels (floating-point rounding differs between compilers).
"""
import os
import sys

TOLERANCE = 2


def load(folder):
    return {name: open(os.path.join(folder, name), "rb").read()
            for name in sorted(os.listdir(folder)) if name.endswith(".rgba")}


def main(reference, *others):
    ref = load(reference)
    assert len(ref) == 46, f"{reference}: {len(ref)} pieces instead of 46"
    ok = True
    for folder in others:
        pieces = load(folder)
        if sorted(pieces) != sorted(ref):
            print(f"{folder}: piece names/sizes differ from {reference}")
            ok = False
            continue
        worst = max(max((abs(a - b) for a, b in zip(ref[n], pieces[n])), default=0) for n in ref)
        print(f"{folder}: 46 pieces, max difference {worst}")
        ok &= worst <= TOLERANCE
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main(*sys.argv[1:])
