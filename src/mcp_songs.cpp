// MCP tools for songs: list, import (ABC, MusicXML), get, analyze, set techniques, update, delete bars.
#include "mcp.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

static void checkInstrument(const Song& s) {
  if (whistleBase(s.whistle) < 0) throw std::runtime_error("unknown whistle key '" + s.whistle + "' (use a note name like D, C, Bb)");
  if (std::find(HARP_KEYS.begin(), HARP_KEYS.end(), s.harp) == HARP_KEYS.end())
    throw std::runtime_error("unknown harmonica key '" + s.harp + "' (use one of G, Ab, A, Bb, B, C, Db, D, Eb, E, F, F#)");
  if (!findInstrument(s.instrument, s.whistle, s.harp)) {
    if (auto in = findInstrument(s.instrument)) {  // it exists, but not in that key
      std::string keys;
      for (auto& k : in->keys) keys += (keys.empty() ? "" : ", ") + k;
      throw std::runtime_error("unknown " + std::string(s.instrument == "harmonica" ? "harmonica key '" + s.harp : "whistle key '" + s.whistle) +
                               "' for the " + in->name + " (use one of " + keys + ")");
    }
    std::string ids;
    for (auto& i : instrumentIds()) ids += (ids.empty() ? "" : ", ") + i;
    throw std::runtime_error("unknown instrument '" + s.instrument + "' (use one of: " + ids + ", or define_instrument)");
  }
}

static Song mustLoad(const json& a) {
  auto s = loadSong(a.value("id", ""));
  if (!s) throw std::runtime_error("no song with id '" + a.value("id", "") + "' (see list_songs)");
  return *s;
}

// The arguments' instrument, whistle and harp, else the user's (Settings). A Settings instrument whose custom file is
// missing or broken reads as the D whistle (#85: Settings are only reset when it is really deleted).
static Song argsOrSettings(const json& a) {
  Settings set = loadSettings();
  if (!findInstrument(set.instrument, set.whistle, set.harp)) set.instrument = "whistle", set.whistle = "D";
  Song like;
  like.instrument = a.value("instrument", set.instrument);
  like.whistle = a.value("whistle", set.whistle);
  like.harp = a.value("harp", set.harp);
  checkInstrument(like);
  return like;
}

// The instrument list_songs / analyze_song judge playability for.
static Instrument playsOn(const json& a) { return instrumentOf(argsOrSettings(a)); }

static std::string fitText(const Playability& p) {
  auto by = [&] { return " (transpose " + std::string(p.shift > 0 ? "+" : "") + std::to_string(p.shift) + ")"; };
  std::string where = p.shift == 0 ? "as written" : p.shift % 12 ? "in " + keyName(p.key) + by() : (p.shift > 0 ? "an octave up" : "an octave down") + by();
  if (p.verdict != Playability::NoFit) return "fits " + where;
  if (p.cost.out == 0) return "doesn't fit: too many hard notes in every key";
  return "doesn't fit: " + std::to_string(p.cost.out) + " of " + std::to_string(p.notes) + " notes out of range at best, " + where;
}

static std::string list_songs(const json& a) {
  const Instrument in = playsOn(a);
  std::map<std::string, std::string, decltype(&folderBefore)> byFolder(&folderBefore);
  for (auto& id : listSongs())
    if (auto s = loadSong(id))
      byFolder[s->folder] += "  " + id + " - " + s->title + " (" + keyName(s->key) + ", " + s->meter + ", " + instrumentLabel(*s) +
                             "): " + fitText(playability(*s, in)) + "\n";
  std::string r;
  for (auto& [f, lines] : byFolder) r += (f.empty() ? "Unsorted" : f) + ":\n" + lines;
  return r.empty() ? "No songs yet. Data folder: " + dataDir().string() : "Playability on the " + in.name + ":\n" + r;
}

static std::string importTool(const json& a, bool xml) {  // import_abc, import_musicxml
  Song like = argsOrSettings(a);
  like.id = a.value("id", "");
  like.source = a.value("source", "");
  like.folder = a.value("folder", "");
  std::string data = a.value("abc", "");
  if (xml) {
    std::string path = a.value("path", "");
    if (path.rfind("~/", 0) == 0) path = homeDir() + path.substr(1);
    data = a.value("xml", "");
    if (!path.empty()) {
      std::ifstream f(fs::path(path), std::ios::binary);
      if (!f) throw std::runtime_error("cannot read " + path);
      data.assign(std::istreambuf_iterator<char>(f), {});
    }
    if (!isMusicXml(data)) throw std::runtime_error("not MusicXML: pass path (a .musicxml, .xml or .mxl file) or xml (text with a <score-partwise> root)");
  }
  std::string out;
  for (auto& ir : importSongs(data, like, a.value("overwrite", false))) {
    out += (out.empty() ? "" : "\n") + std::string("Saved as '") + ir.song.id + "'.\n";
    for (auto& x : ir.warnings) out += "warning: " + x + "\n";
    out += "\n" + listing(ir.song);
  }
  return out;
}
static std::string import_abc(const json& a) { return importTool(a, false); }
static std::string import_musicxml(const json& a) { return importTool(a, true); }

