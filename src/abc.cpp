// ABC notation importer (the subset folk tunes use: thesession.org, abcnotation.com, hand-written by the agent).
#include "john.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <sstream>

namespace john {

static const int NATURAL_PC[7] = {0, 2, 4, 5, 7, 9, 11};

static int letterIndex(char c) {
  const char* p = strchr("CDEFGAB", toupper(c));
  return p && *p ? int(p - "CDEFGAB") : -1;
}

static std::string trim(std::string s) {
  while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
  size_t i = 0;
  while (i < s.size() && isspace((unsigned char)s[i])) i++;
  return s.substr(i);
}

// Map an ABC decoration name to a technique id; "" if we don't track it.
static std::string decoration(const std::string& d, const std::set<std::string>& catalog) {
  static const std::map<std::string, std::string> alias = {
      {"~", "roll"}, {"T", "trill"}, {".", "staccato"}, {"L", "accent"}, {">", "accent"}, {"emphasis", "accent"},
      {"trill", "trill"}, {"roll", "roll"}, {"breath", "breath"}, {"slide", "slide"}, {"staccato", "staccato"},
      {"accent", "accent"}, {"strike", "tap"}};
  if (auto it = alias.find(d); it != alias.end()) return it->second;
  return catalog.count(d) ? d : "";
}

ImportResult importAbc(const std::string& abc) {
  ImportResult r;
  Song& s = r.song;
  std::set<std::string> catalog;
  for (auto& t : loadTechniques()) catalog.insert(t.id);
  std::set<std::string> warned;
  auto warn = [&](const std::string& w) { if (warned.insert(w).second) r.warnings.push_back(w); };

  // ---- split header and body ----
  std::istringstream in(abc);
  std::string line, body, unit, tempo;
  bool inBody = false, sawX = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::string t = trim(line);
    if (!inBody && (t.empty() || t[0] == '%')) continue;  // header comments, %abc-2.1, blank lines
    bool field = line.size() >= 2 && line[1] == ':' &&
                 (inBody ? strchr("KLMQTPVWwNR", line[0]) != nullptr : isalpha((unsigned char)line[0]) != 0);
    if (field) {
      char f = line[0];
      std::string v = trim(line.substr(2));
      if (auto pct = v.find('%'); pct != std::string::npos) v = trim(v.substr(0, pct));
      if (f == 'X') {
        if (sawX) { warn("ABC contains several tunes; only the first was imported"); break; }
        sawX = true;
      } else if (f == 'T' && s.title.empty()) s.title = v;
      else if (f == 'C' || f == 'S' || f == 'Z') s.source += (s.source.empty() ? "" : "; ") + v;
      else if (f == 'M' && !inBody) {
        s.meter = v == "C" || v == "C|" ? v : parseMeter(v);  // keep C and C| for the time-signature glyph
        if (s.meter.empty()) { warn("could not read meter '" + v + "', assumed 4/4"); s.meter = "4/4"; }
      }
      else if (f == 'L' && !inBody) unit = v;
      else if (f == 'Q') tempo = v;
      else if (f == 'K' && !inBody) {
        std::string kv, tok;  // drop "clef=..." style options
        for (std::istringstream ks(v); ks >> tok;) if (tok.find('=') == std::string::npos) kv += tok;
        if (auto k = parseKey(kv)) s.key = *k;
        else warn("could not read key '" + v + "', assumed D major");
        inBody = true;
      } else if (inBody && (f == 'K' || f == 'M' || f == 'L'))
        warn(std::string("mid-tune ") + f + ": change ignored (one key/meter per song)");
      continue;
    }
    if (!inBody && !sawX) warn("no K: field, assumed D major");
    inBody = true;
    body += line + "\n";
  }
  if (s.title.empty()) s.title = "Untitled";

  int ln = 1, ld = 8;
  if (!unit.empty()) {
    if (sscanf(unit.c_str(), "%d/%d", &ln, &ld) != 2 || ln <= 0 || ld <= 0) { ln = 1; ld = 8; warn("bad L: field, assumed 1/8"); }
  } else if (meterTicks(s.meter) * 4 < WHOLE * 3) ld = 16;  // ABC rule: meter < 3/4 means L:1/16
  const int unitTicks = WHOLE * ln / ld;

  if (!tempo.empty()) {
    int n = 1, d = 4, bpm = 0;
    if (sscanf(tempo.c_str(), "%d/%d=%d", &n, &d, &bpm) == 3 && d > 0) s.tempo = bpm * n * 4 / d;
    else if (sscanf(tempo.c_str(), "%d", &bpm) == 1) s.tempo = bpm;
  }

  // ---- body ----
  std::map<std::pair<int, int>, int> barAcc;  // (letter, octave) -> explicit accidental in this bar
  std::vector<std::string> decos;
  std::vector<int> graces;
  int tupP = 0, tupQ = 0, tupLeft = 0;
  bool tupStart = false;
  int brokenNum = 1, brokenDen = 1;
  int lastNote = -1;     // index of last note/rest event
  auto& ev = s.events;
  const char* p = body.c_str();

