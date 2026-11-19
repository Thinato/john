"""Score a folder of <page stem>.musicxml files (e.g. copisteria's Python pipeline output) against pages.json.
   python3 score_dir.py <dir> [<dir> ...]"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from score import score  # noqa: E402

here = os.path.dirname(os.path.abspath(__file__))
pages = json.load(open(os.path.join(here, "pages.json")))
for d in sys.argv[1:]:
    for p, truths in pages.items():
        f = os.path.join(d, p.split("/")[-1].rsplit(".", 1)[0] + ".musicxml")
        if os.path.exists(f):
            s = score(f, [os.path.join(here, "..", t) for t in truths])
            print(f"| {os.path.basename(d)} | {p.split('/')[-1]} | {s['read_notes']}/{s['gt_notes']} | {s['pitch_pct']}% | {s['rhythm_pct']}% | {s['key_ok']} | {s['meter_ok']} |")
