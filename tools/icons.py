#!/usr/bin/env python3
"""
Rebuild every raster icon from the SVGs in assets/.

The SVGs are the only masters. assets/qzdl.svg is the application mark, and
everything else in assets/ plus the application icon in src/resources/Win32 is
rendered from it here; assets/filetypes.svg is the sheet the seven file type
icons are drawn on, and each of those is rendered out of it by the id of its
group. A change to a mark is a change to one file followed by a run of this
script.

Needs rsvg-convert and Pillow.
"""
import os
import struct
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MASTER = os.path.join(ROOT, "assets", "qzdl.svg")
FILETYPES = os.path.join(ROOT, "assets", "filetypes.svg")
WIN32 = os.path.join(ROOT, "src", "resources", "Win32")

# The sizes assets/ ships, and the ones an .ico carries.
PNG_SIZES = (32, 64, 128, 256, 512, 1024)
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)

# Every icns entry is a PNG these days; the pairs are (OSType, pixel size).
ICNS_ENTRIES = (
    (b"icp4", 16), (b"icp5", 32), (b"ic11", 32), (b"ic12", 64),
    (b"ic07", 128), (b"ic13", 256), (b"ic08", 256),
    (b"ic14", 512), (b"ic09", 512), (b"ic10", 1024),
)

# The groups in the sheet. Each is also the name qzdl.rc embeds it under, so
# the id in the SVG, the file on disk and the resource in the binary all read
# the same.
FILETYPES_IDS = ("arch", "arch_red", "cfg", "demo", "patch", "save", "wad")


def render(master, size, out, export_id=None):
    """One size of one mark, straight off the vector."""
    argv = ["rsvg-convert", "-w", str(size), "-h", str(size)]

    # Exporting a group crops to the ink in it, which is why every group in
    # the sheet carries a transparent rect the size of the frame.
    if export_id is not None:
        argv += ["--export-id", export_id]

    subprocess.run(argv + [master, "-o", out], check=True)


def icns(pngs, out):
    """An icns file is a header, a table of contents, and the PNGs in order."""
    entries = [(t, pngs[s]) for t, s in ICNS_ENTRIES]
    toc = b"TOC " + struct.pack(">I", 8 + 8 * len(entries))
    for kind, data in entries:
        toc += kind + struct.pack(">I", 8 + len(data))

    body = b"".join(k + struct.pack(">I", 8 + len(d)) + d for k, d in entries)
    with open(out, "wb") as f:
        f.write(b"icns" + struct.pack(">I", 8 + len(toc) + len(body)) + toc + body)


def ico(raster, out):
    """
    Handing over every size keeps Pillow from resampling one of them down into
    the rest, so each frame is still the vector rendered at that size.
    """
    frames = [Image.open(raster[s]).convert("RGBA") for s in ICO_SIZES]
    frames[-1].save(out, format="ICO", sizes=[(s, s) for s in ICO_SIZES],
                    append_images=frames[:-1])


def main():
    with tempfile.TemporaryDirectory() as tmp:
        # Rendered once per size from the vector, never resampled from each other.
        raster = {}
        for size in sorted({*PNG_SIZES, *ICO_SIZES, *(s for _, s in ICNS_ENTRIES)}):
            path = os.path.join(tmp, f"{size}.png")
            render(MASTER, size, path)
            raster[size] = path

        for size in PNG_SIZES:
            out = os.path.join(ROOT, "assets", f"qzdl-{size}.png")
            with open(raster[size], "rb") as src, open(out, "wb") as dst:
                dst.write(src.read())
            print("assets/qzdl-%d.png" % size)

        icns({s: open(raster[s], "rb").read() for _, s in ICNS_ENTRIES},
             os.path.join(ROOT, "assets", "qzdl.icns"))
        print("assets/qzdl.icns")

        ico(raster, os.path.join(WIN32, "ico_icon.ico"))
        print("src/resources/Win32/ico_icon.ico")

        for name in FILETYPES_IDS:
            frames = {}
            for size in ICO_SIZES:
                path = os.path.join(tmp, f"{name}-{size}.png")
                render(FILETYPES, size, path, export_id=name)
                frames[size] = path

            ico(frames, os.path.join(WIN32, f"ico_{name}.ico"))
            print("src/resources/Win32/ico_%s.ico" % name)


if __name__ == "__main__":
    sys.exit(main())
