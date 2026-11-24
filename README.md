# John

In honour of my uncle John, my first and only music teacher.

John turns tunes into **tin whistle tab sheets with technique markings** (cuts, taps, rolls, crans, slides, tonguing, breaths, ...).
Turning sheet music into whistle tabs is mostly mechanical. Choosing where the ornaments go is musical judgment, so John splits the work:

- **`john-mcp`**: an MCP server. It gives an AI agent exact tools: parse ABC or MusicXML, map notes to whistle fingerings, check range and transpositions, list candidate spots for ornaments, and save the result.
- **`john`**: an ImGui viewer. It shows each tune as real staff notation, with a fingering chart, technique symbols and the agent's reasoning under every note. Hover a note for details, click it to edit its techniques. It reloads live while the agent works.
  - **Play** (or Space): a synthesized voice of your instrument (Settings → My instrument) plays the tune, in the octave it would sound in performance order, through repeats and endings, with its cuts, rolls, slides, vibrato and breaths. The *sound* menu switches to another instrument's voice (same notes and pitch). A one-bar count-in comes first (it can be turned off in Settings), and you can add a metronome. The tempo slider changes the speed, even while playing. Playback starts from the selected note.
  - **Mic**: a live tuner showing the note you play, how far off it is in cents, and its fingering.
    - **Wait for me**: the cursor waits on each note until you play it in tune.
    - **Play along**: grades each note while the backing plays. Green is right, red is wrong, orange is the wrong octave. Use headphones, or untick *Melody* and keep the metronome, so the mic doesn't hear the backing.
    - Right-click *Reset marks* to set the mic noise gate and latency (also in Settings).
  - **Import audio** (button in the sidebar, desktop only): drop an mp3, wav or flac file on the window, or paste its path, and press *Transcribe*. Optional: start and end (seconds), tempo and meter (blank = detect), the instrument the sheet is for (default from Settings) the instrument playing the melody in the recording, *Separate the melody (Demucs)* for a dense mix (vocals or another lead instrument; needs `pip install demucs` or `JOHN_DEMUCS`, slow, so use start/end for a short section), and *Rubato* for music whose tempo drifts. John keeps working while it listens; the draft opens as a new song in *Unsorted*, with the detected tempo, meter and key in its notes (and whether Demucs ran, which the window also shows) and "unclear in the recording, check by ear" on doubtful notes. In a whistle draft, cuts, taps and rolls John hears are marked on their notes as candidates. Under each line of music a strip shows the pitch John heard in the recording, faint where it was unsure. It's a machine draft: compare it with the recording by ear, or ask the agent to tidy it.
  - **Import song** (button in the sidebar, web and desktop): paste ABC notation (from thesession.org, abcnotation.com, ...) and press *Import*, or open an `.abc` file or a MusicXML score (`.musicxml`, `.xml` or zipped `.mxl`, as MuseScore, Finale, Sibelius and flat.io export it) (web: *Open a file...*; desktop: type its path or drop it on the window). Pick the instrument the sheet is for at the top of the window (with the whistle or harmonica key; it starts on the Settings instrument and is remembered until you close John). Every ABC tune (`X:`) becomes a song in *Unsorted* for that instrument, and if many notes fall outside its range the window suggests *Play in key*; a MusicXML score becomes one song from its first part and voice, chords reduced to the top note. Lyrics (ABC `w:`, MusicXML `<lyric>`, first verse) are shown under the notes. Warnings are listed in the window and the first song opens. No agent needed.
    - **Images and PDFs** (web only for now): *Open a file...* also takes a photo, scan or PDF of sheet music (PNG, JPG, WebP; HEIC where the browser opens it). A music reader that runs in the browser ([copisteria](https://github.com/kobimusic/copisteria) with the copista-2m model) reads every page in order into one song, and the image never leaves the device. The first time it downloads about 22 MB (kept by the browser afterwards), with a progress bar and *Cancel*. The first page is shown next to the sheet (*Show the original image*) so you can compare: clean printed scores read best, photos and handwriting worse. Small scans are enlarged 2x before reading, and when only the first line comes out without the key signature of the rest, John gives it that key and says so.
  - **Settings** (button in the sidebar): language, note names (C D E or Do Re Mi) and, in Portuguese, key names ("Ré maior" or "D maior"), instrument hints, zoom, light or dark theme, the tempo for songs without one, count-in, metronome, volume, playback sound, mic tolerance, noise gate and latency, the instrument new songs get when the agent doesn't name one, and your own instruments (see [Instruments](#instruments)). Changes apply at once and are kept between sessions. The controls in the main window (hints, zoom, sound, metronome, volume, tolerance) change the same settings.

## Get it

- **Try it in the browser:** <https://lisecki.dev/john/>. The viewer with the lessons, playback and mic; songs are kept in the browser, and you add your own with Import song (ABC, MusicXML, or a photo, scan or PDF of sheet music). No AI agent and no audio transcription.
- **Desktop app** (adds the AI agent / MCP server, audio transcription and songs on disk): download from [Releases](https://github.com/Thinato/john/releases).
  - **macOS** (11 or later, Apple silicon or Intel): download `john-macos.zip` and unzip it. The app isn't signed, so in Terminal run `xattr -dr com.apple.quarantine john-macos` once, then `cd john-macos && ./john`.
  - **Linux** (x86-64, X11 or XWayland): `tar xzf john-linux.tar.gz && cd john-linux && ./john`.
  - **Windows** (10 version 1903 or later, 64-bit): download `john-windows.zip`, right-click it, *Extract All*, then double-click `john.exe` in the `john-windows` folder. The app isn't signed, so Windows may say it protected your PC: click *More info*, then *Run anyway*.

  Keep `lessons/`, `i18n/`, `instruments/` and `fonts/` next to `john` and `john-mcp`. To connect the agent, run `claude mcp add --scope user john -- "$PWD/john-mcp"` in that folder (see [Use](#use)); on Windows, in PowerShell: `claude mcp add --scope user john -- "$PWD\john-mcp.exe"`.

## Build

Needs CMake ≥ 3.24 and a C++20 compiler. GLFW, Dear ImGui, nlohmann/json, miniaudio, the Bravura music font and the Noto Sans text font (both SIL Open Font License) are downloaded at configure time.
On macOS, the first time you turn on the mic, the system asks to allow microphone access for the app you launched `john` from (Terminal, iTerm, ...).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # core checks + MCP smoke test
```

### Web version

The viewer also runs in the browser, with no server: songs, techniques, custom instruments and settings are kept in the browser's storage (IndexedDB), and the lessons are bundled, plus a dozen starter songs from `demo/` (traditional and public-domain tunes in Beginner, Intermediate and Advanced, installed on the first visit). There is no MCP server or audio-file transcription on the web. Needs [Emscripten](https://emscripten.org/) (`emcc` on the PATH):

```sh
emcmake cmake -S . -B build-web
cmake --build build-web       # john.html, john.js, john.wasm, john.data, and omr/ (the image reader)
python3 -m http.server -d build-web 8000
```

Then open <http://localhost:8000/john.html>. Sound starts on the first click; the mic needs `localhost` or HTTPS.

The image reader (`web/omr/`) needs no npm or bundler: at configure time CMake downloads the copista-2m model (its ONNX export, mirrored on this repo's `models-v1` release), ONNX Runtime Web and PDF.js at pinned versions and checks their SHA-256, into `build-web/omr/`. Pages sends no COOP/COEP headers, so the reader uses WebGPU where the browser has it and single-threaded WASM otherwise.

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

Each song is for an instrument: tin whistle (the default unless another is picked in Settings, in any key), violin, piano, guitar, electric guitar, harmonica or a singing voice (soprano, alto, tenor, bass). The song's instrument says what the sheet was written for. Everything about playing follows *your* instrument (Settings → My instrument): fingering charts (for a whistle, in your whistle's key; untick "Instrument hints" in the sidebar to hide them), red out-of-range notes, the playback voice and octave, and the mic, which expects the octave your instrument sounds in (a whistle on a fiddle tune sounds an octave above the written notes, and that counts as right). Pick it with the Written for picker in the GUI, or tell the agent. Songs are melodies, though: the song list shows the songs that fit *your* instrument (Settings → My instrument, with its whistle or harmonica key), in any key. A song that only fits in another key gets a small badge ("in G", "octave up"), and opening it shows a one-click "Play in G major" button. "Fits" means no note out of range and fewer than 10 % hard notes (whistle half-holes, harmonica bends; a cross-fingered C counts a fifth, an overblow three). Pick "All songs" in the menu above the list to see everything. The agent's `list_songs` and `analyze_song` report the same verdict, for your instrument or any other.

The harmonica is a 10-hole diatonic (Richter) in any key (G to F#; pick yours under Settings, or per song with the Harmonica key picker); its range follows the harp, so an A harp starts on A3. Blow and draw notes are easy; notes you can only get by bending are hard, overblows and overdraws harder (they count three), so a tune in the bottom octave of the harp often fits only an octave up. For harmonica songs the details panel shows the usual keys for that harp (1st position = the harp's key, 2nd position = a fifth above). To play a song in another key, use the Play in key picker: it transposes the song by the smallest step, picks the octave that fits the instrument best, and saves it. The original key is marked, and picking it brings back the original sheet.
Singers pick a voice as their instrument: soprano (C4 to A5), alto (G3 to E5), tenor (C3 to A4) or bass (E2 to E4). Tenor and bass music is written an octave above how it sounds, on the treble staff, as in choir books. With a voice picked, Settings → My instrument shows **Find my range**. Sing your lowest comfortable note for about three seconds, then your highest. John shows each note on a small staff, and you can retry either one. *Save* stores the range as your own instrument, *My voice*, and picks it, so the song list shows the songs that fit your voice. Run it again any time. With the mic on, a note sung an octave off counts as right, because singers pick their own octave. During playback and play-along, the syllable being sung is highlighted. Voices list breaths, slides and vibrato. The tunes are played on the violin sound with breaths, since John has no singing synth.

### Instruments

Each instrument is one JSON file: its name, lowest and highest written note, the octave it sounds in, its key, which notes are easy, hard (half-hole, cross-fingering, bend, overblow) or impossible, the techniques it can play, the voice that plays it, and `"voice": true` for a singing voice (the mic accepts any octave). John's own instruments are in `instruments/`. Your own go in `~/Documents/John/instruments/<id>.json`: make them under Settings → Custom instruments (name, lowest and highest note, key, *Chromatic* or *Diatonic* to fill the notes, then click a note to make it easy, hard or impossible; techniques; sound), or ask the agent ("I play a C flute, low D to high A": `define_instrument`, `list_instruments`, `delete_instrument`). They show up in My instrument, Written for and Import song. A file with the id of one of John's instruments replaces it; a broken file is skipped with a warning in Settings. Deleting an instrument asks first and says how many songs are written for it; those songs keep their notes and show "Written for: <name> (deleted)". Fingering charts exist only for the whistle.

The sheet shows the technique marks that **your** instrument (Settings) can play. A mark it can't play is shown and played as its equivalent when the profile has one (`instead`: the piano plays cuts and taps as grace notes and rolls as turns, the harmonica plays rolls as shakes), and hidden otherwise; the legend says how many are hidden, and *Show all marks* (legend or Settings) brings them back. Breath marks always show; on instruments that don't breathe the legend calls them a phrase end (lift). The song file keeps every mark, so switching back to the whistle shows the original ones. Custom instruments set `instead` in the editor's *Played instead* section or with `define_instrument`.

Songs are grouped into folders (Lessons, Beginner, Intermediate, Advanced, or any name). The agent files new songs by difficulty, and you can move a song with the Folder picker in the GUI.

Songs live in `~/Documents/John/songs/*.json` (on Windows `%USERPROFILE%\Documents\John`; override with `JOHN_HOME`); songs made with Import audio keep their pitch track in `tracks/<id>.json` next to `songs/`. Custom techniques are in `~/Documents/John/techniques.json`, settings in `~/Documents/John/settings.json`.

## Languages

The viewer speaks English and Brazilian Portuguese. By default it follows the system language (`LANG`/`LC_ALL`; on macOS, when those are empty as for an app started from Finder, the first language in System Settings > Language & Region; on Windows, when those are empty, the Windows display language; the browser's language on the web); pick one in Settings. The MCP server and the agent's tools always answer in English. Translations are in `i18n/<language>/`, one folder per language with a file per area, mapping each English text to its translation; to add a language, add a folder (see `kb/notes/i18n-translate-viewer-text.md`).

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
| Model, storage, agent listing | `src/song.cpp`, `src/song.hpp` (`src/john.hpp` includes every area's header) |
| Theory (keys, spelling, fingerings), transposition, bars, score layout | `src/theory.cpp` |
| Data folder, settings, bundled lessons and demo songs | `src/settings.cpp` |
| ABC importer (thesession.org bodies, grace-note cuts/taps, `!decorations!`, tuplets, voltas, `w:` lyrics) | `src/abc.cpp` |
| Hints: range/transposition ranking, cut/roll/cran/slide/breath/tongue candidates | `src/analyze.cpp` |
| Play order (repeats/endings), backing-whistle synth, YIN pitch detection | `src/sound.cpp` |
| Audio transcription (pitch track, beats, meter, key) and Import audio | `src/transcribe.cpp`, `src/import_audio.cpp` |
| Speaker/microphone devices (miniaudio) | `src/audio.cpp` |
| MCP server (stdio JSON-RPC, tools + agent guide in `instructions`) | `src/mcp_main.cpp`, tools in `src/mcp_*.cpp` |
| Viewer (Bravura SMuFL glyphs drawn with ImDrawList) | `src/gui_main.cpp`, `src/gui_*.cpp` (score, panels, practice, settings, import) |
| Viewer translations (English text -> translation) | `i18n/<language>/*.json`, `tr()` in `src/i18n.cpp` |
| Instrument profiles (range, note difficulty, techniques, sound), technique catalog | `instruments/*.json`, `src/catalog.cpp` |
| Web build: IndexedDB storage and page | `web/pre.js`, `web/shell.html` |
| Web image import: the reader (Web Worker) and its copisteria port, the first-line key check | `web/omr/`, `checkFirstLineKey()` in `src/musicxml.cpp` |

Notes are stored at written pitch: whistle music is written an octave below how it sounds, so a D whistle's bottom note is D4.
Durations are in ticks (192 per whole note).

Contributors: see [CONTRIBUTING.md](CONTRIBUTING.md). Lessons learned about the code are in the searchable knowledge base, [`kb/`](kb/README.md).

Dev aid: `JOHN_SNAPSHOT=out.tga ./build/john` renders a few frames, writes the window to a TGA file and exits.

## License

MIT, see [LICENSE](LICENSE). Third-party parts (fonts, libraries) keep their own licences.

The web version's image reader is built from copisteria and the copista-2m model by KobiMusic (Apache-2.0; `web/omr/copisteria/` is the JavaScript port from [brunombo/OpticalMusicRecognition](https://github.com/brunombo/OpticalMusicRecognition), MusicOCR's code MIT), ONNX Runtime Web (MIT) and PDF.js (Apache-2.0). [`web/omr/NOTICE.txt`](web/omr/NOTICE.txt) lists them with their notices and licence files; the site serves it at `omr/NOTICE.txt` (*Licences of the music reader* in the Import song window).
