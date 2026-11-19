"""Make John's toAbc output (L:1/16, a space between every note) look like a printed tune book: beam the notes of
each beat together, drop the technique decorations, write '::' as ':|:'.   python3 beam.py < in.abc > out.abc"""
import re
import sys
from fractions import Fraction

NOTE = re.compile(r"(\(\d)|([\^_=]*[A-Ga-gz][,']*)(\d*)(/?\d*)(-?)|([|:\]\[]+\d?)")


def beam_bar(body, beat, tup):
    out, pos = "", Fraction(0)
    for m in NOTE.finditer(body):
        if m.group(1):  # tuplet marker: its notes take 2/3 of their written length
            out += " " + m.group(1)
            tup[0] = 3
            continue
        if m.group(6):
            out += " " + m.group(6) + " "
            pos = Fraction(0)
            continue
        n = Fraction(int(m.group(3) or 1))
        if m.group(4):
            n /= int(m.group(4)[1:] or 2)
        if tup[0]:
            n = n * 2 / 3
            tup[0] -= 1
        if pos % beat == 0 and pos and not re.search(r"\(\d$", out):
            out += " "
        out += m.group(2) + (m.group(3) or "") + m.group(4) + m.group(5)
        pos += n
    return out


def main():
    beat = 4
    for line in sys.stdin:
        line = re.sub(r"![a-z]+!", "", line.rstrip("\n"))
        if re.match(r"^[A-Z]:", line):
            if line.startswith("M:"):
                num, den = map(int, line[2:].split("/"))
                beat = 6 if den == 8 and num % 3 == 0 else 16 // den  # compound: a dotted quarter
            print(line)
            continue
        line = line.replace("::", ":|:")
        print(re.sub(r"\s+", " ", beam_bar(line, beat, [0])).strip())


if __name__ == "__main__":
    main()
