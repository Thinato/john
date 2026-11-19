"""Score every results/<reader>-<backend>-<hosting>/ run against pages.json and print markdown tables.
   python3 table.py"""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from score import score  # noqa: E402

here = os.path.dirname(os.path.abspath(__file__))
pages = json.load(open(os.path.join(here, "pages.json")))
truth = lambda p: [os.path.join(here, "..", t) for t in pages[p]]  # noqa: E731

print("| reader | browser | backend | hosting | page | notes read / GT | pitches right | rhythms right | key | meter | ms |")
print("|---|---|---|---|---|---|---|---|---|---|---|")
for d in sorted(glob.glob(os.path.join(here, "results", "*", "run.json"))):
    run = json.load(open(d))
    folder = os.path.dirname(d)
    for i, p in enumerate(run["pages"][1:], 1):  # page 0 is the cold read of page 1
        name = p["image"].split("/")[-1]
        row = f"| {run['reader']} | {run.get('browser', 'chrome')} | {run['prefer']} | {run['hosting']} | {name} |"
        if "error" in p:
            print(f"{row} failed: {p['error'][:60]} | | | | | {p['ms']} |")
            continue
        s = score(os.path.join(folder, f"{i}-{name}.musicxml"), truth(p["image"]))
        print(f"{row} {s['read_notes']}/{s['gt_notes']} | {s['pitch_pct']}% | {s['rhythm_pct']}% | {s['key_ok']} | {s['meter_ok']} | {p['ms']} |")

print("\n| reader | browser | backend | hosting | isolated | init ms | first page (cold) ms | warm page ms (median) | models MB | peak memory MB (delta) |")
print("|---|---|---|---|---|---|---|---|---|---|")
for d in sorted(glob.glob(os.path.join(here, "results", "*", "run.json"))):
    r = json.load(open(d))
    warm = sorted(p["ms"] for p in r["pages"][1:])
    print(f"| {r['reader']} | {r.get('browser', 'chrome')} | {r['backend']} | {r['hosting']} | {r['crossOriginIsolated']} | {r['initMs']} | {r['initMs'] + r['pages'][0]['ms']} | "
          f"{warm[len(warm) // 2]} | {r['downloadMB']} | {r['memory']['peakMB']} (+{r['memory']['deltaMB']}) |")
