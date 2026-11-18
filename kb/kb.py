#!/usr/bin/env python3
"""John knowledge base: semantic search over kb/notes/*.md and the code, with Voyage AI embeddings.

    python3 kb/kb.py query "how do breath marks attach to notes"   [-n 6] [--notes | --code]
    python3 kb/kb.py sync     # refresh the index (query does this first)
    python3 kb/kb.py check    # validate the notes' format (no API key needed; runs in ctest)

Needs VOYAGE_API_KEY for sync/query. Standard library only. The index is a local, gitignored cache
(kb/.cache/kb.db), refreshed incrementally: only files whose content changed are re-embedded.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import re
import sqlite3
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NOTES = "kb/notes/"
DB = Path(os.environ.get("KB_DB_PATH") or ROOT / "kb" / ".cache" / "kb.db")
# voyage-4 beat voyage-code-4 on this repo (prose notes + C++): right result first for 9/10 test questions vs 3/10
# (code-4 with input_type=document buries the notes). Same 200M free tokens. Re-measure before changing (issue #28).
MODEL, DIM = os.environ.get("KB_MODEL") or "voyage-4", 1024
CODE_EXT = {".cpp", ".hpp", ".h", ".md", ".txt", ".sh", ".yml"}
WINDOW, OVERLAP = 60, 15
SECTIONS = ["Context", "Rule", "Why", "How to apply"]
WEAK = 0.65  # best distance above this = no strong match (voyage-4: relevant hits ~0.35-0.6, unrelated ~0.8)


# ---------------- chunking ----------------
def chunk_code(path: str, text: str) -> list[dict]:
    lines = text.splitlines()
    out = []
    for start in range(0, max(len(lines), 1), WINDOW - OVERLAP):
        body = "\n".join(lines[start:start + WINDOW])
        if len(re.sub(r"\s", "", body)) >= 30:
            out.append({"kind": "code", "start": start + 1, "end": min(start + WINDOW, len(lines)), "heading": "",
                        "text": f"{path}\n{body}"})
        if start + WINDOW >= len(lines):
            break
    return out


def parse_note(text: str) -> tuple[dict, str]:
    """Frontmatter (title, tags) and the body after it. Raises ValueError if the frontmatter is missing."""
    m = re.match(r"---\n(.*?)\n---\n(.*)", text, re.S)
    if not m:
        raise ValueError("missing --- frontmatter --- block")
    meta = {}
    for line in m.group(1).splitlines():
        k, _, v = line.partition(":")
        meta[k.strip()] = v.strip()
    return meta, m.group(2)


def chunk_note(text: str) -> list[dict]:
    """One chunk per ## section, each prefixed with the title and tags so they take part in matching."""
    meta, body = parse_note(text)
    head = f"{meta.get('title', '')}\ntags: {meta.get('tags', '')}"
    out = []
    for part in re.split(r"(?m)^(?=## )", body):
        if not part.startswith("## ") or not part.strip("# \n"):
            continue
        heading = part.splitlines()[0][3:].strip()
        out.append({"kind": "note", "start": 0, "end": 0, "heading": heading, "text": f"{head}\n\n{part.strip()}"})
    return out


def check_note(name: str, text: str) -> list[str]:
    """Format problems in one note (empty list = fine)."""
    errs = []
    if not re.fullmatch(r"[a-z0-9]+(-[a-z0-9]+)*\.md", name) or len(name) > 63:
        errs.append("file name must be kebab-case, at most 60 characters + .md")
    try:
        meta, body = parse_note(text)
    except ValueError as e:
        return errs + [str(e)]
    if not meta.get("title"):
        errs.append("frontmatter needs a title")
    if not re.fullmatch(r"\[[^\]]+\]", meta.get("tags", "")):
        errs.append("frontmatter needs tags: [tag-one, tag-two]")
    if not re.search(r"(?m)^# \S", body):
        errs.append("needs a '# Title' heading")
    have = re.findall(r"(?m)^## (.+?)\s*$", body)
    errs += [f"missing section '## {s}'" for s in SECTIONS if s not in have]
    return errs


