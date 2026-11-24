#!/bin/sh
# Snapshot the viewer to a PNG, no patching: scripts/snap.sh <out.png> [VAR=value...]
#   scripts/snap.sh /tmp/s.png JOHN_SNAPSHOT_SONG=down-by-the-sally-gardens JOHN_SNAPSHOT_CURSOR=12
# The variables (JOHN_SNAPSHOT_SONG, _OPEN, _HOVER, _CURSOR, _MIC, _FRAMES, or JOHN_HOME=<dir> for your own data
# folder) are in kb/notes/gui-snapshot-convert-full-image.md. Without JOHN_HOME the data folder is new each run, at
# the same path (the path shows in the image): the bundled lessons plus the demo/ songs. Build first: cmake --build build.
set -eu
[ $# -ge 1 ] || { echo "usage: $0 <out.png> [VAR=value...]" >&2; exit 2; }
out=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}; home=${tmp%/}/john-snap/$(basename "$out" .png)
rm -rf "$home" && mkdir -p "$home/songs" && cp "$root"/demo/*.json "$home/songs/"
tga=${out%.png}.tga
rm -f "$tga"
env JOHN_HOME="$home" JOHN_SNAPSHOT="$tga" "$@" "$root/build/john"  # a JOHN_HOME= argument wins over ours
[ -f "$tga" ] || { echo "no snapshot written" >&2; exit 1; }
if command -v sips >/dev/null; then sips -s format png "$tga" --out "$out" >/dev/null && echo "$out"
else echo "$tga (no sips: convert it yourself)"; fi
