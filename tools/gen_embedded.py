#!/usr/bin/env python3
"""Regenerates src/Embedded.{h,cpp} from res/ and data/.

The output is byte-identical to what clang-format would produce for the
project style, so a regeneration with unchanged inputs yields no diff.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = [
    ("avatar", root / "res" / "avatar.jpg"),
    ("cs2", root / "res" / "cs2.jpg"),
    ("csgo", root / "res" / "csgo.jpg"),
    ("valorant", root / "res" / "valorant.jpg"),
    ("apex", root / "res" / "apex.jpg"),
    ("cheats_json", root / "data" / "cheats.json"),
]
out_h = root / "src" / "Embedded.h"
out_cpp = root / "src" / "Embedded.cpp"


def emit_array(name: str, data: bytes) -> str:
    lines = [f"const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    lines.append(f"const size_t {name}_SIZE = sizeof({name});")
    lines.append("")
    return "\n".join(lines)


out_h.write_text(
    """#pragma once
#include <cstddef>

namespace nl::embed {

struct Blob {
    const unsigned char* data;
    size_t size;
};

Blob Avatar();
Blob Cs2();
Blob Csgo();
Blob Valorant();
Blob Apex();
Blob CheatsJson();

} // namespace nl::embed
""",
    encoding="utf-8",
    newline="\n",
)

parts = ['#include "Embedded.h"\n\nnamespace nl::embed {\nnamespace {\n\n']
for name, path in files:
    data = path.read_bytes()
    arr = name.upper() + "_DATA"
    parts.append(emit_array(arr, data))
    print(f"{name}: {len(data)} bytes")

parts.append("} // namespace\n\n")
for fn, arr in [
    ("Avatar", "AVATAR_DATA"),
    ("Cs2", "CS2_DATA"),
    ("Csgo", "CSGO_DATA"),
    ("Valorant", "VALORANT_DATA"),
    ("Apex", "APEX_DATA"),
    ("CheatsJson", "CHEATS_JSON_DATA"),
]:
    parts.append(f"Blob {fn}() {{\n    return Blob{{{arr}, {arr}_SIZE}};\n}}\n")
parts.append("\n} // namespace nl::embed\n")
out_cpp.write_text("".join(parts), encoding="utf-8", newline="\n")
print(f"wrote {out_cpp} ({out_cpp.stat().st_size} bytes)")
