// Deterministic hints about where techniques *could* go. The agent decides what actually fits the tune.
#include "john.hpp"
#include <algorithm>
#include <sstream>
#include <tuple>

namespace john {

namespace {

struct N {
  int idx, bar, tick, len;  // len includes notes tied onto this one
  int midi;
  bool afterRest, phraseStart;
};

}  // namespace

RangeCost rangeCost(const Song& s, const Instrument& in, int semis) {
  RangeCost c;
  for (auto& e : s.events) {
    if (e.kind != Kind::Note) continue;
    if (in.id != "whistle") { c.out += !in.plays(e.midi + semis); continue; }
    Fingering f = finger(in.key, e.midi + semis);
    if (!f.ok) c.out++;
    else if (f.holes.find('H') != std::string::npos) c.half++;
    else if (f.holes == "OXXOOO") c.cross++;
  }
  return c;
}

// transposeToKey's shift, without applying it
static int keyShift(const Song& s, int tonic, const Instrument& in) {
  int d = ((tonic - tonicPc(s.key)) % 12 + 12) % 12;
  if (d > 6) d -= 12;  // smallest interval: -5..+6
  auto score = [&](int t) { RangeCost c = rangeCost(s, in, t); return std::tuple(c.out, c.hard(), std::abs(s.transposed + t), std::abs(t)); };
  int best = d;
  for (int t = d - 24; t <= d + 24; t += 12) {
    bool midi = true;  // stays inside 0..127
    for (auto& e : s.events) midi = midi && (e.kind != Kind::Note || (e.midi + t >= 0 && e.midi + t <= 127));
    if (midi && score(t) < score(best)) best = t;
  }
  return best;
}

int transposeToKey(Song& s, int tonic, const Instrument& in) {
  int t = keyShift(s, tonic, in);
  transpose(s, t);
  return t;
}

bool fits(const RangeCost& c, int notes) {
  return c.out == 0 && (c.hard() == 0 || c.hard() * 10 < notes * PLAYABLE_PERCENT);  // hard() is 10 per half-hole
}

Playability playability(const Song& s, const Instrument& in) {
  Playability p;
  for (auto& e : s.events) p.notes += e.kind == Kind::Note;
  p.key = s.key;
  p.cost = rangeCost(s, in, 0);
  if (fits(p.cost, p.notes)) return p;
  auto score = [&](int t, const RangeCost& c) {
    int d = ((t % 12) + 12) % 12;
    return std::tuple(c.out, !fits(c, p.notes), std::min(d, 12 - d), c.hard(), std::abs(t));
  };
  for (int pc = 0; pc < 12; pc++) {
    int t = keyShift(s, pc, in);
    RangeCost c = rangeCost(s, in, t);
    if (score(t, c) < score(p.shift, p.cost)) { p.shift = t; p.cost = c; }
  }
  p.verdict = fits(p.cost, p.notes) ? Playability::OtherKey : Playability::NoFit;
  Song k;  // no notes: only the key
  k.key = s.key;
  transpose(k, p.shift);
  p.key = k.key;
  return p;
}

std::string analyze(const Song& s) {
  std::ostringstream o;
  const bool compound = compoundMeter(s.meter);
  const int beat = compound ? WHOLE * 3 / 8 : WHOLE / 4;
  const Instrument in = instrumentOf(s);
  const bool whistle = in.id == "whistle";
  const int base = whistle ? in.low : whistleBase("D");  // technique candidates follow tin whistle fingering

  // ---- collect notes with positions; merge tie chains ----
  std::vector<N> notes;
  std::vector<int> phraseEndBars;  // bar numbers ending with a repeat/double bar
  auto pos = positions(s);
  bool restBefore = true, startNext = true, tieOpen = false;
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    if (e.kind == Kind::Bar) {
      // the bar a repeat sign closes is the one the previous note belongs to
      if (e.bar != "|" && !notes.empty()) { phraseEndBars.push_back(notes.back().bar); startNext = true; }
      continue;
    }
    if (e.kind == Kind::Rest) { restBefore = true; tieOpen = false; continue; }
    if (tieOpen && !notes.empty() && notes.back().midi == e.midi) notes.back().len += e.len;
    else notes.push_back({(int)i, pos[i].bar, pos[i].tick, e.len, e.midi, restBefore, startNext || restBefore});
    tieOpen = e.tie;
    restBefore = startNext = false;
  }
  if (notes.empty()) return "No notes.\n";
  int lastBar = notes.back().bar;

