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

# Group ids in the sheet, which are also the names qzdl.rc embeds them under.
FILETYPES_IDS = ("arch", "arch_red", "cfg", "demo", "patch", "save", "wad")


def render(master, size, out, export_id=None):
    """One size of one mark, straight off the vector."""
    argv = ["rsvg-convert", "-w", str(size), "-h", str(size)]

    # Exporting a group crops to the ink in it, which is why every group in
    # the sheet carries a transparent rect the size of the frame.
    if export_id is not None:
        argv += ["--export-id", export_id]

    subprocess.run(argv + [master, "-o", out], check=True)


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
        for size in sorted({*PNG_SIZES, *ICO_SIZES}):
            path = os.path.join(tmp, f"{size}.png")
            render(MASTER, size, path)
            raster[size] = path

        for size in PNG_SIZES:
            out = os.path.join(ROOT, "assets", f"qzdl-{size}.png")
            with open(raster[size], "rb") as src, open(out, "wb") as dst:
                dst.write(src.read())
            print("assets/qzdl-%d.png" % size)

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
