# Contributing to John

Work is planned in [GitHub Issues](https://github.com/Thinato/john/issues). `main` only changes through pull requests, and CI (build + tests on Linux and macOS) must pass before merging.

## Picking an issue

- Issues labelled `ready` have a settled plan. Others may still need discussion: ask in the issue first.
- Comment on the issue when you start, so two people (or agents) don't work on the same thing.
- One issue per branch and per PR. Name the branch `issue-<number>-<short-name>`, for example `issue-16-hints-toggle`.

## Working on it

- Build and test: see the README. Dev notes and code rules are in [CLAUDE.md](CLAUDE.md). Read them, they apply to everyone.
- Keep the change to what the issue asks. Found something else? Open a new issue instead of growing the PR.
- If the plan in the issue turns out wrong or unclear, say so in the issue or the PR rather than guessing.

## Definition of done

A PR is ready for review when:

- [ ] It says `Closes #<number>` and explains what changed in a few plain sentences.
- [ ] `scripts/check.sh` passes locally (build, ctest, kb check; `--web` adds the web build), and new logic has a test in `tests/`.
- [ ] GUI changes were checked with a snapshot (`JOHN_SNAPSHOT`, see CLAUDE.md): say what it shows, and attach the image if you can.
- [ ] README / CLAUDE.md / the MCP agent instructions are updated if behaviour changed.
- [ ] Anything non-obvious you learned about the codebase is written down as a note in `kb/notes/` (see `kb/README.md`).
- [ ] No copyrighted recordings or sheets, no personal paths, names or emails, no secrets.
- [ ] Anything someone has to hear, play or see to judge (sounds, mic, layout) is labelled `needs-human-check`.

## Review and merge

- The reviewer comments on the PR. Fix the comments in new commits on the same branch.
- PRs are squash-merged once CI is green and review comments are resolved.
- PRs labelled `needs-human-check` wait until someone has tried them.
