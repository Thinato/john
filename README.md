# John

In honour of my uncle John, my first and only music teacher.

John turns tunes into **tin whistle tab sheets with technique markings** (cuts, taps, rolls, crans, slides, tonguing, breaths, ...).
Turning sheet music into whistle tabs is mostly mechanical. Choosing where the ornaments go is musical judgment, so John splits the work:

- **`john-mcp`**: an MCP server. It gives an AI agent exact tools: parse ABC, map notes to whistle fingerings, check range and transpositions, list candidate spots for ornaments, and save the result.
- **`john`**: an ImGui viewer. It shows each tune as real staff notation, with a fingering chart, technique symbols and the agent's reasoning under every note. Hover a note for details, click it to edit its techniques. It reloads live while the agent works.
  - **Play** (or Space): a synthesized whistle plays the tune in performance order, through repeats and endings, with its cuts, rolls, slides, vibrato and breaths. A one-bar count-in comes first, and you can add a metronome. The tempo slider changes the speed, even while playing. Playback starts from the selected note.
  - **Mic**: a live tuner showing the note you play, how far off it is in cents, and its fingering.
    - **Wait for me**: the cursor waits on each note until you play it in tune.
    - **Play along**: grades each note while the backing plays. Green is right, red is wrong, orange is the wrong octave. Use headphones, or untick *Whistle* and keep the metronome, so the mic doesn't hear the backing.
    - Right-click *Reset marks* to set the mic noise gate and latency.

## Build

Needs CMake ≥ 3.24 and a C++20 compiler. GLFW, Dear ImGui, nlohmann/json, miniaudio and the Bravura music font are downloaded at configure time.
On macOS, the first time you turn on the mic, the system asks to allow microphone access for the app you launched `john` from (Terminal, iTerm, ...).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # core checks + MCP smoke test
```

## Use

1. Register the MCP server with Claude Code (once):
   `claude mcp add --scope user john -- "$(pwd)/build/john-mcp"`
2. Start the viewer: `./build/john`
3. Ask the agent, for example:
   - "Get Cooley's reel from thesession.org and make me a whistle sheet with ornaments."
   - "Here's a photo of a sheet (attach it). Convert it to tin whistle tabs with techniques."
   - "These are tabs for <tune>: D E F# G ... Where should the cuts and rolls go?"
   - "Document a technique called 'bounce': ..." (adds it to the catalog and legend)

Songs are grouped into folders (Lessons, Beginner, Intermediate, Advanced, or any name). The agent files new songs by difficulty, and you can move a song with the Folder picker in the GUI.

Songs live in `~/Documents/John/songs/*.json` (override with `JOHN_HOME`). Custom techniques are in `~/Documents/John/techniques.json`.

## Lessons

`lessons/` holds a beginner course for the D whistle. John installs it into your song list the first time it runs (delete a lesson and it stays deleted). Each lesson has comments on the notes explaining what to practise:

1. **Breath and tongue**: long tones, tonguing, slurring (low octave)
2. **First high notes**: octave jumps on G, A and B, the easiest second-octave notes
3. **High F#, E and D**: the harder bottom-hand high notes
4. **Across the break**: the two-octave scale and the C# to high D move
5. **Ode to Joy**: Beethoven, almost entirely in the high octave
6. **Cuts**: the core Irish ornament, in both octaves
7. **Amazing Grace**: slides, vibrato and breathing in a slow air
8. **John's Air**: an original piece that combines all of the above

Lessons 5 and 7 are public-domain tunes; the rest are original exercises.

## How it works

| Piece | File |
|---|---|
| Model, theory (keys, spelling, fingerings), storage, agent listing | `src/song.cpp`, `src/john.hpp` |
| ABC importer (thesession.org bodies, grace-note cuts/taps, `!decorations!`, tuplets, voltas) | `src/abc.cpp` |
| Hints: range/transposition ranking, cut/roll/cran/slide/breath/tongue candidates | `src/analyze.cpp` |
| Play order (repeats/endings), backing-whistle synth, YIN pitch detection | `src/sound.cpp` |
| Speaker/microphone devices (miniaudio) | `src/audio.cpp` |
| MCP server (stdio JSON-RPC, tools + agent guide in `instructions`) | `src/mcp_main.cpp` |
| Viewer (Bravura SMuFL glyphs drawn with ImDrawList) | `src/gui_main.cpp` |

Notes are stored at written pitch: whistle music is written an octave below how it sounds, so a D whistle's bottom note is D4.
Durations are in ticks (192 per whole note).

Dev aid: `JOHN_SNAPSHOT=out.tga ./build/john` renders a few frames, writes the window to a TGA file and exits.