  auto readLength = [&](int& num, int& den) {
    num = 1; den = 1;
    if (isdigit((unsigned char)*p)) num = (int)std::min(strtol(p, (char**)&p, 10), 64L);
    while (*p == '/') {
      p++;
      long d = isdigit((unsigned char)*p) ? strtol(p, (char**)&p, 10) : 2;
      if (d > 0 && den * d <= 64) den *= (int)d;
      else warn("unsupported note length, ignored");
    }
    if (num <= 0) { num = 1; warn("zero-length note, treated as one unit"); }
  };

  // Parse one pitch (accidental, letter, octave marks). Returns -1 if none at p.
  auto readPitch = [&](bool remember) -> int {
    const char* q = p;
    int acc = 0;
    bool explicitAcc = false;
    while (*q == '^' || *q == '_' || *q == '=') {
      explicitAcc = true;
      acc += *q == '^' ? 1 : *q == '_' ? -1 : 0;
      q++;
    }
    int li = letterIndex(*q);
    if (li < 0 || !strchr("ABCDEFGabcdefg", *q)) return -1;
    int octave = islower((unsigned char)*q) ? 5 : 4;
    q++;
    while (*q == '\'' || *q == ',') octave += *q++ == '\'' ? 1 : -1;
    octave = std::clamp(octave, 0, 8);
    p = q;
    if (explicitAcc) { if (remember) barAcc[{li, octave}] = acc; }
    else if (auto it = barAcc.find({li, octave}); it != barAcc.end()) acc = it->second;
    else acc = keyAlter(s.key, li);
    return 12 * (octave + 1) + NATURAL_PC[li] + acc;
  };

  auto addTechnique = [](Event& e, const std::string& t) {
    if (!t.empty() && std::find(e.techniques.begin(), e.techniques.end(), t) == e.techniques.end()) e.techniques.push_back(t);
  };

  auto pushNoteOrRest = [&](Kind k, int midi, int num, int den) {
    Event e;
    e.kind = k;
    e.midi = midi;
    long len = (long)unitTicks * num * brokenNum / (den * brokenDen);
    brokenNum = brokenDen = 1;
    if (tupLeft > 0) {
      if (len * tupQ % tupP) warn("tuplet lengths rounded to the timing grid");
      len = len * tupQ / tupP;
      e.tuplet = tupP;
      e.tupletQ = tupQ;
      e.tupletStart = tupStart;
      tupStart = false;
      tupLeft--;
    }
    if (len <= 0) { warn("note shorter than the timing grid, rounded up"); len = 1; }
    e.len = (int)len;
    for (auto& d : decos) {
      std::string t = decoration(d, catalog);
      if (t == "breath") { if (lastNote >= 0) addTechnique(ev[lastNote], t); }
      else if (!t.empty() && k == Kind::Note) addTechnique(e, t);
    }
    decos.clear();
    if (k == Kind::Note && !graces.empty()) {
      bool above = false, below = false;
      for (int g : graces) { above |= g > midi; below |= g < midi; }
      int nAbove = 0;
      for (int g : graces) nAbove += g > midi;
      addTechnique(e, above && below ? "roll" : below ? "tap" : nAbove >= 2 ? "double_cut" : "cut");
    }
    graces.clear();
    ev.push_back(e);
    lastNote = (int)ev.size() - 1;
  };

  auto pushBar = [&](const std::string& b) {
    barAcc.clear();
    if (ev.empty() && b == "|") return;
    if (!ev.empty() && ev.back().kind == Kind::Bar) {
      // merge e.g. ":|" followed by "|:" written separately
      Event& last = ev.back();
      if (last.bar == ":|" && b == "|:") { last.bar = "::"; return; }
    }
    Event e;
    e.kind = Kind::Bar;
    e.bar = b;
    ev.push_back(e);
  };

