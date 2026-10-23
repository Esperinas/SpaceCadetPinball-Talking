#!/usr/bin/env python3
import gzip
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
translations = (root / "SpaceCadetPinball/translations.cpp").read_text(encoding="utf-8")
languages = ("SimplifiedChinese", "Russian", "Japanese")
codepoints = set()
for language in languages:
    values = re.findall(r'\{\s*Lang::' + language + r',\s*"((?:[^"\\]|\\.)*)"\s*\}', translations)
    for value in values:
        codepoints.update(ord(char) for char in value if ord(char) >= 0x100)

glyphs = {}
font_source = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "third_party/unifont/unifont_jp-17.0.03.bdf.gz"
with gzip.open(font_source, "rt", encoding="ascii", errors="replace") as source:
    encoding = None
    width = 0
    rows = []
    in_bitmap = False
    for raw_line in source:
        line = raw_line.strip()
        if line.startswith("ENCODING "):
            encoding = int(line.split()[1])
        elif line.startswith("BBX "):
            width = int(line.split()[1])
        elif line == "BITMAP":
            rows = []
            in_bitmap = True
        elif line == "ENDCHAR":
            if encoding in codepoints and rows:
                normalized = rows[:16] + [0] * max(0, 16 - len(rows))
                glyphs[encoding] = (width, normalized[:16])
            encoding = None
            in_bitmap = False
        elif in_bitmap:
            value = int(line, 16)
            bits = len(line) * 4
            if bits > width:
                value >>= bits - width
            rows.append(value)

missing = sorted(codepoints - glyphs.keys())
if missing:
    raise SystemExit("Missing Unicode glyphs: " + ", ".join(f"U+{cp:04X}" for cp in missing))

output = root / "SpaceCadetPinball/UnicodeGlyphData.h"
with output.open("w", encoding="ascii", newline="\n") as target:
    target.write("// Generated from GNU Unifont JP 17.0.03 (SIL Open Font License 1.1).\n")
    target.write("// Contains only glyphs used by Simplified Chinese, Russian and Japanese translations.\n")
    target.write("#pragma once\n\n#include <cstdint>\n\n")
    target.write("struct UnicodeBitmapGlyph { uint32_t Codepoint; uint8_t Width; uint16_t Rows[16]; };\n")
    target.write("static constexpr UnicodeBitmapGlyph UnicodeBitmapGlyphs[] = {\n")
    for codepoint in sorted(glyphs):
        width, rows = glyphs[codepoint]
        row_text = ", ".join(f"0x{row:04X}" for row in rows)
        target.write(f"\t{{0x{codepoint:04X}, {width}, {{{row_text}}}}},\n")
    target.write("};\n")

print(f"Generated {len(glyphs)} Unicode bitmap glyphs in {output}")
