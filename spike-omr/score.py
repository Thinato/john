"""Score a reader's MusicXML against John song JSONs (the ground truth, in page order).
   python3 score.py result.musicxml truth1.json [truth2.json ...]   -> one JSON line

Notes are compared in written order (repeats not unfolded), grace notes and rests skipped, chords reduced to their
top note (as John's importers do). The two sequences are aligned by edit distance on pitch:
  pitch  = GT notes aligned to a read note of the same MIDI pitch / GT notes
  rhythm = GT notes aligned (match or substitution) to a read note of the same duration / GT notes
  extra  = read notes left over after alignment (inserted)
  key    = a tune counts when every read note aligned to it was read under the tune's key signature (fifths;
           mode ignored); meter likewise with the time signature."""
import json
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction

STEP = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def strip_ns(root):
    for e in root.iter():
        e.tag = e.tag.split("}")[-1]
    return root


def read_xml(path):
    root = strip_ns(ET.parse(path).getroot())
    notes, keys, meters = [], [], []
    key, meter = 0, None  # no <key> yet: no signature (homr writes none for C major)
    parts = root.findall("part")
    if not parts:
        return notes, keys, meters
    # every part (homr writes one part per voice): take the part with the most pitched notes
    best = max(parts, key=lambda p: len(p.findall(".//note/pitch")))
    div = 1
    for m in best.findall("measure"):
        for el in m:
            if el.tag == "attributes":
                if el.find("divisions") is not None:
                    div = int(el.find("divisions").text)
                for k in el.findall("key"):
                    if k.find("fifths") is not None:
                        key = int(k.find("fifths").text)
                        keys.append(key)
                for t in el.findall("time"):
                    b, bt = t.find("beats"), t.find("beat-type")
                    if b is not None and bt is not None:
                        meter = f"{b.text}/{bt.text}"
                        meters.append(meter)
            elif el.tag == "note":
                if el.find("grace") is not None or el.find("rest") is not None or el.find("pitch") is None:
                    continue
                if el.find("voice") is not None and el.find("voice").text not in ("1", None):
                    continue
                p = el.find("pitch")
                midi = 12 * (int(p.find("octave").text) + 1) + STEP[p.find("step").text] + round(float(p.findtext("alter") or 0))
                dur = Fraction(int(el.findtext("duration") or 0), div)
                if el.find("chord") is not None and notes:
                    if midi > notes[-1][0]:
                        notes[-1] = (midi,) + notes[-1][1:]
                    continue
                notes.append((midi, dur, key, meter))
    return notes, keys, meters


def read_truth(paths):
    notes, tunes = [], []
    for p in paths:
        s = json.load(open(p))
        tunes.append((s["key"]["fifths"], s["meter"], len(notes)))
        notes += [(e["midi"], Fraction(e["len"], 48)) for e in s["events"] if e["type"] == "note"]
    return notes, tunes


def align(gt, rd):
    """Levenshtein on pitch; returns the list of (gt_i, rd_j) aligned pairs (matches and substitutions)."""
    n, m = len(gt), len(rd)
    d = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        d[i][0] = i
    for j in range(m + 1):
        d[0][j] = j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            d[i][j] = min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (gt[i - 1][0] != rd[j - 1][0]))
    pairs, i, j = [], n, m
    while i and j:
        if d[i][j] == d[i - 1][j - 1] + (gt[i - 1][0] != rd[j - 1][0]):
            pairs.append((i - 1, j - 1)); i -= 1; j -= 1
        elif d[i][j] == d[i - 1][j] + 1:
            i -= 1
        else:
            j -= 1
    return pairs[::-1]


def score(xml, truths):
    gt, tunes = read_truth(truths)
    rd, keys, meters = read_xml(xml)
    pairs = align(gt, rd)
    owner = [max(t for t in range(len(tunes)) if tunes[t][2] <= i) for i in range(len(gt))]
    key_ok = [all(rd[j][2] == tunes[owner[i]][0] for i, j in pairs if owner[i] == t) and any(owner[i] == t for i, _ in pairs) for t in range(len(tunes))]
    meter_ok = [all(rd[j][3] == tunes[owner[i]][1] for i, j in pairs if owner[i] == t) and any(owner[i] == t for i, _ in pairs) for t in range(len(tunes))]
    pitch = sum(gt[i][0] == rd[j][0] for i, j in pairs)
    rhythm = sum(gt[i][1] == rd[j][1] for i, j in pairs)
    return {
        "gt_notes": len(gt), "read_notes": len(rd),
        "pitch_pct": round(100 * pitch / len(gt), 1), "rhythm_pct": round(100 * rhythm / len(gt), 1),
        "extra": len(rd) - len(pairs),
        "key_ok": f"{sum(key_ok)}/{len(tunes)}", "meter_ok": f"{sum(meter_ok)}/{len(tunes)}",
        "keys_read": sorted(set(keys)), "meters_read": sorted(set(meters)),
    }


if __name__ == "__main__":
    print(json.dumps(score(sys.argv[1], sys.argv[2:])))
