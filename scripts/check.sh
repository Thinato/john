#!/usr/bin/env bash
# The definition of done in one command: configure build/ if needed, build, ctest, kb check.
# Usage: scripts/check.sh [--web]   (--web also builds build-web/ when emcmake is on the PATH)
# Stops at the first failure (exit 1) and prints what ran. Reuses build/ as configured, so CI
# configures it first with its own flags.
set -u
cd "$(dirname "$0")/.." || exit 1

web=0
for arg in "$@"; do
  case $arg in
    --web) web=1 ;;
    -h|--help) sed -n '2,5p' "$0"; exit 0 ;;
    *) echo "check.sh: unknown option $arg (try --help)" >&2; exit 2 ;;
  esac
done

ran=""
summary() { printf '\n== check.sh summary ==\n%b' "$ran"; }
step() {  # step "name" command args...
  name=$1; shift
  printf '\n== %s: %s\n' "$name" "$*"
  if "$@"; then
    ran="$ran  ok    $name\n"
  else
    ran="$ran  FAIL  $name\n"
    summary
    exit 1
  fi
}

[ -f build/CMakeCache.txt ] || step configure cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
step build cmake --build build --parallel
step ctest ctest --test-dir build --output-on-failure
step kb python3 kb/kb.py check

if [ "$web" = 1 ]; then
  if command -v emcmake >/dev/null 2>&1; then
    [ -f build-web/CMakeCache.txt ] || step web-configure emcmake cmake -S . -B build-web
    step web-build cmake --build build-web --parallel
  else
    ran="$ran  skip  web (emcmake not on the PATH)\n"
  fi
fi

summary
