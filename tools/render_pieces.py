#!/usr/bin/env python3
"""
render_pieces.py - OPTIONAL helper, NOT part of the build.

Renders the cburnett SVG chess pieces in tools/cburnett_svg/ to the PNG files
in assets/pieces/ that the game loads at runtime. The PNGs are already
committed, so you only need this if you want a different size or piece set.

    uv run --with cairosvg python tools/render_pieces.py      (or: pip install cairosvg)

Piece artwork: Colin M.L. Burnett ("cburnett"), see assets/pieces/LICENSE.txt.
"""
from pathlib import Path
import cairosvg

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "cburnett_svg"
OUT = ROOT / "assets" / "pieces"
SIZE = 256  # pixels; the game scales them down smoothly (with mipmaps)

OUT.mkdir(parents=True, exist_ok=True)
for svg in sorted(SRC.glob("*.svg")):
    png = OUT / (svg.stem + ".png")
    cairosvg.svg2png(url=str(svg), write_to=str(png), output_width=SIZE, output_height=SIZE)
    print("wrote", png.relative_to(ROOT))