# ---------------- files ----------------
def repo_files() -> dict[str, str]:
    """path -> text for every tracked or untracked-but-not-ignored file we index."""
    names = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=ROOT,
                           capture_output=True, text=True, check=True).stdout.splitlines()
    out = {}
    for n in sorted(set(names)):
        p = ROOT / n
        if (n.startswith(NOTES) and n.endswith(".md")) or (Path(n).suffix in CODE_EXT and not n.startswith("kb/")):
            if p.is_file():
                out[n] = p.read_text(errors="replace")
    return out


# ---------------- Voyage ----------------
def embed(texts: list[str], input_type: str) -> list[array]:
    key = os.environ.get("VOYAGE_API_KEY")
    if not key:
        sys.exit("VOYAGE_API_KEY is not set (get one at https://dash.voyageai.com/)")
    # python.org builds for macOS ship without root certificates: use the system bundle when there is one
    cafile = os.environ.get("SSL_CERT_FILE") or next((f for f in ("/etc/ssl/cert.pem",) if Path(f).is_file()), None)
    ctx = ssl.create_default_context(cafile=cafile)
    out: list[array] = []
    batch: list[str] = []
    size = 0

    def flush() -> None:
        body = json.dumps({"input": batch, "model": MODEL, "input_type": input_type, "output_dimension": DIM}).encode()
        for attempt in range(7):
            req = urllib.request.Request("https://api.voyageai.com/v1/embeddings", body,
                                         {"Authorization": f"Bearer {key}", "Content-Type": "application/json"})
            try:
                with urllib.request.urlopen(req, timeout=120, context=ctx) as r:
                    data = json.load(r)["data"]
                out.extend(array("f", d["embedding"]) for d in sorted(data, key=lambda d: d["index"]))
                return
            except urllib.error.HTTPError as e:
                if e.code not in (429, 500, 502, 503, 504) or attempt == 6:
                    sys.exit(f"Voyage API error {e.code}: {e.read().decode(errors='replace')[:300]}")
            except urllib.error.URLError as e:
                if attempt == 6 or isinstance(e.reason, ssl.SSLError):  # retrying won't fix certificates
                    sys.exit(f"cannot reach Voyage API: {e.reason}")
            time.sleep(min(60, 2 ** attempt) * (0.5 + random.random()))

    for t in texts:  # stay well under the per-request limits (1000 texts, ~120k tokens)
        if batch and (len(batch) >= 128 or size + len(t) > 200_000):
            flush()
            batch, size = [], 0
        batch.append(t)
        size += len(t)
    if batch:
        flush()
    return out


# ---------------- index ----------------
def connect() -> sqlite3.Connection:
    DB.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(DB)
    db.executescript("""
        CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);
        CREATE TABLE IF NOT EXISTS file(path TEXT PRIMARY KEY, hash TEXT);
        CREATE TABLE IF NOT EXISTS chunk(path TEXT, kind TEXT, start INT, end INT, heading TEXT, text TEXT, emb BLOB);
        CREATE INDEX IF NOT EXISTS chunk_path ON chunk(path);
    """)
    model = f"{MODEL}/{DIM}"
    if (db.execute("SELECT value FROM meta WHERE key='model'").fetchone() or [model])[0] != model:
        db.executescript("DELETE FROM file; DELETE FROM chunk;")  # embeddings from another model don't mix
    db.execute("INSERT OR REPLACE INTO meta VALUES('model', ?)", (model,))
    return db


