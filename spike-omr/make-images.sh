#!/usr/bin/env bash
# Test pages for issue #74, all from John's own public-domain/traditional songs (demo/, lessons/).
#   ./make-images.sh <toabc binary> <out dir>
# toabc: a 6-line tool over john_core's toAbc(fromJson()) (see README.md). Needs node (verovio), Chrome, magick.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
toabc="$1"; out="$2"; repo="$here/.."
chrome="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
mkdir -p "$out/abc"

abc() { local f="$repo/demo/$1.json"; [ -f "$f" ] || f="$repo/lessons/$1.json"; "$toabc" "$f" | python3 "$here/beam.py" > "$out/abc/$1.abc"; }
shot() {  # html -> trimmed png with a white margin, as a printed page
  "$chrome" --headless --disable-gpu --hide-scrollbars --force-device-scale-factor=2 --user-data-dir="$out/.chrome" --window-size=2100,2970 \
    --screenshot="$out/$1.raw.png" "file://$out/$1.html" 2>/dev/null
  magick "$out/$1.raw.png" -trim -bordercolor white -border 120 -colorspace Gray "$out/$1.png"; rm "$out/$1.raw.png"
}

for s in the-kesh minuet-in-g drowsy-maggie greensleeves the-silver-spear the-butterfly britches-full-of-stitches; do abc $s; done
for s in the-kesh drowsy-maggie greensleeves the-silver-spear; do
  node "$here/render.mjs" "$out/clean-$s.html" "$out/abc/$s.abc"; shot clean-$s
done

# Two tunes on one page
node "$here/render.mjs" "$out/two-tunes.html" "$out/abc/the-butterfly.abc" "$out/abc/britches-full-of-stitches.abc"; shot two-tunes

# A clean vector PDF (the page reads it through pdf.js, as #75 would)
node "$here/render.mjs" "$out/pdf-minuet-in-g.html" "$out/abc/minuet-in-g.abc"
"$chrome" --headless --disable-gpu --no-pdf-header-footer --user-data-dir="$out/.chrome" \
  --print-to-pdf="$out/pdf-minuet-in-g.pdf" "file://$out/pdf-minuet-in-g.html" 2>/dev/null

# A "phone photo": the clean Kesh page tilted in perspective, rotated, lit unevenly, blurred, noisy, JPEG q55
magick "$out/clean-the-kesh.png" -resize 1800x -bordercolor white -border 60 -alpha set -virtual-pixel background -background '#8a8070' \
  -distort Perspective '0,0 40,70  1919,0 1850,10  0,%[fx:h-1] 10,%[fx:h-40]  1919,%[fx:h-1] 1900,%[fx:h-1]' \
  -rotate 2.5 -alpha off \
  \( +clone -sparse-color Barycentric '0,0 #fff8e8 %[fx:w-1],%[fx:h-1] #8c8478' \) -compose Multiply -composite \
  -blur 0x1.2 -attenuate 0.6 +noise Gaussian -quality 55 "$out/photo-the-kesh.jpg"
rm -rf "$out/.chrome"
ls -la "$out"
