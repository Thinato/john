#!/bin/sh
# Configures and builds this worktree's build/ with the shared dependency cache (JOHN_DEPS_CACHE, default ~/.cache/john)
# and ccache when installed. Extra arguments go to the configure step, e.g. -DCMAKE_BUILD_TYPE=Release.
set -e
cd "$(git rev-parse --show-toplevel)"
cmake -S . -B build -DJOHN_DEPS_CACHE="${JOHN_DEPS_CACHE:-$HOME/.cache/john}" "$@"
cmake --build build --parallel
echo "Built. Test with: ctest --test-dir build --output-on-failure"
