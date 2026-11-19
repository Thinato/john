// MusicXML importer: .musicxml/.xml text or a zipped .mxl. The first part's first voice becomes one Song.
#include "john.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <miniz.h>
#include <pugixml.hpp>

namespace john {

static const int NATURAL_PC[7] = {0, 2, 4, 5, 7, 9, 11};
static const char* MXL_ERROR = "Could not read the .mxl file.";

static bool isZip(const std::string& d) { return d.rfind("PK\x03\x04", 0) == 0; }

bool isMusicXml(const std::string& d) {
  return isZip(d) || d.find("<score-partwise") != std::string::npos || d.find("<score-timewise") != std::string::npos;
}

// The score inside an .mxl: META-INF/container.xml names it (the first rootfile); without one, the first .xml/.musicxml.
static std::string unzipMxl(const std::string& zip) {
  mz_zip_archive z{};
  if (!mz_zip_reader_init_mem(&z, zip.data(), zip.size(), 0)) throw std::runtime_error(MXL_ERROR);
  struct End { mz_zip_archive* z; ~End() { mz_zip_reader_end(z); } } end{&z};  // also when a check throws
  // miniz inflates into a buffer of the declared size, so checking it is enough: an entry that lies fails instead of growing
  auto extract = [&](const std::string& name) {
    int i = mz_zip_reader_locate_file(&z, name.c_str(), nullptr, 0);
    mz_zip_archive_file_stat st;
    if (i < 0 || !mz_zip_reader_file_stat(&z, (mz_uint)i, &st)) return std::string();
    if (st.m_uncomp_size > MXL_MAX_ENTRY) throw std::runtime_error("The .mxl file is too large.");
    size_t n = 0;
    void* p = mz_zip_reader_extract_to_heap(&z, (mz_uint)i, &n, 0);
    std::string s = p ? std::string((const char*)p, n) : "";
    mz_free(p);
    return s;
  };
  std::string root, container = extract("META-INF/container.xml");
  pugi::xml_document c;
  if (c.load_buffer(container.data(), container.size()))
    root = c.child("container").child("rootfiles").child("rootfile").attribute("full-path").value();
  for (mz_uint i = 0; root.empty() && i < mz_zip_reader_get_num_files(&z); i++) {
    char buf[1024];
    mz_zip_reader_get_filename(&z, i, buf, sizeof buf);
    std::string n = buf;
    auto ends = [&](const char* x) { return n.size() >= strlen(x) && n.compare(n.size() - strlen(x), std::string::npos, x) == 0; };
    if (n.rfind("META-INF/", 0) != 0 && (ends(".xml") || ends(".musicxml"))) root = n;
  }
  std::string xml = root.empty() ? "" : extract(root);
  if (xml.empty()) throw std::runtime_error(MXL_ERROR);
  return xml;
}

static std::string modeOf(const std::string& m) {
  for (auto [x, mode] : {std::pair{"minor", "min"}, {"aeolian", "min"}, {"dorian", "dor"}, {"mixolydian", "mix"},
                         {"lydian", "lyd"}, {"phrygian", "phr"}, {"locrian", "loc"}})
    if (m == x) return mode;
  return "maj";
}

static bool hasType(pugi::xml_node parent, const char* child, const char* type) {
  for (auto c : parent.children(child))
    if (strcmp(c.attribute("type").value(), type) == 0) return true;
  return false;
}

ImportResult importMusicXml(const std::string& data) {
  ImportResult r;
  Song& s = r.song;
  std::set<std::string> warned;
  auto warn = [&](const std::string& w) { if (warned.insert(w).second) r.warnings.push_back(w); };

  std::string xml = isZip(data) ? unzipMxl(data) : data;
  pugi::xml_document doc;
  if (!doc.load_buffer(xml.data(), xml.size())) throw std::runtime_error("Could not read the MusicXML file.");
  pugi::xml_node score = doc.child("score-partwise");
  if (!score)
    throw std::runtime_error(doc.child("score-timewise") ? "Timewise MusicXML is not supported: export it as partwise." : "Not a MusicXML score.");
  s.title = score.child("work").child_value("work-title");
  if (s.title.empty()) s.title = score.child_value("movement-title");
  if (s.title.empty()) s.title = "Untitled";
  for (auto c : score.child("identification").children("creator"))
    if (strcmp(c.attribute("type").value(), "composer") == 0) s.source = c.child_value();
  int parts = (int)std::distance(score.children("part").begin(), score.children("part").end());
  if (parts > 1) warn("only the first part was imported, " + std::to_string(parts - 1) + " other part(s) dropped");

  s.key = {0, "maj"};
  bool haveKey = false, haveTime = false;
  int divisions = 1;
  std::string voice;          // the voice kept: the first note's
  std::vector<int> graces;    // grace pitches waiting for their main note
  int last = -1;              // index of the last note/rest event (chord notes merge into it)
  auto& ev = s.events;
  auto barBefore = [&]() -> Event& {  // the bar line before the current measure
    if (ev.empty() || ev.back().kind != Kind::Bar) { Event b; b.kind = Kind::Bar; b.bar = "|"; ev.push_back(b); }
    return ev.back();
  };
  auto push = [&](Event e) { ev.push_back(std::move(e)); last = (int)ev.size() - 1; };
  auto ticks = [&](int duration) {  // <duration> counts divisions per quarter note
    long t = (long)duration * (WHOLE / 4);
    if (t % divisions) warn("note lengths that don't fit John's timing grid (1/192 of a whole note) were rounded");
    return std::max(1, (int)std::lround((double)t / divisions));
  };

  for (auto m : score.child("part").children("measure")) {
    std::string right = "|";  // the bar line closing this measure
    int lastBefore = last;
    for (auto x : m.children()) {
      std::string n = x.name();
      if (n == "attributes") {
        if (auto d = x.child("divisions")) divisions = std::max(1, d.text().as_int(1));
        if (auto k = x.child("key")) {
          Key key{std::clamp(k.child("fifths").text().as_int(), -7, 7), modeOf(k.child_value("mode"))};
          if (!haveKey) { s.key = key; haveKey = true; }
          else if (key.fifths != s.key.fifths || key.mode != s.key.mode) warn("key change ignored (one key per song)");
        }
        if (auto t = x.child("time")) {
          std::string sym = t.attribute("symbol").value();
          std::string meter = sym == "common" ? "C" : sym == "cut" ? "C|" : parseMeter(std::string(t.child_value("beats")) + "/" + t.child_value("beat-type"));
          if (meter.empty()) { warn("could not read the time signature, assumed 4/4"); meter = "4/4"; }
          if (!haveTime) { s.meter = meter; haveTime = true; }
          else if (meter != s.meter) warn("time signature change ignored (one meter per song)");
        }
      } else if ((n == "direction" || n == "sound") && !s.tempo) {
        pugi::xml_node snd = n == "sound" ? x : x.child("sound");
        if (double t = snd.attribute("tempo").as_double(); t > 0) s.tempo = (int)std::lround(t);
        for (auto dt : x.children("direction-type"))
          if (auto mt = dt.child("metronome"); mt && !s.tempo) {  // per-minute in beat-units: quarters per minute
            std::string u = mt.child_value("beat-unit");
            double q = u == "whole" ? 4 : u == "half" ? 2 : u == "eighth" ? 0.5 : u == "16th" ? 0.25 : 1;
            if (mt.child("beat-unit-dot")) q *= 1.5;
            s.tempo = (int)std::lround(mt.child("per-minute").text().as_double() * q);
          }
      } else if (n == "barline") {
        std::string style = x.child_value("bar-style");
        auto ending = x.child("ending");
        std::string type = ending.attribute("type").value();
        if (strcmp(x.attribute("location").as_string("right"), "left") == 0) {
          if (strcmp(x.child("repeat").attribute("direction").value(), "forward") == 0) {
            Event& b = barBefore();
            b.bar = b.bar == ":|" ? "::" : "|:";
          }
          if (ending && type == "start") {
            std::string num = ending.attribute("number").value();
            num.erase(std::remove(num.begin(), num.end(), ' '), num.end());  // "1, 2" -> "1,2"
            barBefore().ending = num;
          }
        } else {
          if (strcmp(x.child("repeat").attribute("direction").value(), "backward") == 0) right = ":|";
          else if (style == "light-heavy") right = "|]";
          else if (style == "light-light" || style == "heavy-heavy") right = "||";
          else if (type == "stop" || type == "discontinue") right = "||";  // closes the last ending for playOrder()
        }
      } else if (n == "forward") {
        if (!voice.empty() && voice == x.child_value("voice")) {
          Event e; e.kind = Kind::Rest; e.len = ticks(x.child("duration").text().as_int());
          push(e);
        }
      } else if (n == "note") {
        if (x.child("cue")) continue;
        std::string v = x.child_value("voice");
        if (v.empty()) v = "1";
        if (voice.empty()) voice = v;
        if (v != voice) { warn("only the first voice was imported, other voices dropped"); continue; }
        int midi = -1;
        if (auto p = x.child("pitch")) {
          const char* li = strchr("CDEFGAB", p.child_value("step")[0]);
          double alter = p.child("alter").text().as_double();
          if (alter != std::round(alter)) warn("microtones rounded to the nearest semitone");
          if (li && *li) midi = std::clamp(12 * (p.child("octave").text().as_int(4) + 1) + NATURAL_PC[li - "CDEFGAB"] + (int)std::lround(alter), 0, 127);
        } else if (!x.child("rest")) warn("unpitched notes imported as rests");
        bool tie = hasType(x, "tie", "start");
        for (auto nt : x.children("notations")) tie |= hasType(nt, "tied", "start");
        if (x.child("grace")) { if (midi >= 0) graces.push_back(midi); continue; }
        if (x.child("chord")) {  // keep the top note, the whistle plays one line
          if (last >= 0 && ev[last].kind == Kind::Note && midi > ev[last].midi) { ev[last].midi = midi; ev[last].tie = tie; }
          warn("chords reduced to their top note");
          continue;
        }
        Event e;
        e.kind = midi < 0 ? Kind::Rest : Kind::Note;
        e.midi = std::max(midi, 0);
        int d = x.child("duration").text().as_int();
        e.len = d <= 0 && x.child("rest").attribute("measure").as_bool() ? meterTicks(s.meter) : ticks(d);
        e.tie = tie && midi >= 0;
        if (auto tm = x.child("time-modification"); tm && tm.child("actual-notes").text().as_int() > 0) {
          e.tuplet = tm.child("actual-notes").text().as_int();
          e.tupletQ = tm.child("normal-notes").text().as_int(2);
          bool start = last < 0 || ev[last].tuplet == 0;
          for (auto nt : x.children("notations")) start |= hasType(nt, "tuplet", "start");
          e.tupletStart = start;
        }
        auto add = [&](const std::string& t) {
          if (std::find(e.techniques.begin(), e.techniques.end(), t) == e.techniques.end()) e.techniques.push_back(t);
        };
        if (e.kind == Kind::Note && !graces.empty()) add(graceTechnique(graces, midi));
        graces.clear();
        for (auto nt : x.children("notations")) {
          auto art = nt.child("articulations");
          if (art.child("breath-mark")) add("breath");  // drawn after the note: a breath after it, as John's
          if (e.kind != Kind::Note) continue;
          if (art.child("staccato")) add("staccato");
          if (art.child("accent")) add("accent");
          if (nt.child("ornaments").child("trill-mark")) add("trill");
        }
        push(e);
      }
    }
    if (last != lastBefore) { Event b; b.kind = Kind::Bar; b.bar = right; ev.push_back(b); }
    else if (right != "|" && !ev.empty()) ev.back().bar = right;  // a measure with nothing kept: no bar of its own
  }
  if (last < 0) throw std::runtime_error("No notes found in the first part.");
  return r;
}

std::optional<FirstLineKey> checkFirstLineKey(std::string& xml) {
  pugi::xml_document doc;
  if (!doc.load_buffer(xml.data(), xml.size())) return std::nullopt;
  auto score = doc.child("score-partwise");
  auto measures = [](pugi::xml_node part) { return std::vector<pugi::xml_node>(part.children("measure").begin(), part.children("measure").end()); };
  auto ms = measures(score.child("part"));
  std::vector<std::pair<size_t, int>> keys;  // (measure, fifths) where the key changes
  size_t line2 = 0;                          // the second line's first measure
  for (size_t i = 0; i < ms.size(); i++) {
    for (auto a : ms[i].children("attributes"))
      if (auto k = a.child("key"); k && (keys.empty() || keys.back().second != k.child("fifths").text().as_int()))
        keys.push_back({i, k.child("fifths").text().as_int()});
    for (auto p : ms[i].children("print"))
      if (i && !line2 && (p.attribute("new-system").as_bool() || p.attribute("new-page").as_bool())) line2 = i;
  }
  if (keys.size() != 2 || keys[0].first != 0 || !line2 || keys[1].first != line2) return std::nullopt;
  FirstLineKey r{keys[0].second, keys[1].second, false};
  const Key key{r.rest, "maj"};
  // A written sharp (flat) on line 1 on a note the later key sharpens (flattens) means that line really has no key
  // signature, since with one printed it wouldn't need it: the key changes for real, leave it.
  bool real = false;
  for (size_t i = 0; i < line2; i++)
    for (auto n : ms[i].children("note")) {
      auto p = n.child("pitch");
      const char* li = p ? strchr("CDEFGAB", p.child_value("step")[0]) : nullptr;
      int alter = p.child("alter").text().as_int();
      real |= li && *li && n.child("accidental") && alter && alter == keyAlter(key, int(li - "CDEFGAB"));
    }
  if (r.first != 0 || real) return r;
  for (auto part : score.children("part")) {
    auto pm = measures(part);
    for (size_t i = 0; i < line2 && i < pm.size(); i++) {
      std::set<std::string> written;  // step + octave with a written accidental earlier in the bar: it still holds
      for (auto n : pm[i].children("note")) {
        auto p = n.child("pitch");
        const char* li = p ? strchr("CDEFGAB", p.child_value("step")[0]) : nullptr;
        if (!li || !*li) continue;
        std::string so = std::string(p.child_value("step")) + p.child_value("octave");
        if (n.child("accidental")) written.insert(so);
        if (written.count(so)) continue;
        int alter = keyAlter(key, int(li - "CDEFGAB"));
        auto a = p.child("alter");
        if (!alter) { p.remove_child(a); continue; }
        if (!a) a = p.insert_child_after("alter", p.child("step"));
        a.text().set(alter);
      }
    }
    if (!pm.empty())
      for (auto a : pm[0].children("attributes"))
        if (auto f = a.child("key").child("fifths")) f.text().set(r.rest);
  }
  std::ostringstream os;
  doc.save(os, "", pugi::format_raw);
  xml = os.str();
  r.fixed = true;
  return r;
}

}  // namespace john