def sync(db: sqlite3.Connection, quiet: bool = False) -> None:
    files = repo_files()
    known = dict(db.execute("SELECT path, hash FROM file"))
    gone = [p for p in known if p not in files]
    todo, chunks, bad = {}, [], []
    for path, text in files.items():
        h = hashlib.sha256(text.encode()).hexdigest()
        if known.get(path) == h:
            continue
        try:
            cs = chunk_note(text) if path.startswith(NOTES) else chunk_code(path, text)
        except ValueError as e:
            bad.append(f"{path}: {e}")
            continue
        todo[path] = h
        chunks += [dict(c, path=path) for c in cs]
    vecs = embed([c["text"] for c in chunks], "document") if chunks else []
    for p in gone + list(todo):
        db.execute("DELETE FROM chunk WHERE path=?", (p,))
        db.execute("DELETE FROM file WHERE path=?", (p,))
    db.executemany("INSERT INTO chunk VALUES(?,?,?,?,?,?,?)",
                   [(c["path"], c["kind"], c["start"], c["end"], c["heading"], c["text"], v.tobytes())
                    for c, v in zip(chunks, vecs)])
    db.executemany("INSERT INTO file VALUES(?,?)", todo.items())
    db.commit()
    if not quiet or bad:
        print(f"kb: {len(todo)} file(s) re-embedded ({len(chunks)} chunks), {len(gone)} removed, "
              f"{len(files) - len(todo)} unchanged", file=sys.stderr)
    for b in bad:
        print(f"kb: skipped {b} (run: python3 kb/kb.py check)", file=sys.stderr)


def query(db: sqlite3.Connection, q: str, n: int, kind: str | None) -> None:
    qv = embed([q], "query")[0]
    rows = []
    for path, k, start, end, heading, text, blob in db.execute("SELECT * FROM chunk"):
        if kind and k != kind:
            continue
        v = array("f")
        v.frombytes(blob)
        rows.append((1 - sum(a * b for a, b in zip(qv, v)), path, k, start, end, heading, text))  # unit vectors
    rows.sort(key=lambda r: r[0])
    if not rows:
        print("No results: the index is empty.")
        return
    shown: set[str] = set()
    for dist, path, k, start, end, heading, text in rows:
        if len(shown) >= n:
            break
        if k == "note":  # one hit per note, printed whole (notes are short)
            if path in shown:
                continue
            print(f"--- [note] {path}  (distance {dist:.2f}, best section: {heading})")
            print(parse_note((ROOT / path).read_text())[1].strip())
        else:
            body = text.split("\n", 1)[-1].splitlines()
            print(f"--- [code] {path}:{start}-{end}  (distance {dist:.2f})")
            print("\n".join(body[:25] + ([f"... ({len(body) - 25} more lines: read the file)"] if len(body) > 25 else [])))
        shown.add(path if k == "note" else f"{path}:{start}")
        print()
    if rows[0][0] > WEAK:
        print(f"(No strong match: best distance {rows[0][0]:.2f}. Search the code directly, or ask the user. "
              "Once you've worked it out, write a note in kb/notes/ so the next agent finds it.)")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    qp = sub.add_parser("query", help="search notes and code")
    qp.add_argument("text", nargs="+")
    qp.add_argument("-n", type=int, default=6, help="number of results (default 6)")
    g = qp.add_mutually_exclusive_group()
    g.add_argument("--notes", action="store_const", const="note", dest="kind", help="only knowledge notes")
    g.add_argument("--code", action="store_const", const="code", dest="kind", help="only code")
    sub.add_parser("sync", help="refresh the index")
    sub.add_parser("check", help="validate kb/notes/*.md")
    a = ap.parse_args()

    if a.cmd == "check":
        notes = sorted((ROOT / NOTES).glob("*.md"))
        failed = 0
        for p in notes:
            for e in check_note(p.name, p.read_text()):
                print(f"{NOTES}{p.name}: {e}")
                failed += 1
        print(f"kb check: {len(notes)} note(s), {failed} problem(s)")
        sys.exit(1 if failed else 0)
    db = connect()
    sync(db, quiet=a.cmd == "query")
    if a.cmd == "query":
        query(db, " ".join(a.text), a.n, a.kind)


if __name__ == "__main__":
    main()
