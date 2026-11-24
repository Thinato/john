// Songs: the JSON form, storage in the data folder, and the agent-facing listing.
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace john {

std::string slug(const std::string& title) {
  std::string r;
  for (unsigned char c : title) {
    if (isalnum(c)) r += (char)tolower(c);
    else if (!r.empty() && r.back() != '-') r += '-';
  }
  while (!r.empty() && r.back() == '-') r.pop_back();
  return r.empty() ? "song" : r;
}

json toJson(const Song& s) {
  json ev = json::array();
  for (auto& e : s.events) {
    json x;
    if (e.kind == Kind::Bar) {
      x = {{"type", "bar"}, {"bar", e.bar}};
      if (!e.ending.empty()) x["ending"] = e.ending;
    } else {
      x = {{"type", e.kind == Kind::Note ? "note" : "rest"}, {"len", e.len}};
      if (e.kind == Kind::Note) x["midi"] = e.midi;
      if (e.tie) x["tie"] = true;
      if (e.tuplet) { x["tuplet"] = e.tuplet; x["tuplet_q"] = e.tupletQ; }
      if (e.tupletStart) x["tuplet_start"] = true;
      if (!e.techniques.empty()) x["techniques"] = e.techniques;
      if (!e.comment.empty()) x["comment"] = e.comment;
      if (!e.lyric.empty()) x["lyric"] = e.lyric;
      if (e.lyricHyphen) x["lyric_hyphen"] = true;
      if (e.lyricHeld) x["lyric_held"] = true;
    }
    ev.push_back(x);
  }
  json j = {{"id", s.id}, {"title", s.title}, {"source", s.source}, {"notes", s.notes},
          {"meter", s.meter}, {"key", {{"fifths", s.key.fifths}, {"mode", s.key.mode}}},
          {"tempo", s.tempo}, {"instrument", s.instrument}, {"whistle", s.whistle}, {"events", ev}};
  if (!s.folder.empty()) j["folder"] = s.folder;
  if (s.harp != "C") j["harp"] = s.harp;  // C harp songs stay as before
  std::string name = s.instrumentName;     // a custom instrument's name, kept for when it is deleted (#85)
  for (auto& i : instruments()) if (i.id == s.instrument) name = i.builtin ? "" : i.name;
  if (!name.empty()) j["instrument_name"] = name;
  if (s.transposed) j["transposed"] = s.transposed;
  if (!s.i18n.empty()) j["i18n"] = s.i18n;
  if (!s.words.empty()) j["words"] = s.words;
  return j;
}

bool folderBefore(const std::string& a, const std::string& b) {
  auto rank = [](const std::string& f) {
    auto it = std::find(FOLDERS.begin(), FOLDERS.end(), f);
    return f.empty() ? 1000 : it != FOLDERS.end() ? (int)(it - FOLDERS.begin()) : 100;
  };
  return rank(a) != rank(b) ? rank(a) < rank(b) : a < b;
}

Song fromJson(const json& j) {
  Song s;
  s.id = j.value("id", "");
  s.title = j.value("title", "");
  s.source = j.value("source", "");
  s.notes = j.value("notes", "");
  s.folder = j.value("folder", "");
  s.meter = j.value("meter", "4/4");
  if (j.contains("key")) s.key = {std::clamp(j["key"].value("fifths", 2), -7, 7), j["key"].value("mode", "maj")};
  s.tempo = j.value("tempo", 0);
  s.instrument = j.value("instrument", "whistle");
  s.whistle = j.value("whistle", "D");
  s.harp = j.value("harp", "C");
  s.instrumentName = j.value("instrument_name", "");
  s.transposed = j.value("transposed", 0);
  for (auto& x : j.value("events", json::array())) {
    Event e;
    std::string t = x.value("type", "note");
    e.kind = t == "bar" ? Kind::Bar : t == "rest" ? Kind::Rest : Kind::Note;
    e.midi = x.value("midi", 0);
    e.len = x.value("len", 0);
    e.tie = x.value("tie", false);
    e.tuplet = x.value("tuplet", 0);
    e.tupletQ = x.value("tuplet_q", 0);
    e.tupletStart = x.value("tuplet_start", false);
    e.techniques = x.value("techniques", std::vector<std::string>{});
    e.comment = x.value("comment", "");
    e.bar = x.value("bar", "");
    e.ending = x.value("ending", "");
    e.lyric = x.value("lyric", "");
    e.lyricHyphen = x.value("lyric_hyphen", false);
    e.lyricHeld = x.value("lyric_held", false);
    s.events.push_back(e);
  }
  s.words = j.value("words", "");
  s.i18n = j.value("i18n", decltype(s.i18n){});
  return s;
}

std::vector<std::string> listSongs() {
  std::vector<std::string> r;
  for (auto& f : fs::directory_iterator(dataDir() / "songs"))
    if (f.path().extension() == ".json") r.push_back(f.path().stem().string());
  std::sort(r.begin(), r.end());
  return r;
}

std::optional<Song> loadSong(const std::string& id) {
  if (id.empty() || id.find('/') != std::string::npos || id.find("..") != std::string::npos) return std::nullopt;
  json j = readJson(dataDir() / "songs" / (id + ".json"));
  if (!j.is_object()) return std::nullopt;
  try {
    Song s = fromJson(j);
    s.id = id;
    return s;
  } catch (const json::exception&) {
    return std::nullopt;  // malformed file: hide it rather than break every listing
  }
}

void saveSong(const Song& s) { writeJson(dataDir() / "songs" / (s.id + ".json"), toJson(s)); }

std::string uniqueId(const std::string& title) {
  std::string base = slug(title), id = base;
  for (int n = 2; loadSong(id); n++) id = base + "-" + std::to_string(n);
  return id;
}

// ---------------- listing ----------------

static std::string beatLabel(const Song& s, int tickInBar) {
  int beat = compoundMeter(s.meter) ? WHOLE * 3 / 8 : WHOLE / 4;
  double b = 1.0 + (double)tickInBar / beat;
  char buf[16];
  snprintf(buf, sizeof buf, "%g", std::round(b * 100) / 100);
  return buf;
}

std::string listing(const Song& s) {
  std::ostringstream o;
  const Instrument in = instrumentOf(s);
  const bool whistle = in.id == "whistle";
  o << "id: " << s.id << "\ntitle: " << s.title << "\nkey: " << keyName(s.key) << "  meter: " << s.meter
    << "  instrument: " << instrumentLabel(s) << "  tempo: " << (s.tempo ? std::to_string(s.tempo) : "?") << "\n";
  if (!s.source.empty()) o << "source: " << s.source << "\n";
  if (!s.notes.empty()) o << "notes: " << s.notes << "\n";
  if (!s.words.empty()) o << "words (W:): " << s.words << "\n";
  o << "\n#idx  bar beat  note  length                   " << (whistle ? "fingering    " : "range        ") << " techniques  // comment\n";
  auto pos = positions(s);
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    char buf[160];
    if (e.kind == Kind::Bar) {
      snprintf(buf, sizeof buf, "#%-4zu %s%s\n", i, e.bar.c_str(), e.ending.empty() ? "" : ("  ending " + e.ending).c_str());
      o << buf;
      continue;
    }
    std::string name = e.kind == Kind::Note ? noteName(spell(s.key, e.midi)) : "rest";
    std::string fing = e.kind != Kind::Note ? "" : whistle ? fingeringText(finger(in.key, e.midi)) : in.plays(e.midi) ? "" : "out-of-range";
    std::string tech;
    for (auto& t : e.techniques) tech += (tech.empty() ? "" : ",") + t;
    snprintf(buf, sizeof buf, "#%-4zu %-3d %-5s %-5s %-24s %-13s %s", i, pos[i].bar, beatLabel(s, pos[i].tick).c_str(), name.c_str(),
             (durationName(e) + (e.tie ? " tie" : "")).c_str(), fing.c_str(), tech.c_str());
    std::string row = buf;
    if (!e.lyric.empty()) row += "  lyric \"" + e.lyric + (e.lyricHyphen ? "-" : "") + "\"";
    else if (e.lyricHeld) row += "  lyric _";
    while (!row.empty() && row.back() == ' ') row.pop_back();
    o << row;
    if (!e.comment.empty()) o << "  // " << e.comment;
    o << "\n";
  }
  return o.str();
}

}  // namespace john
