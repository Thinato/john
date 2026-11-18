# John

In honour of my uncle John, my first and only music teacher.

John turns tunes into **tin whistle tab sheets with technique markings** (cuts, taps, rolls, crans, slides, tonguing, breaths, ...).
Turning sheet music into whistle tabs is mostly mechanical. Choosing where the ornaments go is musical judgment, so John splits the work:

- **`john-mcp`**: an MCP server. It gives an AI agent exact tools: parse ABC, map notes to whistle fingerings, check range and transpositions, list candidate spots for ornaments, and save the result.
- **`john`**: an ImGui viewer. It shows each tune as real staff notation, with a fingering chart, technique symbols and the agent's reasoning under every note. Hover a note for details, click it to edit its techniques. It reloads live while the agent works.
  - **Play** (or Space): a synthesized voice of the song's instrument plays the tune in performance order, through repeats and endings, with its cuts, rolls, slides, vibrato and breaths. The *sound* menu switches to another instrument's voice (same notes and pitch). A one-bar count-in comes first (it can be turned off in Settings), and you can add a metronome. The tempo slider changes the speed, even while playing. Playback starts from the selected note.
  - **Mic**: a live tuner showing the note you play, how far off it is in cents, and its fingering.
    - **Wait for me**: the cursor waits on each note until you play it in tune.
    - **Play along**: grades each note while the backing plays. Green is right, red is wrong, orange is the wrong octave. Use headphones, or untick *Melody* and keep the metronome, so the mic doesn't hear the backing.
    - Right-click *Reset marks* to set the mic noise gate and latency (also in Settings).
  - **Settings** (button in the sidebar): instrument hints, zoom, light or dark theme, the tempo for songs without one, count-in, metronome, volume, playback sound, mic tolerance, noise gate and latency, and the instrument new songs get when the agent doesn't name one. Changes apply at once and are kept between sessions. The controls in the main window (hints, zoom, sound, metronome, volume, tolerance) change the same settings.

## Get it

- **Try it in the browser:** <https://thinato.github.io/john/>. The viewer with the lessons, playback and mic; songs are kept in the browser. No AI agent and no audio transcription.
- **Desktop app** (adds the AI agent / MCP server, audio transcription and songs on disk): download from [Releases](https://github.com/Thinato/john/releases).
  - **macOS** (11 or later, Apple silicon or Intel): download `john-macos.zip` and unzip it. The app isn't signed, so in Terminal run `xattr -dr com.apple.quarantine john-macos` once, then `cd john-macos && ./john`.
  - **Linux** (x86-64, X11 or XWayland): `tar xzf john-linux.tar.gz && cd john-linux && ./john`.
  - **Windows**: not yet ([#50](https://github.com/Thinato/john/issues/50)).

  Keep `lessons/` and `fonts/` next to `john` and `john-mcp`. To connect the agent, run `claude mcp add --scope user john -- "$PWD/john-mcp"` in that folder (see [Use](#use)).

## Build

Needs CMake ≥ 3.24 and a C++20 compiler. GLFW, Dear ImGui, nlohmann/json, miniaudio, the Bravura music font and the Noto Sans text font (both SIL Open Font License) are downloaded at configure time.
On macOS, the first time you turn on the mic, the system asks to allow microphone access for the app you launched `john` from (Terminal, iTerm, ...).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # core checks + MCP smoke test
```

### Web version

The viewer also runs in the browser, with no server: songs, techniques and settings are kept in the browser's storage (IndexedDB), and the lessons are bundled, plus a dozen starter songs from `demo/` (traditional and public-domain tunes in Beginner, Intermediate and Advanced, installed on the first visit). There is no MCP server or audio-file transcription on the web. Needs [Emscripten](https://emscripten.org/) (`emcc` on the PATH):

```sh
emcmake cmake -S . -B build-web
cmake --build build-web       # john.html, john.js, john.wasm, john.data
python3 -m http.server -d build-web 8000
```

Then open <http://localhost:8000/john.html>. Sound starts on the first click; the mic needs `localhost` or HTTPS.

## Use

1. Register the MCP server with Claude Code (once):
   `claude mcp add --scope user john -- "$(pwd)/build/john-mcp"`
2. Start the viewer: `./build/john`
3. Ask the agent, for example:
   - "Get Cooley's reel from thesession.org and make me a whistle sheet with ornaments."
   - "Here's a photo of a sheet (attach it). Convert it to tin whistle tabs with techniques."
   - "These are tabs for <tune>: D E F# G ... Where should the cuts and rolls go?"
   - "Document a technique called 'bounce': ..." (adds it to the catalog and legend)

Each song is for an instrument: tin whistle (the default unless another is picked in Settings, in any key), violin, piano, guitar, electric guitar or harmonica. Range checks, transposition hints, playback pitch and the mic follow the song's instrument; fingering charts are shown for the whistle only (untick "Instrument hints" in the sidebar to hide them). Pick it with the Instrument picker in the GUI, or tell the agent. The song list shows the instrument after each title (nothing for a D whistle), and the menu above the list shows only the songs for one instrument; the agent's `list_songs` can filter the same way.

Songs are grouped into folders (Lessons, Beginner, Intermediate, Advanced, or any name). The agent files new songs by difficulty, and you can move a song with the Folder picker in the GUI.

Songs live in `~/Documents/John/songs/*.json` (override with `JOHN_HOME`). Custom techniques are in `~/Documents/John/techniques.json`, settings in `~/Documents/John/settings.json`.

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
| Web build: IndexedDB storage and page | `web/pre.js`, `web/shell.html` |

Notes are stored at written pitch: whistle music is written an octave below how it sounds, so a D whistle's bottom note is D4.
Durations are in ticks (192 per whole note).

Contributors: see [CONTRIBUTING.md](CONTRIBUTING.md). Lessons learned about the code are in the searchable knowledge base, [`kb/`](kb/README.md).

Dev aid: `JOHN_SNAPSHOT=out.tga ./build/john` renders a few frames, writes the window to a TGA file and exits.
