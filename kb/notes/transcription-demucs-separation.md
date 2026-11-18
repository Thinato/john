---
title: The separate option runs Demucs without a shell, its stdio off the MCP streams, and finds <stem>.wav under <out>/<model>/<name>/
tags: [transcription, demucs, mcp, process, windows, testing]
---

# Source separation with Demucs (separate option)

## Context
Changing `transcribeFile()`, `demucsStem()` or `runQuiet()` in `src/song.cpp`, the `separate` option (`TranscribeOptions`, `transcribe_audio`), or running any other external tool from core.

## Rule
`runQuiet()` spawns the tool without a shell (`posix_spawnp`; `CreateProcessW` with each argument quoted by the CommandLineToArgvW rules on Windows, where `_wspawnvp` would join arguments with bare spaces) with stdin from the null device and stdout + stderr into a log file in the temp folder. `demucs --two-stems=<stem> -o <out> <in>.wav` writes `<out>/<model>/<in>/<stem>.wav` and `no_<stem>.wav` (the model folder is `htdemucs` by default and follows `-n`), so `demucsStem()` searches `<out>` for `<stem>.wav`. Demucs exits 0 when it can't read a track, so a missing stem is a failure too. Every failure falls back to the full mix with a `separation:` report line that gives the reason and the last line of the log.

## Why
Anything the child writes to an inherited stdout breaks the JSON-RPC stream, and a child that inherits stdin can read the client's next requests. A shell would split paths with spaces or quotes. Demucs works on 44.1 kHz stereo, so the section goes to it at that rate rather than at the 22050 Hz mono that transcription uses.

## How to apply
Test with a stub: a shell script set as `JOHN_DEMUCS` that writes a known WAV where Demucs would (`tests/test_core.cpp`, skipped on Windows, where a script can't be spawned). Never install Demucs on a dev machine for tests. `tests/mcp_smoke.sh` runs a stub that only prints noise and checks that none of it reaches the replies. The "not found" test empties `PATH` so that a dev machine with Demucs installed doesn't run it. The web build has no processes, so there `runQuiet()` returns -1, the same as a tool that isn't found. The desktop Import audio window (#69) offers the same option; `importAudio()` copies the `separation:` line into the song notes, and the window stays open after the import to show it (red when the full mix was used). Check that flow with a stub `JOHN_DEMUCS` that copies its input to `$3/htdemucs/<name>/<stem>.wav`, and with `JOHN_DEMUCS` set to a missing path for the fallback.