static std::string get_song(const json& a) { return listing(mustLoad(a)); }

static std::string analyze_song(const json& a) {
  const Song s = mustLoad(a);
  const Instrument in = playsOn(a);
  return "Playability on the " + in.name + ": " + fitText(playability(s, in)) + "\n\n" + analyze(s, in);
}

static std::string set_techniques(const json& a) {
  Song s = mustLoad(a);
  std::set<std::string> known;
  for (auto& t : loadTechniques()) known.insert(t.id);
  const json edits = a.value("edits", json::array());
  if (!edits.is_array()) throw std::runtime_error("edits must be an array");
  for (auto& e : edits) {  // validate everything first
    long long i = e.is_object() && e.contains("index") && e["index"].is_number_integer() ? e["index"].get<long long>() : -1;
    if (i < 0 || i >= (long long)s.events.size() || s.events[i].kind != Kind::Note)
      throw std::runtime_error("index " + std::to_string(i) + " is not a note (see get_song)");
    for (const char* k : {"techniques", "add", "remove"})
      for (auto& t : e.value(k, std::vector<std::string>{}))
        if (!known.count(t)) throw std::runtime_error("unknown technique '" + t + "' (see list_techniques or define_technique)");
  }
  std::set<int> touched;
  for (auto& e : edits) {
    Event& ev = s.events[e["index"].get<int>()];
    if (e.contains("techniques")) ev.techniques = e["techniques"].get<std::vector<std::string>>();
    for (auto& t : e.value("add", std::vector<std::string>{}))
      if (std::find(ev.techniques.begin(), ev.techniques.end(), t) == ev.techniques.end()) ev.techniques.push_back(t);
    for (auto& t : e.value("remove", std::vector<std::string>{}))
      ev.techniques.erase(std::remove(ev.techniques.begin(), ev.techniques.end(), t), ev.techniques.end());
    if (e.contains("comment")) ev.comment = e["comment"].get<std::string>();
    touched.insert(e["index"].get<int>());
  }
  saveSong(s);
  std::string out = "Updated " + std::to_string(touched.size()) + " note(s).\n";
  std::istringstream lines(listing(s));
  for (std::string l; std::getline(lines, l);) {
    int idx;
    if (sscanf(l.c_str(), "#%d", &idx) == 1 && touched.count(idx)) out += l + "\n";
  }
  return out;
}

static std::string update_song(const json& a) {
  Song s = mustLoad(a);
  s.title = a.value("title", s.title);
  s.source = a.value("source", s.source);
  s.notes = a.value("notes", s.notes);
  s.folder = a.value("folder", s.folder);
  s.tempo = a.value("tempo", s.tempo);
  if (a.contains("instrument") || a.contains("whistle") || a.contains("harp")) {  // a song with a bad stored value can still be edited
    s.instrument = a.value("instrument", s.instrument);
    s.whistle = a.value("whistle", s.whistle);
    s.harp = a.value("harp", s.harp);
    checkInstrument(s);
  }
  if (int t = a.value("transpose", 0)) {
    if (t < -24 || t > 24) throw std::runtime_error("transpose must be within -24..24 semitones");
    for (auto& e : s.events)
      if (e.kind == Kind::Note && (e.midi + t < 0 || e.midi + t > 127)) throw std::runtime_error("transposition leaves the MIDI range");
    transpose(s, t);
  }
  saveSong(s);
  return "Saved.\n" + listing(s);
}

static std::string delete_bars(const json& a) {
  Song s = mustLoad(a);
  int first = a.value("first", 0);
  int n = deleteBars(s, first, a.value("last", first));
  saveSong(s);
  return "Deleted " + std::to_string(n) + " note(s)/rest(s).\n" + listing(s);
}

