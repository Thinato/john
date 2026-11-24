#!/bin/sh
# Drives john-mcp over stdio. Each tests/mcp/<area>/<name>.sh is one scenario (areas mirror src/mcp_<area>.cpp,
# protocol/ is mcp_main.cpp) and runs against its own fresh server and empty JOHN_HOME. A scenario is a list of
#   call <id> '<request body after "id">'   then   expect '<grep pattern>' / refute '<grep pattern>'
# on that call's reply, found by its id. The runner reads a scenario twice: first to send all its requests to
# the server in one run, then to check each reply. A failing check prints the scenario, call id and reply; the
# other checks still run. Adding a tool's checks = a new file in its area. See kb/notes/mcp-smoke-per-tool-checks.md.
BIN="$1"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
REQ="$T/requests"
RES="$T/replies"
CR=$(printf '\r')  # replies end in a bare \n (Windows text-mode stdout would add \r)

fail() {
  echo "FAIL $NAME: $ID: $1"
  [ -z "$REPLY" ] || printf '  reply: %.600s\n' "$REPLY"
  FAILS=$((FAILS + 1))
}
call() {
  ID=$1
  if [ "$PHASE" = send ]; then
    printf '{"jsonrpc":"2.0","id":"%s",%s}\n' "$1" "$2" >> "$REQ"
    CALLS=$((CALLS + 1))
    return
  fi
  REPLY=$(grep -F "\"id\":\"$1\",\"jsonrpc\":\"2.0\"" "$RES")
  case $(printf '%s\n' "$REPLY" | grep -c .) in
    1) ;;
    0) fail "no reply" ;;
    *) fail "more than one reply (id used twice in this scenario?)" ;;
  esac
}
notify() { if [ "$PHASE" = send ]; then printf '{"jsonrpc":"2.0",%s}\n' "$1" >> "$REQ"; fi; }
expect() { if [ "$PHASE" = check ] && ! printf '%s\n' "$REPLY" | grep -q -e "$1"; then fail "reply lacks: $1"; fi; }
refute() { if [ "$PHASE" = check ] && printf '%s\n' "$REPLY" | grep -q -e "$1"; then fail "reply has: $1"; fi; }

FAILED=
for f in "$(dirname "$0")"/mcp/*/*.sh; do
  NAME=${f#"$(dirname "$0")"/mcp/}
  ( FAILS=0 CALLS=0 ID=server REPLY=
    JOHN_HOME="$T/home"; export JOHN_HOME
    rm -rf "$JOHN_HOME"; mkdir "$JOHN_HOME"
    : > "$REQ"
    PHASE=send; . "$f"
    "$BIN" < "$REQ" > "$RES" || fail "exited with status $?"
    ID=stdout
    REPLY=$(grep -vn '^{.*"jsonrpc":"2.0".*}$' "$RES" | head -1)
    [ -z "$REPLY" ] || fail "a line on stdout that is not a JSON-RPC message (line:text below)"
    REPLY=
    if grep -q "$CR" "$RES"; then fail "a reply contains \\r"; fi
    n=$(grep -c . "$RES")
    [ "$n" = "$CALLS" ] || fail "$n replies to $CALLS calls (notifications get none)"
    PHASE=check; . "$f"
    [ "$FAILS" = 0 ] ) || FAILED="$FAILED $NAME"
done
[ -z "$FAILED" ] || { echo "mcp smoke failed:$FAILED"; exit 1; }
echo "mcp smoke passed"