  // ---- range / fingering ----
  o << "== Range on the " << in.name << " (playable " << noteName(spell(s.key, in.low)) << " .. "
    << noteName(spell(s.key, in.high)) << ") ==\n";
  RangeCost now = rangeCost(s, in, 0);
  o << "current: " << now.out << " out of range";
  if (whistle) o << ", " << now.half << " half-holed, " << now.cross << " C-natural cross-fingered";
  o << "\n";
  std::vector<std::pair<int, int>> ranked;  // score, semis
  for (int t = -12; t <= 12; t++) {
    RangeCost c = rangeCost(s, in, t);
    ranked.push_back({c.out * 1000 + c.hard() + std::abs(t), t});
  }
  std::sort(ranked.begin(), ranked.end());
  o << "best transpositions:";
  for (int i = 0; i < 3; i++) {
    RangeCost c = rangeCost(s, in, ranked[i].second);
    Song tmp = s;
    transpose(tmp, ranked[i].second);
    o << "  [" << (ranked[i].second > 0 ? "+" : "") << ranked[i].second << " -> " << keyName(tmp.key) << ": " << c.out << " out";
    if (whistle) o << ", " << c.half << " half, " << c.cross << " cross";
    o << "]";
  }
  o << "\n";
  for (auto& n : notes) {
    Fingering f = whistle ? finger(s.whistle, n.midi) : Fingering{"", 0, in.plays(n.midi)};
    std::string nm = noteName(spell(s.key, n.midi));
    if (!f.ok) o << "#" << n.idx << " bar " << n.bar << " " << nm << ": OUT OF RANGE\n";
    else if (f.holes.find('H') != std::string::npos)
      o << "#" << n.idx << " bar " << n.bar << " " << nm << ": half-hole " << fingeringText(f) << " (hard; consider transposing or an alternative note)\n";
  }

  // ---- technique candidates ----
  o << "\n== Technique candidates (hints, not rules; " << (compound ? "compound" : "simple") << " meter, beat = "
    << (compound ? "dotted quarter" : "quarter") << ") ==\n";
  if (!whistle) o << "(the candidates follow D tin whistle fingering; on the " << in.name << " read them as phrasing hints)\n";
  auto line = [&](const N& n, const std::string& what) {
    o << "#" << n.idx << " bar " << n.bar << " beat " << (1 + (double)n.tick / beat) << " "
      << noteName(spell(s.key, n.midi)) << ": " << what << "\n";
  };
  for (size_t k = 0; k < notes.size(); k++) {
    const N& n = notes[k];
    int off = n.midi - base;
    int pc = ((off % 12) + 12) % 12;
    bool onBeat = n.tick % beat == 0;
    const N* prev = k > 0 ? &notes[k - 1] : nullptr;
    const N* next = k + 1 < notes.size() ? &notes[k + 1] : nullptr;

    if (n.phraseStart) line(n, "tongue (phrase start / after a rest)");
    if (prev && prev->midi == n.midi && !n.afterRest)
      line(n, "cut (or tongue) to separate the repeated note");

    bool rollable = pc >= 2 && pc <= 9;  // E..B; not bottom D (no tap), awkward on C/C#
    if (n.len >= WHOLE * 3 / 8) {
      if (pc == 0) line(n, "cran (long D: rolls are impossible on bottom D)");
      else if (rollable) line(n, std::string("long roll") + (pc == 2 && off < 12 ? " or cran (bottom E)" : "") + (onBeat ? " (long note on the beat)" : ""));
      else line(n, "cut or vibrato (roll impractical on C/C#)");
    } else if (n.len == WHOLE / 4) {
      if (compound && next && next->midi == n.midi && next->len == WHOLE / 8 && onBeat)
        line(n, "long roll covering this note and the next (written 'X2X' is often played '~X3')");
      else if (rollable) line(n, "short roll or cut (quarter note)");
      else if (pc == 0) line(n, "cut or short cran");
    }
    bool stepUp = prev && prev->midi < n.midi && n.midi - prev->midi <= 2;
    if (pc >= 2 && pc <= 9 && off < 12 && ((n.len >= WHOLE / 4 && n.phraseStart) || (n.len >= WHOLE * 3 / 8 && stepUp)))
      line(n, "slide up into the note (expressive, mostly in slower tunes)");
  }

  // ---- breathing: one breath per ~4-bar phrase, after the longest note of the phrase's last bar ----
  o << "\n== Breath candidates ==\n";
  for (int b = 1; b <= lastBar; b++) {
    bool boundary = b % 4 == 0 || std::count(phraseEndBars.begin(), phraseEndBars.end(), b);
    if (!boundary || b == lastBar) continue;
    const N* best = nullptr;
    for (auto& n : notes) if (n.bar == b && (!best || n.len >= best->len)) best = &n;
    if (best) line(*best, "breath after (end of phrase; shorten this note or drop the next weak note)");
  }
  o << "\nApply with set_techniques; add a short comment explaining musical choices so the player learns from them.\n";
  return o.str();
}

}  // namespace john
