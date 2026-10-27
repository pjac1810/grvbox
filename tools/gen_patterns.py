#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generate src/patterns.h: GRVBOX's eight factory patterns, written here as step grids.

Drums: a row per sound, one character a sixteenth: x a hit, X an accent, . nothing.
Parts: tokens a sixteenth, space separated: . rest, - tie (the notes before go on), C3 a note (C4 = MIDI 60),
C3+E3+G3 a chord, ! after a note an accent.

  python3 tools/gen_patterns.py
"""
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "src" / "patterns.h"
DRUM = {"BD": 0, "SD": 1, "CH": 2, "OH": 3, "CP": 4, "LT": 5, "MT": 6, "HT": 7, "RS": 8, "CB": 9, "CY": 10, "RD": 11,
        "B2": 12, "S2": 13, "SH": 14, "FX": 15}
TONE = {n: i for i, n in enumerate(["ACID BASS", "ACID SQR", "101 BASS", "808 BASS", "FM BASS", "SUB BASS", "SAW LEAD",
        "SQR LEAD", "FM LEAD", "SYN BRASS", "HOUSE STAB", "ORGAN STAB", "E.PIANO", "CLAV", "BELLS", "STRINGS",
        "WARM PAD", "CHOIR", "SUPERSAW", "PLUCK", "ZAP", "NOISE SWEEP", "SINE", "ORGAN"])}
KIT = {"808": 0, "909": 1, "606": 2, "CR-78": 3}
NOTES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(tok):
  n = NOTES[tok[0]]
  i = 1
  while tok[i] in "#b":
    n += 1 if tok[i] == "#" else -1
    i += 1
  return n + 12 * (int(tok[i:]) + 1)


# part settings: tone, level, pan, cut, res, env, lrate, ldepth, lfilt, porta, rev, dly, cho, oct
def ps(tone, level=100, pan=0, cut=0, res=0, env=0, rev=16, dly=0, cho=0, oct=0, ldepth=0, lfilt=0, porta=0):
  return [TONE[tone], level, pan, cut, res, env, 60, ldepth, lfilt, porta, rev, dly, cho, oct]


PATTERNS = [
  dict(name="HOUSE", bpm=124, bars=2, kit="909", shuffle=12,
       parts={1: ps("101 BASS", 105), 2: ps("ORGAN STAB", 80, pan=-10, rev=30, dly=20), 3: ps("STRINGS", 60, pan=12, rev=50, cho=50),
              4: ps("WARM PAD"), 5: ps("SAW LEAD"), 6: ps("BELLS", rev=40), 7: ps("PLUCK")},
       drums={"BD": "x...x...x...x...x...x...x...x...",
              "CP": "....x.......x.......x.......x...",
              "OH": "..x...x...x...x...x...x...x...x.",
              "CH": "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.",
              "SH": ".x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x"},
       notes={1: ". . C2 . . . C2 . . . C2 . . . Bb1 . . . C2 . . . C2 . . . F2 . . . Eb2 .",
              2: ". . . . . . C4+Eb4+G4 . . . . . . C4+Eb4+G4 . . . . . . . C4+Eb4+G4 . . . . . . Bb3+D4+F4 . . .",
              3: "C4+Eb4+G4 - - - - - - - - - - - - - - - Bb3+D4+F4 - - - - - - - - - - - - - - -"}),
  dict(name="TECHNO", bpm=132, bars=1, kit="909", shuffle=0,
       parts={1: ps("ACID BASS", 100, cut=-4, res=6, dly=24), 2: ps("ZAP", 70, pan=20, dly=40), 3: ps("STRINGS", 50, rev=60),
              4: ps("WARM PAD"), 5: ps("ACID SQR"), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "X...x...x...x...", "RD": "..x...x...x...x.", "CH": "x.xxx.xxx.xxx.xx", "CP": "....x.......x...",
              "LT": "...........x...."},
       notes={1: "A1! A1 A2 - A1 G2 A1 C3! A1 A1 A2 D2! - A1 E2 G2",
              2: ". . . . . . . . . . . . . . . E5"}),
  dict(name="HIP HOP", bpm=90, bars=2, kit="808", shuffle=45,
       parts={1: ps("808 BASS", 110), 2: ps("E.PIANO", 85, pan=-8, rev=30, cho=40), 3: ps("STRINGS"), 4: ps("WARM PAD"),
              5: ps("SQR LEAD"), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "x.......x.x.....x......x..x.....",
              "SD": "....x.......x.......x.......x...",
              "CH": "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.",
              "OH": "..............x...............x."},
       notes={1: "F1 - - - - - - - . . F1 . . . . . Ab1 - - - - - - . . . Eb1 - . . . .",
              2: "F3+Ab3+C4+Eb4 - - - - - - - . . . . . . . . Db3+F3+Ab3+C4 - - - - - - - . . Eb3+G3+Bb3+Db4 - - . . ."}),
  dict(name="DRUM BASS", bpm=172, bars=2, kit="909", shuffle=0,
       parts={1: ps("SUB BASS", 115), 2: ps("SAW LEAD"), 3: ps("STRINGS"), 4: ps("WARM PAD", 65, rev=70, cho=40),
              5: ps("SQR LEAD"), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "x.........x.......x.......x.....",
              "SD": "....x.......x.......x.......x...",
              "CH": "x.xxx.x.x.xxx.x.x.xxx.x.x.xxx.x.",
              "S2": ".............x...............x.."},
       notes={1: "E1 - - - - - - - - - G1 - - - - - D1 - - - - - - - - - A1 - - - . .",
              4: "E3+G3+B3 - - - - - - - - - - - - - - - D3+F#3+A3 - - - - - - - - - - - - - - -"}),
  dict(name="ELECTRO", bpm=118, bars=1, kit="808", shuffle=0,
       parts={1: ps("101 BASS", 100, cut=6), 2: ps("ZAP", 80, dly=30), 3: ps("STRINGS"), 4: ps("WARM PAD"),
              5: ps("SQR LEAD", 70, dly=40), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "x.....x...x.....", "CP": "....x.......x...", "CH": "x.x.x.x.x.x.x.x.", "CB": "..x.......x...x."},
       notes={1: "C2 . C2 C3 . C2 . Bb1 . C2 C3 . Eb2 . F2 .",
              2: ". . . . . . . . . . . . . . C5 .",
              5: ". . . . G4 . . . . . Bb4 . . . . ."}),
  dict(name="ACID", bpm=128, bars=2, kit="808", shuffle=8,
       parts={1: ps("ACID BASS", 100, res=10, dly=20), 2: ps("SAW LEAD"), 3: ps("STRINGS"), 4: ps("WARM PAD"),
              5: ps("ACID SQR", 70, pan=20, cut=10, dly=40), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "x...x...x...x...x...x...x...x...",
              "CH": "..x...x...x...x...x...x...x...x.",
              "SD": "....x.......x.......x.......x...",
              "CY": "x..............................."},
       notes={1: "C2! C2 C3 C2 Eb2 C2 - F2! C2 C2 Bb2 C2 G2! - C3 Bb1 C2! C2 C3 C2 Eb2 C2 - F2! C2 Ab2 - G2 Eb2! C2 C3 Bb1",
              5: ". . . . . . . . . . . . . . . . . . . . . . . . G4 . Bb4 . C5! . . ."}),
  dict(name="TRANCE", bpm=138, bars=2, kit="909", shuffle=0,
       parts={1: ps("FM BASS", 100), 2: ps("SUPERSAW", 72, rev=50, dly=30, cho=30), 3: ps("STRINGS"), 4: ps("WARM PAD"),
              5: ps("SQR LEAD"), 6: ps("PLUCK", 70, pan=-16, dly=50), 7: ps("PLUCK")},
       drums={"BD": "x...x...x...x...x...x...x...x...",
              "OH": "..x...x...x...x...x...x...x...x.",
              "CP": "....x.......x.......x.......x...",
              "CH": "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x."},
       notes={1: ". A1 A1 A1 . A1 A1 A1 . A1 A1 A1 . A1 A1 A1 . F1 F1 F1 . F1 F1 F1 . G1 G1 G1 . G1 G1 G1",
              2: ". . A3+C4+E4 . . . A3+C4+E4 . . . A3+C4+E4 . . . A3+C4+E4 . . . F3+A3+C4 . . . F3+A3+C4 . . . G3+B3+D4 . . . G3+B3+D4 .",
              6: "A4 E5 C5 E5 A4 E5 C5 E5 A4 E5 C5 E5 A4 E5 C5 E5 F4 C5 A4 C5 F4 C5 A4 C5 G4 D5 B4 D5 G4 D5 B4 D5"}),
  dict(name="BREAKBEAT", bpm=108, bars=2, kit="606", shuffle=20,
       parts={1: ps("FM BASS", 100), 2: ps("SAW LEAD"), 3: ps("CLAV", 80, pan=10, cho=20), 4: ps("WARM PAD"),
              5: ps("SQR LEAD"), 6: ps("BELLS"), 7: ps("PLUCK")},
       drums={"BD": "x.x.......x.....x.........x.x...",
              "SD": "....x..x.x..x.......x..x.x..x..x",
              "CH": "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.",
              "OH": "..............x...............x."},
       notes={1: "E1 . . E2 . . G1 . A1 . . . . . D2 . E1 . . E2 . . G1 . A1 . B1 . D2 . . .",
              3: ". . E3+G3+B3 . . E3+G3+B3 . . . . E3+G3+B3 . . . . . . . E3+G3+B3 . . E3+G3+B3 . . . . A3+C4+E4 . . . . ."}),
]


def build(p):
  steps = p["bars"] * 16
  ev = []
  for d, row in p["drums"].items():
    assert len(row) == steps, (p["name"], d, len(row))
    for s, c in enumerate(row):
      if c in "xX":
        ev.append((s, 0, DRUM[d], 127 if c == "X" else 100, 1))
  for part, line in p["notes"].items():
    toks = line.split()
    assert len(toks) == steps, (p["name"], part, len(toks))
    cur = []  # (index in ev) of the notes sounding
    for s, t in enumerate(toks):
      if t == "-":
        for i in cur:
          a = ev[i]
          ev[i] = (a[0], a[1], a[2], a[3], a[4] + 1)
        continue
      cur = []
      if t == ".":
        continue
      acc = t.endswith("!")
      for n in t.rstrip("!").split("+"):
        ev.append((s, part, midi(n), 120 if acc else 96, 1))
        cur.append(len(ev) - 1)
  ev.sort(key=lambda e: e[0])
  assert len(ev) <= 240
  return ev


def main():
  out = ["/* SPDX-License-Identifier: GPL-3.0-only */", "/* generated by tools/gen_patterns.py: do not edit */",
         "#ifndef GRVBOX_PATTERNS_H", "#define GRVBOX_PATTERNS_H", '#include "gb.h"', "",
         "static const pattern_t FACTORY_PAT[PATTERNS] = {"]
  for p in PATTERNS:
    ev = build(p)
    parts = []
    for i in range(8):
      if i == 0:
        v = [0, 110, 0, 0, 0, 0, 60, 0, 0, 0, 10, 0, 0, 0]
      else:
        v = p["parts"].get(i, ps("PLUCK"))
      parts.append("{{" + ",".join(str(x) for x in v) + "}}")
    evs = ",".join("{%d,%d,%d,%d,%d}" % e for e in ev)
    out.append(f'  {{"{p["name"]}", {p["bars"]}, {p["bpm"]}, {p["shuffle"]}, {KIT[p["kit"]]},')
    out.append("   {" + ",".join(parts) + "},")
    out.append(f"   {len(ev)}, {{{evs}}}}},")
  out += ["};", "", "#endif", ""]
  OUT.write_text("\n".join(out))
  print("wrote", OUT, [len(build(p)) for p in PATTERNS])


if __name__ == "__main__":
  main()
