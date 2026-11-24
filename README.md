# John

In honour of my uncle John, my first and only music teacher.

John turns tunes into **tin whistle tab sheets with technique markings** (cuts, taps, rolls, crans, slides, tonguing, breaths, ...).
Turning sheet music into whistle tabs is mostly mechanical. Choosing where the ornaments go is musical judgment, so John splits the work:

- **`john-mcp`**: an MCP server. It gives an AI agent exact tools: parse ABC or MusicXML, map notes to whistle fingerings, check range and transpositions, list candidate spots for ornaments, and save the result.
- **`john`**: an ImGui viewer. It shows each tune as real staff notation, with a fingering chart, technique symbols and the agent's reasoning under every note. Hover a note for details, click it to edit its techniques. It reloads live while the agent works.

The viewer also plays the tune, listens to you through the mic, imports songs and recordings, and works for other instruments: violin, piano, guitar, harmonica and singing voices. See [More](#more).

## Get it

- **Try it in the browser:** <https://lisecki.dev/john/>. The viewer with the lessons, playback and mic; songs are kept in the browser, and you add your own with Import song (ABC, MusicXML, or a photo, scan or PDF of sheet music). No AI agent and no audio transcription.
- **Desktop app** (adds the AI agent / MCP server, audio transcription and songs on disk): download from [Releases](https://github.com/Thinato/john/releases).
  - **macOS** (11 or later, Apple silicon or Intel): download `john-macos.zip` and unzip it. The app isn't signed, so in Terminal run `xattr -dr com.apple.quarantine john-macos` once, then `cd john-macos && ./john`.
  - **Linux** (x86-64, X11 or XWayland): `tar xzf john-linux.tar.gz && cd john-linux && ./john`.
  - **Windows** (10 version 1903 or later, 64-bit): download `john-windows.zip`, right-click it, *Extract All*, then double-click `john.exe` in the `john-windows` folder. The app isn't signed, so Windows may say it protected your PC: click *More info*, then *Run anyway*.

  Keep `lessons/`, `i18n/`, `instruments/` and `fonts/` next to `john` and `john-mcp`. To connect the agent, see [below](#use-it-with-an-ai-agent).

## Use it with an AI agent

1. Register the MCP server with Claude Code (once):
   - from a downloaded app, in its folder: `claude mcp add --scope user john -- "$PWD/john-mcp"`; on Windows, in PowerShell: `claude mcp add --scope user john -- "$PWD\john-mcp.exe"`;
   - from a build: `claude mcp add --scope user john -- "$(pwd)/build/john-mcp"`.
2. Start the viewer: `./john` (or `./build/john`).
3. Ask the agent, for example: "Get Cooley's reel from thesession.org and make me a whistle sheet with ornaments."

More examples and the full list of tools: [docs/mcp-tools.md](docs/mcp-tools.md).

## Build

Needs CMake ≥ 3.24 and a C++20 compiler. The libraries and fonts are downloaded at configure time.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # core checks + MCP smoke test
```

On macOS, the first time you turn on the mic, the system asks to allow microphone access for the app you launched `john` from (Terminal, iTerm, ...).

The web version: [docs/web.md](docs/web.md#build-it). Build details and a map of the code: [docs/development.md](docs/development.md).

## More

| Doc | What's in it |
|---|---|
| [Songs and Import song](docs/songs-and-import.md) | Import ABC, MusicXML, photos and PDFs; folders; where songs are kept; the lessons |
| [Instruments](docs/instruments.md) | the song's instrument and yours, which songs fit, play in another key, harmonica, singing voices, custom instruments, technique marks |
| [Practice](docs/practice-and-mic.md) | playback, the mic tuner, wait for me, play along |
| [Transcription](docs/transcription.md) | Import audio: a draft sheet from a recording, Demucs, rubato |
| [MCP tools](docs/mcp-tools.md) | what to ask the agent, and its tools |
| [Settings and languages](docs/settings-and-languages.md) | every setting, English and Portuguese, translated songs |
| [Web version](docs/web.md) | what the browser version has, how to build it, the image reader |
| [Development](docs/development.md) | build details, how the code is laid out, dev aids |

Contributors: see [CONTRIBUTING.md](CONTRIBUTING.md). Lessons learned about the code are in the searchable knowledge base, [`kb/`](kb/README.md).

## License

MIT, see [LICENSE](LICENSE). Third-party parts (fonts, libraries) keep their own licences.

The web version's image reader is built from copisteria and the copista-2m model by KobiMusic (Apache-2.0; `web/omr/copisteria/` is the JavaScript port from [brunombo/OpticalMusicRecognition](https://github.com/brunombo/OpticalMusicRecognition), MusicOCR's code MIT), ONNX Runtime Web (MIT) and PDF.js (Apache-2.0). [`web/omr/NOTICE.txt`](web/omr/NOTICE.txt) lists them with their notices and licence files; the site serves it at `omr/NOTICE.txt` (*Licences of the music reader* in the Import song window).
