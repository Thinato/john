# John — dev notes

- Build: `cmake --build build`; test: `ctest --test-dir build --output-on-failure` (tests/test_core.cpp asserts, tests/mcp_smoke.sh drives the server).
- One shared core (`john_core`: song.cpp, abc.cpp, analyze.cpp, sound.cpp) used by both executables. Put logic there, not in the GUI or MCP files.
- Bar/beat numbers always come from `positions()`. Accidental display always comes from `accidentals()`. Don't re-derive them.
- The MCP server speaks the initialize-based protocol (2024-11-05 .. 2025-11-25). It answers unknown methods (e.g. `server/discover`) with -32601, so modern clients fall back to `initialize`.
- Nothing but JSON-RPC may be written to the MCP server's stdout.
- Audio: all music logic (play order, schedule, synth, YIN) lives in core `sound.cpp` and is tested offline. `audio.cpp` only does device I/O. Never play sound or open the mic during automated checks; render to a buffer instead (see the pitch tests in tests/test_core.cpp).
- Check GUI changes visually with `JOHN_HOME=<tmp> JOHN_SNAPSHOT=<tmp>/s.tga ./build/john`, then convert with `sips -s format png`. Never take full-screen screenshots.
- Audio transcription (`transcribe.cpp`): harmonic-summation salience, octave-ghost suppression, Viterbi melody line, continuity filter (octave errors, accompaniment), Ellis beat tracking, sixteenth grid. Test MP3s go in `tmp/` (gitignored, copyrighted). To tune it, `JOHN_TRANSCRIBE_DEBUG=<file>` dumps the per-frame track (time, chosen MIDI, confidence, top 3 candidates, onset flux). Check every change against the synthetic mix test in tests/test_core.cpp, then by ear on real songs.
