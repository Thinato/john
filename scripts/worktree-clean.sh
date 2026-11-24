#!/bin/bash
# Removes agent worktrees (.claude/worktrees/*) whose branch is merged into origin/main (squash-merged PRs too, via gh)
# or whose remote branch is gone. Lists them; deletes only with --yes, always with plain `git worktree remove` (never
# --force), which keeps the branch itself. Never touches the main checkout, the current worktree, a locked worktree (an
# agent is using it) or one with uncommitted or untracked changes.
set -e
yes=0
[ "$1" = "--yes" ] && yes=1
here="$(git rev-parse --show-toplevel)"
git fetch --prune --quiet origin
merged_prs="$(gh pr list --state merged --limit 1000 --json headRefName,headRefOid \
  -q '.[] | .headRefName + " " + .headRefOid' 2>/dev/null || true)"

remove=()
check() {  # path branch locked
  local wt="$1" branch="$2" why="" head oid o
  case "$wt" in */.claude/worktrees/*) ;; *) return ;; esac
  if [ "$wt" = "$here" ]; then echo "skip   $wt (current worktree)"; return; fi
  if [ -n "$3" ]; then echo "skip   $wt (locked)"; return; fi
  if [ ! -d "$wt" ]; then echo "skip   $wt (missing: git worktree prune drops the entry)"; return; fi
  if [ -n "$(git -C "$wt" status --porcelain)" ]; then echo "skip   $wt (uncommitted changes)"; return; fi
  head="$(git -C "$wt" rev-parse HEAD)"
  if git merge-base --is-ancestor "$head" origin/main; then
    why="in origin/main"
  elif [ -n "$branch" ]; then
    oid="$(printf '%s\n' "$merged_prs" | awk -v b="$branch" '$1 == b {print $2}')"
    for o in $oid; do  # merged PR of this branch, and nothing committed after it
      if git merge-base --is-ancestor "$head" "$o" 2>/dev/null; then why="PR merged"; fi
    done
    if [ -z "$why" ] && [ "$(git for-each-ref --format='%(upstream:track)' "refs/heads/$branch")" = "[gone]" ]; then
      why="remote branch gone"
    fi
  fi
  if [ -n "$why" ]; then
    echo "remove $wt (${branch:-detached}: $why)"
    remove+=("$wt")
  else
    echo "keep   $wt (${branch:-detached}: not merged)"
  fi
}

wt="" branch="" locked="" first=1
while IFS= read -r line; do
  case "$line" in
    "worktree "*) wt="${line#worktree }"; branch=""; locked="" ;;
    "branch "*) branch="${line#branch refs/heads/}" ;;
    locked*) locked=1 ;;
    "") if [ $first = 0 ] && [ -n "$wt" ]; then check "$wt" "$branch" "$locked"; fi; first=0; wt="" ;;  # first: main checkout
  esac
done < <(git worktree list --porcelain; echo)

echo "${#remove[@]} to remove"
if [ $yes = 1 ]; then
  for wt in "${remove[@]}"; do git worktree remove "$wt" && echo "removed $wt"; done
elif [ ${#remove[@]} -gt 0 ]; then
  echo "Dry run: run again with --yes to remove them."
fi