std::vector<Tool> songTools() {
  json strArr = {{"type", "array"}, {"items", str}};
  json playsOn = {  // playability (#84): which instrument to judge the songs for
      {"instrument", {{"type", "string"}, {"description", "an instrument id (list_instruments); judge playability for this instrument; default: the one chosen in John's Settings (the user's instrument)"}}},
      {"whistle", {{"type", "string"}, {"description", "whistle key for instrument whistle; default: John's Settings (D unless changed)"}}},
      {"harp", {{"type", "string"}, {"enum", HARP_KEYS}, {"description", "harmonica key for instrument harmonica; default: John's Settings (C unless changed)"}}}};
  json analyzeArgs = playsOn;
  analyzeArgs["id"] = str;
  return {
      tool("list_songs",
           "List saved songs by folder (id, title, key, meter, the instrument it was written for) and whether each fits an instrument "
           "(range and hard notes such as half-holes, in any key of the same mode): 'fits as written', 'fits in <key> (transpose N)', or "
           "'doesn't fit'. Use it for 'what can I play on my A harmonica?'; update_song transpose moves a song.",
           playsOn, list_songs),
      tool("import_abc",
           "Create a song from ABC notation (full tune with X/T/M/L/K headers; each X: tune of a multi-tune file becomes its own song, id applies to the first). Returns warnings and the note listing. "
           "Techniques in the ABC (~, {g}A grace cuts, !id! decorations) and lyrics (w: first verse, W:) are kept.",
           {{"abc", str},
            {"id", {{"type", "string"}, {"description", "optional song id; default derived from the title"}}},
            {"source", {{"type", "string"}, {"description", "where the tune came from (URL, 'photo of ...')"}}},
            {"instrument", {{"type", "string"}, {"description", "an instrument id (list_instruments); instrument the song is for, default: the one chosen in John's Settings (whistle unless changed)"}}},
            {"whistle", {{"type", "string"}, {"description", "whistle key, default: the one chosen in John's Settings (D unless changed)"}}},
            {"harp", {{"type", "string"}, {"enum", HARP_KEYS}, {"description", "harmonica key, default: the one chosen in John's Settings (C unless changed)"}}},
            {"overwrite", {{"type", "boolean"}, {"description", "replace an existing song with the same id"}}},
            {"folder", {{"type", "string"}, {"description", "Beginner, Intermediate, Advanced or Lessons (any name works)"}}}},
           {"abc"}, import_abc),
      tool("import_musicxml",
           "Create a song from a MusicXML file (.musicxml, .xml, or zipped .mxl, as MuseScore, Finale, Sibelius or flat.io export it; "
           "optical music readers such as Audiveris output it too). Takes the first part's first voice; chords keep their top note, grace notes become cuts/taps. "
           "Returns warnings (what was dropped or rounded) and the note listing. Same options as import_abc.",
           {{"path", {{"type", "string"}, {"description", "absolute path (or ~/...) to the .musicxml, .xml or .mxl file"}}},
            {"xml", {{"type", "string"}, {"description", "the MusicXML text itself, instead of a path"}}},
            {"id", {{"type", "string"}, {"description", "optional song id; default derived from the title"}}},
            {"source", {{"type", "string"}, {"description", "where the score came from; default the composer in the file"}}},
            {"instrument", {{"type", "string"}, {"description", "an instrument id (list_instruments); instrument the song is for, default: the one chosen in John's Settings (whistle unless changed)"}}},
            {"whistle", {{"type", "string"}, {"description", "whistle key, default: the one chosen in John's Settings (D unless changed)"}}},
            {"harp", {{"type", "string"}, {"enum", HARP_KEYS}, {"description", "harmonica key, default: the one chosen in John's Settings (C unless changed)"}}},
            {"overwrite", {{"type", "boolean"}, {"description", "replace an existing song with the same id"}}},
            {"folder", {{"type", "string"}, {"description", "Beginner, Intermediate, Advanced or Lessons (any name works)"}}}}, import_musicxml),
      tool("get_song",
           "Show a song as a table: event index, bar, beat, note, length, whistle fingering (X covered, O open, H half, + upper octave; other instruments: out-of-range marks), techniques, lyric syllable (\"syl-\" = the word goes on, _ = holds the previous syllable), comment.",
           {{"id", str}}, {"id"}, get_song),
      tool("analyze_song",
           "Deterministic hints: whether the song fits an instrument (default the user's, from Settings) as written or in which key, "
           "range check on the song's instrument, best transpositions, and candidate notes for cuts, rolls, crans, slides, tonguing and breaths "
           "as that instrument shows them (its equivalents, e.g. a piano's turn for a roll; marks it hides are left out).",
           analyzeArgs, {"id"}, analyze_song),
      tool("set_techniques",
           "Edit techniques on notes, by event index from get_song. Each edit may replace the list ('techniques'), or 'add'/'remove' ids, and set a 'comment' explaining the choice. All edits are validated before any is applied.",
           {{"id", str},
            {"edits",
             {{"type", "array"},
              {"items",
               {{"type", "object"},
                {"properties", {{"index", {{"type", "integer"}}}, {"techniques", strArr}, {"add", strArr}, {"remove", strArr}, {"comment", str}}},
                {"required", {"index"}}}}}}},
           {"id", "edits"}, set_techniques),
      tool("update_song",
           "Change song info: title, source, notes (overall advice shown in the GUI), folder (song list group), tempo (quarter notes per minute), instrument, whistle key, harmonica key (harp), or transpose by semitones.",
           {{"id", str}, {"title", str}, {"source", str}, {"notes", str}, {"folder", str}, {"tempo", {{"type", "integer"}}}, {"instrument", instrumentSchema}, {"whistle", str},
            {"harp", {{"type", "string"}, {"enum", HARP_KEYS}}}, {"transpose", {{"type", "integer"}}}},
           {"id"}, update_song),
      tool("delete_bars",
           "Delete bars first..last (inclusive, bar numbers as get_song shows them; a pickup is bar 0) with their notes, rests and techniques. "
           "Repeat/end signs and volta marks closing the deleted bars move to the previous bar line. Event indexes change afterwards.",
           {{"id", str}, {"first", {{"type", "integer"}}}, {"last", {{"type", "integer"}, {"description", "default: same as first"}}}},
           {"id", "first"}, delete_bars),
  };
}