  while (*p) {
    char c = *p;
    if (isspace((unsigned char)c) || c == '`' || c == '\\' || c == 'y' || c == ')') { p++; continue; }
    if (c == '%') { while (*p && *p != '\n') p++; continue; }
    if (c == '"') { p++; while (*p && *p != '"' && *p != '\n') p++; if (*p == '"') p++; continue; }
    if (c == '!' || c == '+') {
      const char* e = p + 1;
      while (*e && *e != c && *e != '\n' && !isspace((unsigned char)*e) && *e != '|') e++;
      if (*e == c && e > p + 1) { decos.emplace_back(p + 1, e); p = e + 1; }
      else p++;  // thesession.org uses a lone "!" as a line break
      continue;
    }
    if (c == '~' || c == '.' || c == 'T' || c == 'L') { decos.emplace_back(1, c); p++; continue; }
    if (strchr("HJKMNOPQRSUVWuv", c)) { p++; continue; }  // other one-letter decorations (fermata, bowing, ...)
    if (c == '{') {
      p++;
      while (*p && *p != '}') {
        int g = readPitch(false);
        if (g >= 0) { graces.push_back(g); int a, b; readLength(a, b); }
        else p++;
      }
      if (*p == '}') p++;
      continue;
    }
    if (c == '(') {
      if (isdigit((unsigned char)p[1])) {
        p++;
        tupP = (int)strtol(p, (char**)&p, 10);
        tupQ = 0;
        tupLeft = tupP;
        if (*p == ':') { p++; if (isdigit((unsigned char)*p)) tupQ = (int)strtol(p, (char**)&p, 10); }
        if (*p == ':') { p++; if (isdigit((unsigned char)*p)) tupLeft = (int)strtol(p, (char**)&p, 10); }
        if (!tupQ) tupQ = (tupP == 3 || tupP == 6) ? 2 : (tupP == 2 || tupP == 4 || tupP == 8) ? 3 : compoundMeter(s.meter) ? 3 : 2;
        if (tupP <= 0) tupLeft = 0;
        tupStart = true;
      } else p++;  // slur: whistle players slur by default
      continue;
    }
    if (c == '[') {
      if (isdigit((unsigned char)p[1])) {  // volta "[1"
        p++;
        std::string n;
        while (isdigit((unsigned char)*p) || *p == ',' || *p == '-') n += *p++;
        if (ev.empty() || ev.back().kind != Kind::Bar) { Event b; b.kind = Kind::Bar; b.bar = "|"; ev.push_back(b); barAcc.clear(); }
        ev.back().ending = n;
        continue;
      }
      if (isalpha((unsigned char)p[1]) && p[2] == ':') {  // inline field
        if (strchr("KML", p[1])) warn(std::string("inline [") + p[1] + ":] change ignored (one key/meter per song)");
        while (*p && *p != ']') p++;
        if (*p) p++;
        continue;
      }
      if (p[1] == '|') { p += 2; pushBar("||"); continue; }
      // chord: keep the highest note, the whistle plays one line
      p++;
      int top = -1, num = 1, den = 1;
      while (*p && *p != ']') {
        const char* before = p;
        int m = readPitch(true);
        if (m >= 0) { int a, b; readLength(a, b); if (m > top) { top = m; num = a; den = b; } }
        else if (p == before) p++;
      }
      if (*p == ']') p++;
      int a, b;
      readLength(a, b);
      if (top >= 0) { pushNoteOrRest(Kind::Note, top, num * a, den * b); warn("chords reduced to their top note"); }
      continue;
    }
    if (c == '|' || c == ':') {
      std::string run;
      while (*p == '|' || *p == ':' || *p == ']') run += *p++;
      std::string b = "|";
      bool startColon = run[0] == ':', endColon = run.back() == ':';
      if (startColon && endColon && run.size() > 1) b = "::";
      else if (startColon) b = ":|";
      else if (endColon) b = "|:";
      else if (run.find(']') != std::string::npos) b = "|]";
      else if (run.find("||") != std::string::npos) b = "||";
      pushBar(b);
      if (isdigit((unsigned char)*p)) {
        std::string n;
        while (isdigit((unsigned char)*p) || *p == ',' || *p == '-') n += *p++;
        if (!ev.empty()) ev.back().ending = n;
      }
      continue;
    }
    if (c == '>' || c == '<') {
      int n = 0;
      while (*p == c) { n++; p++; }
      if (n > 3) { warn("broken rhythm deeper than >>> treated as >>>"); n = 3; }
      int den = 1 << n;
      int longNum = 2 * den - 1;  // > : 3/2, >> : 7/4, >>> : 15/8
      if (lastNote >= 0) {
        Event& prev = ev[lastNote];
        prev.len = c == '>' ? prev.len * longNum / den : prev.len / den;
      }
      brokenNum = c == '>' ? 1 : longNum;
      brokenDen = den;
      continue;
    }
    if (c == '-') { if (lastNote >= 0 && ev[lastNote].kind == Kind::Note) ev[lastNote].tie = true; p++; continue; }
    if (c == 'z' || c == 'x') {
      p++;
      int a, b;
      readLength(a, b);
      pushNoteOrRest(Kind::Rest, 0, a, b);
      continue;
    }
    if (c == 'Z' || c == 'X') {  // multi-bar rest
      p++;
      int a, b;
      readLength(a, b);  // num is capped at 64 bars
      for (int i = 0; i < a; i++) {
        Event e;
        e.kind = Kind::Rest;
        e.len = meterTicks(s.meter);
        ev.push_back(e);
        if (i + 1 < a) pushBar("|");
      }
      continue;
    }
    const char* before = p;
    int m = readPitch(true);
    if (m >= 0) {
      int a, b;
      readLength(a, b);
      pushNoteOrRest(Kind::Note, m, a, b);
      continue;
    }
    p = before + 1;
    warn(std::string("ignored unsupported ABC symbol '") + c + "'");
  }
  while (!ev.empty() && ev.back().kind == Kind::Bar && ev.back().bar == "|" && ev.back().ending.empty() && ev.size() > 1 &&
         ev[ev.size() - 2].kind == Kind::Bar)
    ev.pop_back();
  if (ev.empty()) warn("no notes found");
  return r;
}

}  // namespace john
