#!/usr/bin/env python3
"""ETCH -- by Synth Centric.  Build release/ETCH_Source.zip from the
repository sources (everything needed to reproduce the ROM; build output,
release binaries and local emulator binaries are excluded)."""
import os
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXCLUDE_DIRS = {".git", "build", "release", "__pycache__", os.path.join("tests", "sameboy", "bin")}


def files():
    for dp, dns, fns in os.walk(ROOT):
        rel = os.path.relpath(dp, ROOT)
        dns[:] = sorted(d for d in dns if os.path.normpath(os.path.join(rel, d)) not in EXCLUDE_DIRS and d not in EXCLUDE_DIRS)
        for f in sorted(fns):
            if f.endswith((".pyc", ".sav", ".bad")):
                continue
            yield os.path.normpath(os.path.join(rel, f))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "release", "ETCH_Source.zip")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    n = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in files():
            info = zipfile.ZipInfo(os.path.join("ETCH", f), date_time=(2026, 11, 10, 0, 0, 0))
            info.external_attr = (0o755 if os.access(os.path.join(ROOT, f), os.X_OK) else 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(os.path.join(ROOT, f), "rb") as fh:
                z.writestr(info, fh.read())
            n += 1
    print("wrote %s (%d files)" % (out, n))


if __name__ == "__main__":
    main()
