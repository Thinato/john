// The data folder, user settings and the bundled songs (settings.cpp).
#pragma once
#include "song.hpp"

namespace john {

std::string homeDir();                            // $HOME, or %USERPROFILE% on Windows
fs::path dataDir();                         // $JOHN_HOME or ~/Documents/John
// User preferences, saved in dataDir()/settings.json and read by both the GUI and the MCP server.
// A new setting: one field here, its name in the JSON list in settings.cpp, one line in the GUI's Settings window.
struct Settings {
  bool hints = true;                // instrument hints (whistle fingering charts and fingerings)
  float zoom = 9;                   // staff space in px
  bool light = false;               // light theme instead of dark
  int tempo = 100;                  // playback tempo for songs without one
  bool countIn = true;              // one bar of clicks before Play
  bool metronome = true;
  float volume = 0.6f;
  std::string sound;                // playback sound, a SOUNDS id; "" = my instrument's
  float gate = 0.01f;               // mic level below which nothing is heard
  float latencyMs = 80;             // mic + speaker delay, for grading play-along
  float tolerance = 40;             // cents from the note that still count as right
  std::string instrument = "whistle";  // my instrument: the song list shows what fits it; new songs are for it
  std::string whistle = "D";        // my whistle's key, when instrument is "whistle"
  bool allSongs = false;            // song list shows every song, not only those that fit my instrument
  std::string language;             // viewer language: "" = automatic (system/browser), "en", "pt-BR", ...
  bool solfege = false;             // note names Do Re Mi instead of C D E
  bool solfegeKeys = true;          // key names Do Re Mi ("Ré maior"); only in a language other than English
  std::string harp = "C";           // my harmonica's key (also for songs switched to or imported as harmonica), one of HARP_KEYS
  bool allMarks = false;            // the sheet shows every technique mark, not only my instrument's (marks())
  bool lyrics = true;               // the sheet shows the song's lyrics (they are kept and exported either way)
  bool operator==(const Settings&) const = default;
};
Settings loadSettings();                    // missing keys -> defaults; unreadable file -> all defaults
void saveSettings(const Settings& s);
fs::path lessonsDir();                      // lessons/ next to the executable (release package), else the repo's
fs::path bundled(const fs::path& rel, const fs::path& built);  // rel next to the executable if it exists, else built
int installSongs(const fs::path& from);     // copy new songs from a bundled folder (lessons/, demo/), update untouched copies; returns new count

}  // namespace john
