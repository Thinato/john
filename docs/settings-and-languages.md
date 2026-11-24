# Settings and languages

## Settings

**Settings** is a button in the sidebar. It has:

- language;
- note names: C D E or Do Re Mi;
- in Portuguese, key names: "Ré maior" or "D maior";
- instrument hints;
- zoom;
- light or dark theme;
- the tempo for songs without one;
- count-in;
- metronome;
- volume;
- playback sound;
- mic tolerance, noise gate and latency;
- the instrument new songs get when the agent doesn't name one;
- your own instruments (see [Instruments](instruments.md#custom-instruments)).

Changes apply at once and are kept between sessions. The controls in the main window (hints, zoom, sound, metronome, volume, tolerance) change the same settings.

On the desktop, settings are kept in `~/Documents/John/settings.json`. On the web, in the browser.

## Languages

The viewer speaks English and Brazilian Portuguese. Pick one in Settings.

By default, it follows the system language:

- `LANG` / `LC_ALL`;
- on macOS, when those are empty (as for an app started from Finder): the first language in System Settings > Language & Region;
- on Windows, when those are empty: the Windows display language;
- on the web: the browser's language.

The MCP server and the agent's tools always answer in English.

Translations are in `i18n/<language>/`: one folder per language, with a file per area, mapping each English text to its translation. To add a language, add a folder (see [i18n-translate-viewer-text.md](../kb/notes/i18n-translate-viewer-text.md)).

## Translated songs

The lessons and the starter songs carry their own Portuguese title, notes and comments (an `"i18n"` map inside each song file). They are shown when the viewer is in Portuguese.

- Lessons installed before this was added are your own copies, and stay in English.
- New installs and new browsers get the translations.
- Your own songs, and anything the agent writes, stay in the language they were written in.
