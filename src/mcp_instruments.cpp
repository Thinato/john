// MCP tools for instruments: list, define, delete custom instruments (#85).
#include "mcp.hpp"
#include <set>

static std::string sharpName(int midi) { return noteName(spell(Key{0, "maj"}, midi)); }  // "C#4"

static std::string describe(const Instrument& in) {
  std::string r = in.id + " - " + in.name + (in.custom ? (in.builtin ? " (custom, overrides the built-in)" : " (custom)") : " (built-in)") +
                  ": written " + sharpName(in.low) + ".." + sharpName(in.high);
  if (in.octave) r += in.octave > 0 ? ", sounds " + std::to_string(in.octave * 12) + " semitones higher" : ", sounds " + std::to_string(-in.octave * 12) + " semitones lower";
  if (!in.key.empty()) r += ", key " + in.key;
  if (in.id == "whistle" && !in.keys.empty()) r += " (other keys: the whistle argument)";
  if (in.id == "harmonica" && !in.keys.empty()) r += " (other keys: the harp argument)";
  std::string hard, impossible;
  for (auto& [at, n] : in.notes) {
    char w[16];
    snprintf(w, sizeof w, "%g", n.weight);
    if (n.level == NoteLevel::Hard) hard += " " + sharpName(in.low + at) + (n.how.empty() ? "" : " " + n.how) + (n.weight != 1 ? " x" + std::string(w) : "") + ",";
    if (n.level == NoteLevel::Impossible) impossible += " " + sharpName(in.low + at) + ",";
  }
  if (!hard.empty()) r += "\n  hard:" + hard.substr(0, hard.size() - 1);
  if (!impossible.empty()) r += "\n  impossible:" + impossible.substr(0, impossible.size() - 1);
  std::string tech;
  for (auto& t : in.techniques) tech += (tech.empty() ? "" : ", ") + t;
  std::string instead;
  for (auto& [from, to] : in.instead) instead += (instead.empty() ? "" : ", ") + from + " -> " + to;
  if (!instead.empty()) tech += "; instead: " + instead;
  r += "\n  techniques: " + (tech.empty() ? "none listed" : tech) + "; sound: " + in.sound + (in.hints ? "; fingering charts in the GUI" : "") +
       (in.voice ? "; singing voice (the mic counts a note an octave off as right)" : "") + "\n";
  return r;
}

static std::string list_instruments(const json& a) {
  std::string r;
  for (auto& in : instruments()) r += describe(in);
  for (auto& w : instrumentWarnings()) r += "warning: " + w + "\n";
  return r;
}

static std::string define_instrument(const json& a) {
  static const std::set<std::string> FIELDS = {"id", "name", "low", "high", "octave", "key", "techniques", "instead", "sound", "voice"};
  json j = json::object();  // only what the tool offers: hints, order, keys stay built-in properties
  for (auto& [k, v] : a.items()) {
    if (FIELDS.count(k)) j[k] = v;
    else if (k != "preset" && k != "notes")
      throw std::runtime_error("unknown field '" + k + "' (define_instrument takes id, name, low, high, octave, key, preset, notes, techniques, instead, sound, voice)");
  }
  if (!j.contains("id") && a.value("name", "") != "") j["id"] = slug(a.value("name", ""));
  std::string preset = a.value("preset", "chromatic");
  if (preset != "chromatic" && preset != "diatonic") throw std::runtime_error("preset must be \"chromatic\" or \"diatonic\"");
  json notes = a.value("notes", json::object());
  Instrument in = parseInstrument(j);  // validates every field
  applyPreset(in, preset == "diatonic");
  j["notes"] = notes;
  for (auto& [at, n] : parseInstrument(j).notes) in.notes[at] = n;  // the explicit levels win over the preset
  auto before = findInstrument(in.id);
  saveInstrument(in);
  std::string what = !before ? "created" : before->custom ? "replaced" : "saved (it overrides the built-in " + before->name + ")";
  return "Instrument '" + in.id + "' " + what + ". Use instrument = '" + in.id + "' in import_abc, update_song, list_songs and analyze_song.\n" +
         describe(*findInstrument(in.id));
}

