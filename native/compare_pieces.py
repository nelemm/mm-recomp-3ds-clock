"""Checks that every platform's library cut the same pieces from the test atlas.

usage: python compare_pieces.py <reference folder> <other folder>...
Each folder holds the NN_WxH.rgba files written by clock3ds_textures_test. Sizes must match exactly.
Pixels may differ slightly because floating-point rounding differs between compilers, so the check is
on what can be seen: alpha, and colour weighted by alpha (a nearly transparent pixel's colour can vary
more without any visible difference).
"""
import os
import sys

TOLERANCE = 2  # out of 255


def load(folder):
    return {name: open(os.path.join(folder, name), "rb").read()
            for name in sorted(os.listdir(folder)) if name.endswith(".rgba")}


def visible_difference(a, b):
    worst = 0.0
    for i in range(0, len(a), 4):
        worst = max(worst, abs(a[i + 3] - b[i + 3]))
        for c in range(3):
            worst = max(worst, abs(a[i + c] * a[i + 3] - b[i + c] * b[i + 3]) / 255)
    return worst


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
        worst = max(visible_difference(ref[n], pieces[n]) for n in ref)
        print(f"{folder}: 46 pieces, max visible difference {worst:.2f}/255")
        ok &= worst <= TOLERANCE
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main(*sys.argv[1:])
