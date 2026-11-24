// Songs: the model (events, key, song) and its storage in the data folder (song.cpp).
#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace john {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr int WHOLE = 192;  // ticks per whole note (divisible by triplet 32nds)

enum class Kind { Note, Rest, Bar };

struct Event {
  Kind kind = Kind::Note;
  int midi = 0;          // Note only; written pitch (whistle music is written an octave low, D whistle low D = D4 = 62)
  int len = 0;           // Note/Rest: sounding length in ticks
  bool tie = false;      // Note: tied to the next note
  int tuplet = 0;        // Note/Rest inside a tuplet: p of (p:q
  int tupletQ = 0;       // q of (p:q
  bool tupletStart = false;
  std::vector<std::string> techniques;  // technique ids from the catalog
  std::string comment;   // why the techniques were chosen (shown in the GUI)
  std::string bar;       // Bar only: "|", "||", "|]", "|:", ":|", "::"
  std::string ending;    // Bar only: volta number that starts after this bar ("1", "2")
  // Note only, first verse of the lyrics (ABC w:, MusicXML <lyric>): the syllable sung on this note, whether the word
  // goes on (a hyphen before the next syllable), or, with no syllable, whether the previous syllable is held over it.
  std::string lyric;
  bool lyricHyphen = false;
  bool lyricHeld = false;
};

struct Key {
  int fifths = 2;          // -7..7, number of sharps (positive) or flats (negative)
  std::string mode = "maj";  // maj min dor mix lyd phr loc
};

struct Song {
  std::string id;
  std::string title;
  std::string source;      // where it came from (thesession URL, photo, ...)
  std::string notes;       // free text from the agent: overall advice for the tune
  std::string folder;      // song list group: one of FOLDERS or any name; empty = unsorted
  std::string meter = "4/4";
  Key key;
  int tempo = 0;           // quarter notes per minute, 0 = unknown
  std::string instrument = "whistle";  // an instrument id (instrumentIds())
  std::string whistle = "D";           // whistle key (used when instrument is "whistle")
  int transposed = 0;                  // semitones transpose() moved it since import; 0 = the original sheet
  std::string harp = "C";              // diatonic harmonica key (used when instrument is "harmonica"), one of HARP_KEYS
  std::string instrumentName;          // a custom instrument's name ("instrument_name"), so a deleted one is still named
  std::vector<Event> events;
  std::string words;       // text printed after the tune (ABC W: lines), one line per W:
  // translations of the title, notes and comments: language -> English text -> translation ("pt-BR" -> ...)
  std::map<std::string, std::map<std::string, std::string>> i18n;
};

// ---- storage ----
std::vector<std::string> listSongs();       // ids
std::optional<Song> loadSong(const std::string& id);
void saveSong(const Song& s);
std::string slug(const std::string& title);
inline const std::vector<std::string> FOLDERS = {"Lessons", "Beginner", "Intermediate", "Advanced"};
bool folderBefore(const std::string& a, const std::string& b);  // FOLDERS order, then custom A-Z, unsorted last
json toJson(const Song& s);
Song fromJson(const json& j);
std::string listing(const Song& s);          // compact text table of every event, for the agent
std::string uniqueId(const std::string& title);  // slug(title), with -2, -3... if a song already has it

}  // namespace john
