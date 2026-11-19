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
  - **Import audio** (button in the sidebar, desktop only): drop an mp3, wav or flac file on the window, or paste its path, and press *Transcribe*. Optional: start and end (seconds), tempo and meter (blank = detect), the instrument the sheet is for (default from Settings) the instrument playing the melody in the recording, *Separate the melody (Demucs)* for a dense mix (vocals or another lead instrument; needs `pip install demucs` or `JOHN_DEMUCS`, slow, so use start/end for a short section), and *Rubato* for music whose tempo drifts. John keeps working while it listens; the draft opens as a new song in *Unsorted*, with the detected tempo, meter and key in its notes (and whether Demucs ran, which the window also shows) and "unclear in the recording, check by ear" on doubtful notes. In a whistle draft, cuts, taps and rolls John hears are marked on their notes as candidates. Under each line of music a strip shows the pitch John heard in the recording, faint where it was unsure. It's a machine draft: compare it with the recording by ear, or ask the agent to tidy it.
  - **Import song** (button in the sidebar, web and desktop): paste ABC notation (from thesession.org, abcnotation.com, ...) and press *Import*, or open an `.abc` file (web: *Open a file...*; desktop: type its path or drop it on the window). Every tune (`X:`) becomes a song in *Unsorted* for the Settings instrument; ABC warnings are listed in the window and the first tune opens. No agent needed.
  - **Settings** (button in the sidebar): language, note names (C D E or Do Re Mi) and, in Portuguese, key names ("Ré maior" or "D maior"), instrument hints, zoom, light or dark theme, the tempo for songs without one, count-in, metronome, volume, playback sound, mic tolerance, noise gate and latency, and the instrument new songs get when the agent doesn't name one. Changes apply at once and are kept between sessions. The controls in the main window (hints, zoom, sound, metronome, volume, tolerance) change the same settings.

## Get it

- **Try it in the browser:** <https://lisecki.dev/john/>. The viewer with the lessons, playback and mic; songs are kept in the browser, and you add your own with Import song (ABC). No AI agent and no audio transcription.
- **Desktop app** (adds the AI agent / MCP server, audio transcription and songs on disk): download from [Releases](https://github.com/Thinato/john/releases).
  - **macOS** (11 or later, Apple silicon or Intel): download `john-macos.zip` and unzip it. The app isn't signed, so in Terminal run `xattr -dr com.apple.quarantine john-macos` once, then `cd john-macos && ./john`.
  - **Linux** (x86-64, X11 or XWayland): `tar xzf john-linux.tar.gz && cd john-linux && ./john`.
  - **Windows** (10 version 1903 or later, 64-bit): download `john-windows.zip`, right-click it, *Extract All*, then double-click `john.exe` in the `john-windows` folder. The app isn't signed, so Windows may say it protected your PC: click *More info*, then *Run anyway*.

  Keep `lessons/`, `i18n/` and `fonts/` next to `john` and `john-mcp`. To connect the agent, run `claude mcp add --scope user john -- "$PWD/john-mcp"` in that folder (see [Use](#use)); on Windows, in PowerShell: `claude mcp add --scope user john -- "$PWD\john-mcp.exe"`.

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

The agent can also transcribe an audio file (`transcribe_audio`). For a singer or lead over a full band, it can first separate the melody with [Demucs](https://github.com/adefossez/demucs), an optional tool you install yourself (`pip install demucs`, or set `JOHN_DEMUCS` to its path). It is slow, so give it short sections; use only short sections of copyrighted recordings, for personal practice. Without Demucs the full mix is transcribed. For slow airs, film music and other playing that speeds up and slows down, `rubato` makes the beats follow the recording's tempo.

Each song is for an instrument: tin whistle (the default unless another is picked in Settings, in any key), violin, piano, guitar, electric guitar or harmonica. Range checks, transposition hints, playback pitch and the mic follow the song's instrument; fingering charts are shown for the whistle only (untick "Instrument hints" in the sidebar to hide them). Pick it with the Instrument picker in the GUI, or tell the agent. The song list shows the instrument after each title (nothing for a D whistle), and the menu above the list shows only the songs for one instrument; the agent's `list_songs` can filter the same way.

The harmonica is a 10-hole diatonic in any key (G to F#; pick yours under Settings, or per song with the Harmonica key picker); its range follows the harp, so an A harp starts on A3. For harmonica songs the details panel shows the usual keys for that harp (1st position = the harp's key, 2nd position = a fifth above). To play a song in another key, use the Play in key picker: it transposes the song by the smallest step, picks the octave that fits the instrument best, and saves it. The original key is marked, and picking it brings back the original sheet.

Songs are grouped into folders (Lessons, Beginner, Intermediate, Advanced, or any name). The agent files new songs by difficulty, and you can move a song with the Folder picker in the GUI.

Songs live in `~/Documents/John/songs/*.json` (on Windows `%USERPROFILE%\Documents\John`; override with `JOHN_HOME`); songs made with Import audio keep their pitch track in `tracks/<id>.json` next to `songs/`. Custom techniques are in `~/Documents/John/techniques.json`, settings in `~/Documents/John/settings.json`.

## Languages

The viewer speaks English and Brazilian Portuguese. By default it follows the system language (`LANG`/`LC_ALL`; on macOS, when those are empty as for an app started from Finder, the first language in System Settings > Language & Region; on Windows, when those are empty, the Windows display language; the browser's language on the web); pick one in Settings. The MCP server and the agent's tools always answer in English. Translations are in `i18n/<language>.json`, one file per language, mapping each English text to its translation; to add a language, add a file (see `kb/notes/i18n-translate-viewer-text.md`).

The lessons and the starter songs carry their own Portuguese title, notes and comments (an `"i18n"` map inside each song file), shown when the viewer is in Portuguese. Lessons installed before this was added are your own copies and stay in English; new installs and new browsers get the translations. Your own songs, and anything the agent writes, stay in the language they were written in.

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
| Audio transcription (pitch track, beats, meter, key) and Import audio | `src/transcribe.cpp`, `importAudio()` in `src/song.cpp` |
| Speaker/microphone devices (miniaudio) | `src/audio.cpp` |
| MCP server (stdio JSON-RPC, tools + agent guide in `instructions`) | `src/mcp_main.cpp` |
| Viewer (Bravura SMuFL glyphs drawn with ImDrawList) | `src/gui_main.cpp` |
| Viewer translations (English text -> translation) | `i18n/*.json`, `tr()` in `src/song.cpp` |
| Web build: IndexedDB storage and page | `web/pre.js`, `web/shell.html` |

Notes are stored at written pitch: whistle music is written an octave below how it sounds, so a D whistle's bottom note is D4.
Durations are in ticks (192 per whole note).

Contributors: see [CONTRIBUTING.md](CONTRIBUTING.md). Lessons learned about the code are in the searchable knowledge base, [`kb/`](kb/README.md).

Dev aid: `JOHN_SNAPSHOT=out.tga ./build/john` renders a few frames, writes the window to a TGA file and exits.

## License

MIT, see [LICENSE](LICENSE). Third-party parts (fonts, libraries) keep their own licences.
