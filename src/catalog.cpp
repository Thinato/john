// The catalog: techniques (built-in and custom) and instrument profiles, and the marks an instrument shows.
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>
#include <stdexcept>

namespace john {

// ---------------- catalog ----------------

std::vector<Technique> defaultTechniques() {
  std::vector<Technique> r = {
      {"cut", "Cut", "^", "Grace note ABOVE the melody note: flick a finger up off a hole just above the sounding note and put it straight back. Separates two notes of the same pitch without tonguing, and accents a note. Usually the G finger (cutting notes below G) or the A/B finger for upper notes."},
      {"tap", "Tap (strike)", "v", "Grace note BELOW: slap a finger down on the hole just below the sounding note and lift it immediately. Accents a note; impossible on bottom D."},
      {"roll", "Long roll", "~", "Note, cut, note, tap, note, all in the time of a dotted quarter (3 eighths). The signature ornament on long notes in jigs and reels. Not possible on bottom D (use a cran)."},
      {"short_roll", "Short roll", "s~", "Starts directly with the cut (cut, note, tap, note), in the time of a quarter note. Fits where a long roll does not."},
      {"cran", "Cran", "cr", "Series of quick cuts with no taps, on notes where a roll is impossible: bottom D and E. Borrowed from the uilleann pipes."},
      {"slide", "Slide", "/", "Glide up into the note by slowly uncovering the hole(s) from the note below. Expressive; common on F#, G, A and B in slow airs and polkas."},
      {"tongue", "Tongued", "T", "Start the note with the tongue ('tu' or 'du'). Irish style slurs most notes; tongue only phrase starts and accented notes."},
      {"breath", "Breath", ",", "Take a breath AFTER this note. Usually done by leaving out a weak note or shortening a long one."},
      {"staccato", "Staccato", ".", "Short, detached note."},
      {"accent", "Accent", ">", "Emphasise the note with breath or tonguing."},
      {"vibrato", "Vibrato", "vib", "Pulse the breath (diaphragm) or shake a finger over an open hole below the note. Mostly for slow airs."},
      {"trill", "Trill", "tr", "Rapid alternation with the note above."},
      {"double_cut", "Double cut", "^^", "Two quick cuts in a row before the note, giving a stutter. A variant of rolls and crans."},
      // equivalents for instruments that can't play the finger ornaments above (Instrument::instead, #86)
      {"grace", "Grace note", "gr", "A very short note a step above, played just before the main note and taken from its time. How a keyboard or a harmonica plays a cut or a tap."},
      {"turn", "Turn", "tn", "Four quick notes around the main note: the note above, the note, the note below, the note. Takes the place of a roll on a keyboard."},
      {"shake", "Shake", "sh", "Harmonica: rock the harp quickly between this hole and the next one up, so the two notes alternate. Takes the place of a roll."},
  };
  for (auto& t : r) { t.name = tr(t.name); t.description = tr(t.description); }  // English unless the GUI set a language
  return r;
}

// The ids techniques.json defines, as of the last loadTechniques() (marks() asks per note, so no file read there).
// Guarded: Import audio's worker thread imports ABC, which loads the catalog.
static std::mutex gTechMu;
static std::set<std::string> gCustomTech;
static bool gTechLoaded = false;

bool builtinTechnique(const std::string& id) {
  static const std::set<std::string> ids = [] {  // ids don't depend on the language
    std::set<std::string> r;
    for (auto& t : defaultTechniques()) r.insert(t.id);
    return r;
  }();
  if (!ids.count(id)) return false;
  bool loaded;
  { std::lock_guard lock(gTechMu); loaded = gTechLoaded; }
  if (!loaded) loadTechniques();
  std::lock_guard lock(gTechMu);
  return !gCustomTech.count(id);  // a user's own technique with this id (e.g. a grace defined before #86) is custom
}

std::vector<Technique> loadTechniques() {
  auto all = defaultTechniques();
  std::set<std::string> custom;
  json j = readJson(dataDir() / "techniques.json");
  if (j.is_array())
    for (auto& t : j) {
      Technique x;
      try { x = {t.value("id", ""), t.value("name", ""), t.value("symbol", ""), t.value("description", "")}; }
      catch (const json::exception&) { continue; }  // skip malformed entries
      if (x.id.empty()) continue;
      custom.insert(x.id);
      auto it = std::find_if(all.begin(), all.end(), [&](auto& a) { return a.id == x.id; });
      if (it != all.end()) *it = x; else all.push_back(x);
    }
  std::lock_guard lock(gTechMu);
  gCustomTech = custom;
  gTechLoaded = true;
  return all;
}

void saveCustomTechnique(const Technique& t) {
  fs::path p = dataDir() / "techniques.json";
  json j = readJson(p);
  if (!j.is_array()) j = json::array();
  json x = {{"id", t.id}, {"name", t.name}, {"symbol", t.symbol}, {"description", t.description}};
  bool replaced = false;
  for (auto& e : j) if (e.value("id", "") == t.id) { e = x; replaced = true; }
  if (!replaced) j.push_back(x);
  writeJson(p, j);
  std::lock_guard lock(gTechMu);
  gCustomTech.insert(t.id);
}

// ---------------- instruments (#85) ----------------

fs::path instrumentsDir() {
#ifdef JOHN_INSTRUMENTS
  return bundled("instruments", JOHN_INSTRUMENTS);
#else
  return {};
#endif
}

static bool validId(const std::string& id) {  // also a file name, so none of Windows' reserved device names
  static const std::set<std::string> RESERVED = {"con", "prn", "aux", "nul"};
  bool device = RESERVED.count(id) || (id.size() == 4 && (!id.compare(0, 3, "com") || !id.compare(0, 3, "lpt")) && id[3] >= '1' && id[3] <= '9');
  return !id.empty() && id.size() <= 60 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") == std::string::npos && !device;
}

static int jsonNote(const json& j, const char* field) {
  if (!j.contains(field)) throw std::runtime_error(std::string(field) + " is missing (a note like \"D4\" or a MIDI number, C4 = 60)");
  const json& v = j[field];
  int m = v.is_number_integer() ? v.get<int>() : v.is_string() ? parseNote(v.get<std::string>()) : -1;
  if (m < 0 || m > 127) throw std::runtime_error(std::string(field) + " must be a note like \"D4\" or \"Bb3\", or a MIDI number 0..127 (C4 = 60)");
  return m;
}

Instrument parseInstrument(const json& j) {
  using E = std::runtime_error;
  if (!j.is_object()) throw E("an instrument must be a JSON object");
  auto str = [&](const char* f, const std::string& def) {
    if (!j.contains(f)) return def;
    if (!j[f].is_string()) throw E(std::string(f) + " must be a string");
    return j[f].get<std::string>();
  };
  auto strings = [&](const char* f) {
    std::vector<std::string> r;
    if (!j.contains(f)) return r;
    if (!j[f].is_array()) throw E(std::string(f) + " must be a list of strings");
    for (auto& x : j[f]) {
      if (!x.is_string()) throw E(std::string(f) + " must be a list of strings");
      r.push_back(x.get<std::string>());
    }
    return r;
  };
  Instrument in;
  in.id = str("id", "");
  if (!validId(in.id)) throw E("id must be 1-60 characters: lowercase letters, digits, - or _ (not con, prn, aux, nul, com1-9, lpt1-9)");
  in.name = str("name", "");
  if (in.name.empty()) throw E("name is missing");
  in.low = jsonNote(j, "low");
  in.high = jsonNote(j, "high");
  if (in.low > in.high) throw E("low must not be above high");
  if (j.contains("octave")) {
    if (!j["octave"].is_number_integer() || j["octave"].get<int>() < -2 || j["octave"].get<int>() > 2)
      throw E("octave must be -2..2 (sounding = written + 12 * octave: whistle 1, guitar -1)");
    in.octave = j["octave"].get<int>();
  }
  in.key = str("key", "");
  if (!in.key.empty() && whistleBase(in.key) < 0) throw E("key must be a note name like D, Bb or F#");
  in.keys = strings("keys");
  for (auto& k : in.keys) if (whistleBase(k) < 0) throw E("keys: '" + k + "' is not a note name like D, Bb or F#");
  if (!in.keys.empty() && std::find(in.keys.begin(), in.keys.end(), in.key) == in.keys.end()) throw E("keys must include key");
  in.keyedName = str("keyed_name", "");
  if (!in.keys.empty() && in.keyedName.find("%s") == std::string::npos) throw E("keyed_name must contain %s (the key)");
  if (j.contains("notes")) {
    if (!j["notes"].is_object()) throw E("notes must be an object: semitones above low -> easy, hard or impossible");
    for (auto& [k, v] : j["notes"].items()) {
      const std::string at = "notes \"" + k + "\": ";
      if (k.empty() || k.size() > 3 || k.find_first_not_of("0123456789") != std::string::npos || std::stoi(k) > in.high - in.low)
        throw E(at + "must be a number of semitones above low, 0.." + std::to_string(in.high - in.low));
      if (!v.is_string() && !v.is_object()) throw E(at + "must be \"easy\", \"hard\", \"impossible\" or {\"level\", \"how\", \"weight\"}");
      std::string lv = v.is_string() ? v.get<std::string>() : v.value("level", "");
      NoteLevel n;
      if (lv == "hard") n.level = NoteLevel::Hard;
      else if (lv == "impossible") n.level = NoteLevel::Impossible;
      else if (lv != "easy") throw E(at + "level must be easy, hard or impossible");
      if (v.is_object()) {
        if (v.contains("how")) {
          if (!v["how"].is_string()) throw E(at + "how must be a string");
          n.how = v["how"].get<std::string>();
          if (std::find(NOTE_HOWS.begin(), NOTE_HOWS.end(), n.how) == NOTE_HOWS.end())
            throw E(at + "how must be half-hole, cross-fingering, bend or overblow");
        }
        if (v.contains("weight")) {
          if (!v["weight"].is_number() || v["weight"].get<double>() <= 0 || v["weight"].get<double>() > 10)
            throw E(at + "weight must be a number above 0, at most 10 (1 = a whistle half-hole)");
          n.weight = v["weight"].get<double>();
        }
      }
      in.notes[std::stoi(k)] = n;
    }
  }
  in.techniques = strings("techniques");
  if (j.contains("instead")) {
    if (!j["instead"].is_object()) throw E("instead must be an object: technique id -> the technique id played instead");
    for (auto& [k, v] : j["instead"].items()) {
      if (!v.is_string()) throw E("instead \"" + k + "\" must be a technique id");
      std::string to = v.get<std::string>();
      if (std::find(in.techniques.begin(), in.techniques.end(), k) != in.techniques.end())
        throw E("instead \"" + k + "\": the instrument lists " + k + " itself, so it is never replaced");
      if (std::find(in.techniques.begin(), in.techniques.end(), to) == in.techniques.end())
        throw E("instead \"" + k + "\": \"" + to + "\" must be one of the instrument's techniques");
      in.instead[k] = to;
    }
  }
  in.sound = str("sound", "whistle");
  if (std::find(SOUNDS.begin(), SOUNDS.end(), in.sound) == SOUNDS.end())
    throw E("sound must be one of whistle, violin, piano, guitar, electric-guitar, harmonica");
  if (j.contains("hints") && !j["hints"].is_boolean()) throw E("hints must be true or false");
  in.hints = j.value("hints", false);
  if (j.contains("voice") && !j["voice"].is_boolean()) throw E("voice must be true or false");
  in.voice = j.value("voice", false);
  if (j.contains("order") && !j["order"].is_number_integer()) throw E("order must be a whole number");
  in.order = j.value("order", 1000);
  return in;
}

json toJson(const Instrument& in) {
  json notes = json::object();
  for (auto& [at, n] : in.notes) {
    if (n.level == NoteLevel::Easy) continue;  // missing = easy
    json x = {{"level", n.level == NoteLevel::Hard ? "hard" : "impossible"}};
    if (!n.how.empty()) x["how"] = n.how;
    if (n.level == NoteLevel::Hard && n.weight != 1) x["weight"] = n.weight;
    notes[std::to_string(at)] = x;
  }
  json j = {{"name", in.name}, {"low", in.low}, {"high", in.high}, {"octave", in.octave}, {"notes", notes},
            {"techniques", in.techniques}, {"sound", in.sound}};
  if (!in.key.empty()) j["key"] = in.key;
  if (!in.keys.empty()) { j["keys"] = in.keys; j["keyed_name"] = in.keyedName; }
  if (!in.instead.empty()) j["instead"] = in.instead;
  if (in.hints) j["hints"] = true;
  if (in.voice) j["voice"] = true;
  return j;
}

// The catalog, read from the files once and again on reloadInstruments(). Guarded: Import audio's worker thread
// calls findInstrument() while the GUI may reload.
static std::mutex gInstMu;
static std::vector<Instrument> gInst;
static std::vector<std::string> gInstWarn;
static bool gInstLoaded = false;

static void loadInstrumentsLocked() {
  gInst.clear();
  gInstWarn.clear();
  auto read = [](const fs::path& dir, bool custom) {
    std::error_code ec;
    std::vector<fs::path> files;
    for (auto& f : fs::directory_iterator(dir, ec)) if (f.path().extension() == ".json") files.push_back(f.path());
    std::sort(files.begin(), files.end());
    for (auto& f : files) {
      try {
        json j = readJson(f);
        if (j.is_discarded()) throw std::runtime_error("not valid JSON");
        if (j.is_object()) j["id"] = f.stem().string();  // the file name is the id
        Instrument in = parseInstrument(j);
        in.custom = custom;
        auto it = std::find_if(gInst.begin(), gInst.end(), [&](auto& b) { return b.id == in.id; });
        in.builtin = !custom || it != gInst.end();
        if (it == gInst.end()) gInst.push_back(in);
        else { in.order = it->order; *it = in; }  // a custom file overrides the built-in with its id
      } catch (const std::exception& e) {
        gInstWarn.push_back(f.string() + " skipped: " + e.what());
      }
    }
  };
  read(instrumentsDir(), false);
  read(dataDir() / "instruments", true);
  if (std::none_of(gInst.begin(), gInst.end(), [](auto& i) { return i.id == "whistle"; })) {
    // ponytail: instrumentOf() falls back to the whistle, so there always is one, even without instruments/
    Instrument w;
    w.id = "whistle", w.name = "D whistle", w.low = 62, w.high = 85, w.octave = 1, w.key = "D", w.builtin = true, w.order = 0;
    for (auto& t : defaultTechniques())  // whistle.json's list: every built-in but the other instruments' equivalents
      if (t.id != "grace" && t.id != "turn" && t.id != "shake") w.techniques.push_back(t.id);
    gInst.push_back(w);
    gInstWarn.push_back("no whistle profile in " + instrumentsDir().string() + ": using a plain D whistle");
  }
  std::stable_sort(gInst.begin(), gInst.end(), [](auto& a, auto& b) { return std::tie(a.order, a.name) < std::tie(b.order, b.name); });
  gInstLoaded = true;
}

void reloadInstruments() {
  std::lock_guard lock(gInstMu);
  loadInstrumentsLocked();
}

std::vector<Instrument> instruments() {
  std::lock_guard lock(gInstMu);
  if (!gInstLoaded) loadInstrumentsLocked();
  return gInst;
}

std::vector<std::string> instrumentIds() {
  std::vector<std::string> r;
  for (auto& i : instruments()) r.push_back(i.id);
  return r;
}

std::vector<std::string> instrumentWarnings() {
  std::lock_guard lock(gInstMu);
  if (!gInstLoaded) loadInstrumentsLocked();
  return gInstWarn;
}

std::optional<Instrument> findInstrument(const std::string& id, const std::string& whistleKey, const std::string& harpKey) {
  std::optional<Instrument> r;
  {
    std::lock_guard lock(gInstMu);  // copy only the match: called per frame and per note by the GUI
    if (!gInstLoaded) loadInstrumentsLocked();
    for (auto& i : gInst) if (i.id == id) { r = i; break; }
  }
  if (!r || r->keys.empty()) return r;
  Instrument& in = *r;  // a keyed instrument: move the range (and so the note table) to the asked key
  std::string k = id == "whistle" ? whistleKey : id == "harmonica" ? harpKey : in.key;
  auto lower = [](std::string x) { for (auto& c : x) c = (char)std::tolower((unsigned char)c); return x; };
  auto it = std::find_if(in.keys.begin(), in.keys.end(), [&](auto& x) { return lower(x) == lower(k); });  // "bb" is Bb
  if (it == in.keys.end()) return std::nullopt;
  k = *it;
  auto pos = [&](const std::string& x) { return mod12(whistleBase(x) - whistleBase(in.keys[0])); };  // above the lowest key
  int shift = pos(k) - pos(in.key);
  in.low += shift;
  in.high += shift;
  if (k != in.key) {
    in.name = in.keyedName;
    in.name.replace(in.name.find("%s"), 2, k);
    in.key = k;
  }
  return r;
}

Instrument instrumentOf(const Song& s) {
  if (auto i = findInstrument(s.instrument, s.whistle, s.harp)) return *i;
  if (auto i = findInstrument("whistle", "D")) return *i;
  auto all = instruments();
  for (auto& i : all) if (i.id == "whistle") return i;  // a custom whistle that doesn't come in D
  return all.front();                                    // unreachable: the catalog always has a whistle
}

std::string instrumentLabel(const Song& s) {
  if (findInstrument(s.instrument, s.whistle, s.harp) || s.instrumentName.empty()) return instrumentOf(s).name;
  return s.instrumentName + " (deleted)";
}

void saveInstrument(const Instrument& in) {
  json j = toJson(in);
  json check = j;
  check["id"] = in.id;
  parseInstrument(check);  // never write a file the loader would skip: throws with the reason instead
  std::set<std::string> known;
  for (auto& t : loadTechniques()) known.insert(t.id);
  auto need = [&](const std::string& t) {
    if (!known.count(t)) throw std::runtime_error("unknown technique '" + t + "' (see list_techniques or define_technique)");
  };
  for (auto& t : in.techniques) need(t);
  for (auto& [from, to] : in.instead) need(from), need(to);
  fs::create_directories(dataDir() / "instruments");
  writeJson(dataDir() / "instruments" / (in.id + ".json"), j);
  reloadInstruments();
}

Marks marks(const std::vector<std::string>& techniques, const Instrument& in) {
  Marks m;
  auto lists = [&](const std::string& t) { return std::find(in.techniques.begin(), in.techniques.end(), t) != in.techniques.end(); };
  for (auto& t : techniques) {
    auto it = in.instead.find(t);
    std::string show = lists(t) || t == "breath" || !builtinTechnique(t) ? t : it != in.instead.end() ? it->second : "";
    if (show.empty()) m.hidden.push_back(t);
    else if (std::find(m.shown.begin(), m.shown.end(), show) == m.shown.end()) m.shown.push_back(show);
  }
  return m;
}

int hiddenMarks(const Song& s, const Instrument& in) {
  int n = 0;
  for (auto& e : s.events) if (e.kind == Kind::Note) n += (int)marks(e.techniques, in).hidden.size();
  return n;
}

bool deleteInstrument(const std::string& id) {
  std::error_code ec;
  bool done = validId(id) && fs::remove(dataDir() / "instruments" / (id + ".json"), ec);
  persist();
  reloadInstruments();
  // A real delete resets a Settings instrument that is gone now (not a built-in that came back). A file that is only
  // broken or missing doesn't: myInstrument()/playsOn() fall back to the D whistle while reading.
  Settings s = loadSettings();
  if (done && s.instrument == id && !findInstrument(id)) {
    s.instrument = "whistle", s.whistle = "D";
    saveSettings(s);
  }
  return done;
}

void applyPreset(Instrument& in, bool diatonic) {
  in.notes.clear();
  if (!diatonic) return;
  static const bool MAJOR[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
  const int tonic = in.key.empty() ? in.low : whistleBase(in.key);
  for (int m = in.low; m <= in.high; m++)
    if (!MAJOR[mod12(m - tonic)]) in.notes[m - in.low] = {NoteLevel::Hard};
}

}  // namespace john