static std::string delete_instrument(const json& a) {
  std::string id = a.value("id", "");
  auto in = findInstrument(id);
  if (!in || !in->custom) throw std::runtime_error("'" + id + "' is not a custom instrument (built-ins can't be deleted; see list_instruments)");
  int songs = 0;
  for (auto& s : listSongs())
    if (auto x = loadSong(s)) songs += x->instrument == id;
  deleteInstrument(id);
  if (auto back = findInstrument(id)) return "Deleted the custom '" + id + "'. The built-in " + back->name + " is back.";
  return "Deleted '" + id + "'. " + std::to_string(songs) + " song(s) are written for it" +
         (songs ? ": they keep their notes and show it as deleted." : ".");
}

std::vector<Tool> instrumentTools() {
  return {
      tool("list_instruments",
           "List the instruments John knows (built-in and the user's custom ones): id, name, written range, sounding octave, key, "
           "hard and impossible notes, techniques, playback sound. Use the id as instrument in the other tools.",
           json::object(), list_instruments),
      tool("define_instrument",
           "Create or replace a custom instrument (e.g. 'I play a C flute, low D to high A'), saved in John's data folder and "
           "shown in the GUI. Songs and playability can then use its id. A custom id equal to a built-in's overrides that built-in.",
           {{"id", {{"type", "string"}, {"description", "lowercase letters, digits, - or _; default: from the name"}}},
            {"name", str},
            {"low", {{"type", {"string", "integer"}}, {"description", "lowest written note: a note like \"D4\" (C4 = middle C) or a MIDI number"}}},
            {"high", {{"type", {"string", "integer"}}, {"description", "highest written note, same format"}}},
            {"octave", {{"type", "integer"}, {"minimum", -2}, {"maximum", 2},
                        {"description", "sounding = written + 12 * octave (whistle and flute music: often 0 or 1; guitar -1). Default 0"}}},
            {"key", {{"type", "string"}, {"description", "the instrument's key, a note name like C or Bb (keyed instruments); default none"}}},
            {"preset", {{"type", "string"}, {"enum", {"chromatic", "diatonic"}},
                        {"description", "chromatic: every note in range is easy; diatonic: notes outside the major scale of key (or "
                                        "of the lowest note) are hard. Default chromatic"}}},
            {"notes", {{"type", "object"},
                       {"description", "per-note levels over the preset, keyed by semitones above low: \"easy\", \"hard\", "
                                       "\"impossible\", or {\"level\": \"hard\", \"how\": \"half-hole\"|\"cross-fingering\"|\"bend\"|"
                                       "\"overblow\", \"weight\": 1 (= a whistle half-hole)}"}}},
            {"techniques", {{"type", "array"}, {"items", str}, {"description", "technique ids it can play (list_techniques); the sheet hides the others"}}},
            {"instead", {{"type", "object"}, {"additionalProperties", str},
                         {"description", "techniques it can't play -> one of its techniques shown and played in their place, "
                                         "e.g. {\"cut\": \"grace\", \"roll\": \"turn\"}"}}},
            {"sound", {{"type", "string"}, {"enum", SOUNDS}, {"description", "playback voice, default whistle"}}},
            {"voice", {{"type", "boolean"}, {"description", "a singing voice: the mic counts a note sung an octave off as right, breaths are played. Default false"}}}},
           {"name", "low", "high"}, define_instrument),
      tool("delete_instrument",
           "Delete a custom instrument. Songs written for it keep their notes and show it as deleted; a deleted custom file "
           "that overrode a built-in brings the built-in back.",
           {{"id", str}}, {"id"}, delete_instrument),
  };
}
