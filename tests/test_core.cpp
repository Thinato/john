// Core self-checks: key parsing, spelling, fingerings, ABC import, transpose, analysis, JSON round trip.
#undef NDEBUG  // the checks are asserts: keep them in Release builds
#include "john.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

using namespace john;

static const Event& nth(const Song& s, int n) {  // n-th note/rest (skipping bars)
  for (auto& e : s.events) if (e.kind != Kind::Bar && n-- == 0) return e;
  assert(false);
  return s.events[0];
}

int main() {
  fs::path home = fs::temp_directory_path() / ("john-test-" + std::to_string(getpid()));
  setenv("JOHN_HOME", home.c_str(), 1);

  assert(parseKey("D")->fifths == 2);
  assert(parseKey("Ador")->fifths == 1 && parseKey("Ador")->mode == "dor");
  assert(parseKey("Bm")->fifths == 2 && parseKey("Bminor")->mode == "min");
  assert(parseKey("G Mixolydian")->fifths == 0);
  assert(parseKey("Bb")->fifths == -2);
  assert(parseKey("Dmajor")->fifths == 2);  // thesession.org style
  assert(!parseKey("H"));
  assert(keyName({1, "dor"}) == "A dorian");
  assert(keyName({-2, "maj"}) == "Bb major");
  assert(keyName({3, "min"}) == "F# minor");

  Key d{2, "maj"};
  assert(noteName(spell(d, 66)) == "F#4");
  assert(noteName(spell(d, 72)) == "C5");
  assert(noteName(spell({-1, "maj"}, 70)) == "Bb4");
  assert(noteName(spell({0, "maj"}, 61)) == "C#4");

  assert(finger("D", 62).holes == "XXXXXX" && finger("D", 62).octave == 0);
  assert(finger("D", 79).holes == "XXXOOO" && finger("D", 79).octave == 1);
  assert(finger("D", 72).holes == "OXXOOO");
  assert(finger("D", 85).ok && !finger("D", 86).ok && !finger("D", 61).ok);
  assert(finger("C", 60).holes == "XXXXXX");
  assert(fingeringText(finger("D", 76)) == "XXX XXO +");

  // ABC: roll, grace cut, tuplet, rest, tie, voltas, key signature
  auto r = importAbc("X:1\nT:Test Jig\nM:6/8\nL:1/8\nK:D\n|:~A3 {g}AFA|d2e f2d|(3efg a2 z|1 d3- d2:|2 d3 d2B||\n");
  Song& s = r.song;
  assert(s.title == "Test Jig" && s.meter == "6/8" && s.key.fifths == 2);
  assert(s.events[0].kind == Kind::Bar && s.events[0].bar == "|:");
  assert(nth(s, 0).midi == 69 && nth(s, 0).len == 72 && nth(s, 0).techniques == std::vector<std::string>{"roll"});
  assert(nth(s, 1).techniques == std::vector<std::string>{"cut"});
  assert(nth(s, 2).midi == 66);  // F is sharp in D
  assert(nth(s, 8).len == 16 && nth(s, 8).tuplet == 3 && nth(s, 8).tupletStart && !nth(s, 9).tupletStart);
  assert(nth(s, 12).kind == Kind::Rest);
  assert(nth(s, 13).tie);
  int endings = 0, repeats = 0;
  for (auto& e : s.events) { endings += !e.ending.empty(); repeats += e.bar == ":|"; }
  assert(endings == 2 && repeats == 1);
  assert(r.warnings.empty());

  // broken rhythm and accidentals that last for the bar
  auto b = importAbc("X:1\nT:b\nM:4/4\nL:1/8\nK:G\nA>B c<d|\n").song;
  assert(nth(b, 0).len == 36 && nth(b, 1).len == 12 && nth(b, 2).len == 12 && nth(b, 3).len == 36);
  auto a = importAbc("X:1\nT:a\nM:4/4\nL:1/8\nK:D\n=F F f2 | F4|\n").song;
  assert(nth(a, 0).midi == 65 && nth(a, 1).midi == 65 && nth(a, 2).midi == 78 && nth(a, 3).midi == 66);
  auto acc = accidentals(a);
  assert(acc[0].shown && acc[0].alter == 0 && !acc[1].shown);

  // thesession.org bare body: no headers, "!" line breaks
  auto t = importAbc("A2FA A2dB|A2FA BEE2|! A2FA A2Bd|egfd edBd|");
  assert(t.warnings == std::vector<std::string>{"no K: field, assumed D major"} && nth(t.song, 1).midi == 66 && t.song.title == "Untitled");

  // pickup bar is bar 0; positions follow elapsed time
  auto pk = importAbc("X:1\nT:p\nM:4/4\nL:1/8\nK:Edor\n|:D2|EBBA B2EB|]\n").song;
  auto ps = positions(pk);
  assert(ps[1].bar == 0 && ps[1].tick == WHOLE * 3 / 4);  // D2 pickup
  assert(ps[3].bar == 1 && ps[3].tick == 0);                // first E

  // review regressions: header comments, volta at start, zero/huge lengths, odd meters, voltas restart time
  auto hc = importAbc("%abc-2.1\nX:1\nT:G jig\n% comment\nM:6/8\nL:1/8\nK:G\nGAB cBc|\n");
  assert(hc.song.key.fifths == 1 && hc.song.meter == "6/8" && nth(hc.song, 3).midi == 72);
  assert(!importAbc("[1 AB|").song.events.empty());
  assert(!importAbc("|1 AB|").song.events.empty());
  assert(nth(importAbc("X:1\nK:D\nA/0 B|").song, 0).len > 0);
  assert(importAbc("X:1\nK:D\nZ3000000|").song.events.size() < 200);
  assert(meterTicks("1/256") > 0 && meterTicks("0/4") == WHOLE);
  assert(whistleBase("Dm") < 0 && whistleBase("Bb") == 70 && whistleBase("D") == 62);
  auto vt = importAbc("X:1\nT:v\nM:6/8\nL:1/8\nK:D\n|:A|d2d fed|1 d3 d2:|2 d3 d2e||f3 f2e|\n").song;
  auto vp = positions(vt);
  for (size_t i = 0; i < vt.events.size(); i++)
    if (vt.events[i].kind == Kind::Note && vt.events[i].midi == 78 && vt.events[i].len == 36) assert(vp[i].tick == 0);

  // play order: repeats and 1st/2nd endings
  auto po = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:D\n|: A B |1 c :|2 d |] |: e :: f :|\n").song;
  std::string seq;
  for (int i : playOrder(po)) seq += noteName(spell(po.key, po.events[i].midi)).substr(0, 1);
  assert(seq == "ABCABDEEFF");
  auto orderOf = [](const std::string& body) {
    auto so = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:C\n" + body + "\n").song;
    std::string r;
    for (int i : playOrder(so)) r += noteName(spell(so.key, so.events[i].midi)).substr(0, 1);
    return r;
  };
  assert(orderOf("|: A B |1 c :|2 d || e f |1 g :|2 a |]") == "ABCABDEFGEFA");  // B part without |:
  assert(orderOf("|: A B |1 c :| d |]") == "ABCABD");                          // implied 2nd ending
  assert(orderOf("|: A |1 B :|2 C :|3 D |]") == "ABACAD");                      // three endings
  assert(orderOf("A B :| c |]") == "ABABC");                                     // repeat to the start

  // ties merge only the tied notes; the next same-pitch note is re-articulated
  auto tie = perform(importAbc("X:1\nT:t\nM:4/4\nL:1/4\nK:D\nA- A A B|\n").song, 120, 0, false);
  assert(tie.voices.size() == 3 && std::abs(tie.voices[0].dur - 1.0) < 1e-9);

  // count-in stays on the beat grid for a jig pickup: clicks every dotted quarter
  auto jig = perform(importAbc("X:1\nT:j\nM:6/8\nL:1/8\nK:D\nA|d2d fed|\n").song, 120, 0, true);
  for (size_t i = 1; i < jig.clicks.size(); i++) assert(std::abs(jig.clicks[i].first - jig.clicks[i - 1].first - 0.75) < 1e-9);
  assert(std::abs(jig.voices[0].start - (jig.countIn)) < 1e-9 && std::abs(jig.voices[1].start - jig.clicks[2].first - 0.75) < 1e-9);

  // performance timing: quarter = 0.5 s at 120 bpm, one bar count-in, ties merge, cut adds a grace
  auto pf = importAbc("X:1\nT:p\nM:4/4\nL:1/4\nK:D\n{g}A B- B c|\n").song;
  Performance perf = perform(pf, 120);
  assert(std::abs(perf.countIn - 2.0) < 1e-9 && perf.voices.size() == 3);
  assert(std::abs(perf.voices[0].start - 2.0) < 1e-9 && std::abs(perf.voices[1].dur - 1.0) < 1e-9);
  assert(perf.voices[0].graces.size() == 1 && std::abs(perf.length - 4.0) < 1e-9);
  assert(perf.clicks.size() == 8 && perf.clicks[0].second && !perf.clicks[1].second);

  // pitch detection: pure sine, synthesized whistle note, silence
  {
    const int sr = 48000, n = 2048;
    std::vector<float> buf(n);
    for (int i = 0; i < n; i++) buf[i] = 0.5f * std::sin(6.283185307f * 880.0f * i / sr);
    assert(std::abs(detectPitch(buf.data(), n, sr) - 880) < 2);
    Song one = importAbc("X:1\nT:x\nM:4/4\nL:1/4\nK:D\nA4|\n").song;  // A4 written, sounds A5 = 880 Hz
    Performance p1 = perform(one, 60, 0, false);
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 2;  // skip the attack
    std::vector<float> w(n);
    syn.render(p1, sp, w.data(), n);
    float hz = detectPitch(w.data(), n, sr);
    assert(std::abs(writtenMidiFromFreq(hz, 1) - 69) < 0.2);
    std::vector<float> quiet(n, 0.0f);
    assert(detectPitch(quiet.data(), n, sr) == 0 && rms(quiet.data(), n) == 0);
  }

  // instruments: ranges, written vs sounding octave, JSON, playback pitch, range hints
  {
    for (auto& id : INSTRUMENTS) assert(findInstrument(id));
    assert(!findInstrument("kazoo") && !findInstrument("whistle", "Dm"));
    Instrument cw = *findInstrument("whistle", "C");
    assert(cw.low == 60 && cw.high == 83 && cw.octave == 1 && cw.name == "C whistle");
    assert(findInstrument("guitar")->octave == -1 && findInstrument("violin")->plays(55) && !findInstrument("violin")->plays(54));
    assert(std::abs(soundingFreq(69, 0) - 440) < 1e-9 && std::abs(soundingFreq(69, 1) - 880) < 1e-9);
    assert(std::abs(soundingFreq(69, -1) - 220) < 1e-9 && std::abs(writtenMidiFromFreq(220, -1) - 69) < 1e-9);

    Song v = importAbc("X:1\nT:v\nM:4/4\nL:1/4\nK:D\nA4|\n").song;
    assert(v.instrument == "whistle" && fromJson(toJson(v)).instrument == "whistle");
    json old = toJson(v);
    old.erase("instrument");  // songs saved before instruments existed
    assert(fromJson(old).instrument == "whistle");
    v.instrument = "violin";
    assert(fromJson(toJson(v)).instrument == "violin" && instrumentOf(v).name == "Violin");
    Song odd = v;
    odd.instrument = "kazoo";
    assert(instrumentOf(odd).name == "D whistle");

    const int sr = 48000, n = 2048;  // written A4 on a violin sounds A4 = 440 Hz (a whistle would sound 880)
    Performance pv = perform(v, 60, 0, false);
    assert(pv.octave == 0);
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 2;
    std::vector<float> w(n);
    syn.render(pv, sp, w.data(), n);
    assert(std::abs(detectPitch(w.data(), n, sr, 200, 3000) - 440) < 3);

    Song low = importAbc("X:1\nT:l\nM:4/4\nL:1/4\nK:C\nG,4|F,4|\n").song;  // G3 is a violin's lowest note, F3 is below it
    low.instrument = "violin";
    std::string an = analyze(low), ls = listing(low);
    assert(an.find("Range on the Violin (playable G3 .. E7)") != std::string::npos && an.find("current: 1 out of range\n") != std::string::npos);
    assert(an.find("half") == std::string::npos && an.find("phrasing hints") != std::string::npos);
    assert(ls.find("instrument: Violin") != std::string::npos && ls.find("out-of-range") != std::string::npos);
    low.instrument = "piano";
    assert(analyze(low).find("current: 0 out of range") != std::string::npos);
  }

  // playback sounds: every voice keeps the song's pitch, struck/plucked voices decay, no NaN or clipping
  {
    const int sr = 48000, n = 2048;
    Song v = importAbc("X:1\nT:v\nM:4/4\nL:1/4\nK:D\nA4|\n").song;  // 4 s whole note
    v.instrument = "violin";                                         // written A4 sounds 440 Hz
    auto window = [&](const std::vector<float>& x, double at) { return x.data() + (int)(at * sr); };
    for (auto& id : INSTRUMENTS) {
      Performance p = perform(v, 60, 0, false, id);
      assert(p.sound == id && p.octave == 0);
      Synth syn;
      syn.metronome = false;
      syn.volume = 1;
      long long sp = 0;
      std::vector<float> x(sr * 9 / 2);
      syn.render(p, sp, x.data(), (int)x.size());
      for (float f : x) assert(std::isfinite(f) && std::abs(f) < 1);
      float hz = detectPitch(window(x, 0.15), n, sr, 100, 3000);
      if (std::abs(hz - 440) >= 3) fprintf(stderr, "%s: %.1f Hz\n", id.c_str(), hz);
      assert(std::abs(hz - 440) < 3);
      float early = rms(window(x, 0.1), n), late = rms(window(x, 2.5), n);
      bool decays = id == "piano" || id == "guitar" || id == "electric-guitar";
      assert(early > 0.05f && (decays ? late < 0.6f * early : late > 0.8f * early));
      assert(rms(window(x, 4.2), n) < 0.01f);  // silent after the note
    }
    Song w = v;  // the octave comes from the song's instrument, not the sound: a whistle song on guitar still sounds A5
    w.instrument = "whistle";
    Performance pg = perform(w, 60, 0, false, "guitar");
    assert(pg.octave == 1 && perform(w, 60, 0, false, "kazoo").sound == "whistle" && perform(w, 60).sound == "whistle");
    Synth syn;
    syn.metronome = false;
    long long sp = sr / 10;
    std::vector<float> x(n);
    syn.render(pg, sp, x.data(), n);
    assert(std::abs(detectPitch(x.data(), n, sr) - 880) < 5);

    // which ornaments each voice plays (kb/notes/playback-voice-ornaments.md)
    Song o = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:D\n{g}A !breath! B !slide!c !vibrato!d|\n").song;
    auto on = [&](const char* id) { return perform(o, 60, 0, false, id).voices; };
    auto wh = on("whistle"), vn = on("violin"), pn = on("piano"), gt = on("guitar"), hm = on("harmonica");
    assert(wh[0].graces.size() == 1 && wh[0].gap > 0.1 && wh[2].graces.size() == 1 && wh[3].vibrato);
    assert(vn[0].graces.size() == 1 && vn[0].gap == 0 && vn[2].graces.size() == 1 && vn[3].vibrato);  // no breaths on a bow
    assert(pn[0].graces.empty() && pn[0].gap < 0.02 && pn[2].graces.empty() && !pn[3].vibrato);      // no ornaments on keys
    assert(gt[0].graces.size() == 1 && gt[2].graces.size() == 1 && !gt[3].vibrato);                  // hammer-ons, slides
    assert(hm[0].graces.empty() && hm[0].gap > 0.1 && hm[2].graces.size() == 1 && hm[3].vibrato);    // breaths, bends
  }

  // transpose D major down a tone -> C major
  Song c = s;
  transpose(c, -2);
  assert(c.key.fifths == 0 && nth(c, 0).midi == 67);

  // delete bars: pickup is bar 0, closing repeat moves back, ties into deleted notes are dropped
  {
    auto t = importAbc("X:1\nT:del\nM:4/4\nL:1/4\nK:D\nA | d e f g | a b c' d'- | d' c' b a :|\n").song;
    Song u = t;
    assert(deleteBars(u, 3, 3) == 4);
    assert(u.events.back().kind == Kind::Bar && u.events.back().bar == ":|");
    assert(!nth(u, 8).tie && nth(u, 8).midi == 86);
    u = t;
    assert(deleteBars(u, 0, 0) == 1 && u.events.front().kind == Kind::Note && u.events.front().midi == 74);
    bool threw = false;
    try { deleteBars(u, 9, 9); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false;
    try { u = t; deleteBars(u, 0, 3); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    // the bar line opening a 1st ending keeps its volta mark when the bar before it goes
    auto v = importAbc("X:1\nT:volta\nM:2/4\nL:1/4\nK:D\n|: d e | f g |1 a b :|2 c' d' |]\n").song;
    assert(deleteBars(v, 2, 2) == 2);
    assert(playOrder(v).size() == 8);  // d e a b d e c' d'
  }

  // transcription: John's synth playing a tune over a louder piano-like accompaniment, then back to notes
  {
    auto tune = importAbc("X:1\nT:t\nM:4/4\nL:1/8\nK:G\nd2 B2 G2 B2|d2 g2 f2 e2|d2 B2 c2 A2|G4 z4|"
                          "g2 f2 e2 d2|e2 c2 A2 c2|B2 d2 A2 F2|G8|]\n").song;
    const int sr = 22050;
    Performance perf = perform(tune, 100, 0, false);
    std::vector<float> mix((size_t)((perf.length + 1) * sr), 0.f);
    Synth syn;
    syn.sampleRate = sr;
    syn.metronome = false;
    long long pos = 0;
    syn.render(perf, pos, mix.data(), (int)mix.size());
    float melodyRms = rms(mix.data(), (int)mix.size());
    // accompaniment: a decaying chord + bass per bar (G G C G | C C D G), 6 harmonics each
    const int chords[8][4] = {{43, 55, 59, 62}, {43, 55, 59, 62}, {48, 55, 60, 64}, {43, 55, 59, 62},
                              {48, 55, 60, 64}, {48, 55, 60, 64}, {50, 57, 62, 66}, {43, 55, 59, 62}};
    std::vector<float> acc(mix.size(), 0.f);
    const double barSec = 2.4;  // 4 beats at 100 bpm
    for (int b = 0; b < 8; b++)
      for (int beat = 0; beat < 4; beat++)
        for (int m : chords[b]) {
          double f0 = 440 * std::pow(2, (m - 69) / 12.0), t0 = b * barSec + beat * 0.6;
          for (size_t i = (size_t)(t0 * sr); i < (size_t)((t0 + 0.6) * sr) && i < acc.size(); i++) {
            double t = (double)i / sr - t0, v = 0;
            for (int h = 1; h <= 6; h++) v += std::sin(2 * M_PI * f0 * h * t) / h;
            acc[i] += (float)(v * std::exp(-3 * t));
          }
        }
    float accRms = rms(acc.data(), (int)acc.size());
    for (size_t i = 0; i < mix.size(); i++) mix[i] += acc[i] * 0.5f * melodyRms / accRms;  // 6 dB under the melody, like a lead over a band
    auto melodyOf = [](const Song& s) {
      std::vector<int> out;  // played pitches; tied continuations merged
      bool tied = false;
      for (auto& e : s.events) {
        if (e.kind == Kind::Bar) continue;
        if (e.kind == Kind::Note && !tied) out.push_back(e.midi);
        tied = e.kind == Kind::Note && e.tie;
      }
      return out;
    };
    auto want = melodyOf(tune);
    auto common = [](const std::vector<int>& a, const std::vector<int>& b) {  // longest common subsequence length
      std::vector<std::vector<int>> L(a.size() + 1, std::vector<int>(b.size() + 1, 0));
      for (size_t i = 1; i <= a.size(); i++)
        for (size_t j = 1; j <= b.size(); j++)
          L[i][j] = a[i - 1] == b[j - 1] ? L[i - 1][j - 1] + 1 : std::max(L[i - 1][j], L[i][j - 1]);
      return L[a.size()][b.size()];
    };
    {  // for a violin the melody is written at the pitch it sounds: an octave above the whistle's written notes
      TranscribeOptions o;
      o.bpm = 100;
      o.instrument = "violin";
      Transcription tr = transcribe(mix, sr, o);
      std::vector<int> up = want;
      for (int& m : up) m += 12;
      auto got = melodyOf(tr.song);
      assert(tr.song.instrument == "violin" && common(up, got) >= (int)up.size() * 9 / 10);
      assert(tr.report.find("Violin plays at the recording's pitch") != std::string::npos);
      bool threw = false;
      o.instrument = "kazoo";
      try { transcribe(mix, sr, o); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
    {  // a melody narrower than an octave fits the whistle twice: take the low octave, easier for beginners
      Song narrow = importAbc("X:1\nT:n\nM:4/4\nL:1/4\nK:D\nd e f g|a g f e|d4|]\n").song;  // written D5..A5
      Performance pn = perform(narrow, 100, 0, false);
      std::vector<float> x((size_t)((pn.length + 1) * sr), 0.f);
      Synth sn;
      sn.sampleRate = sr;
      sn.metronome = false;
      long long at = 0;
      sn.render(pn, at, x.data(), (int)x.size());
      TranscribeOptions o;
      o.bpm = 100;
      std::vector<int> low = melodyOf(narrow);
      for (int& m : low) m -= 12;
      auto got = melodyOf(transcribe(x, sr, o).song);
      assert(common(low, got) >= (int)low.size() - 1);
    }
    {  // notes of irregular lengths have no steady beat: the tempo is flagged as uncertain, with a runner-up
      Song odd = importAbc("X:1\nT:o\nM:4/4\nL:1/16\nK:D\nd3 e5 f2 g7 a3 b6 a2 g5 f3 e7 d2 e4 f9 g3 a5 b2 a6 g3 f7 e2 d5|]\n").song;
      Performance po = perform(odd, 100, 0, false);
      std::vector<float> x((size_t)((po.length + 1) * sr), 0.f);
      Synth so;
      so.sampleRate = sr;
      so.metronome = false;
      long long at = 0;
      so.render(po, at, x.data(), (int)x.size());
      std::string r = transcribe(x, sr, {}).report;
      printf("transcribe (irregular): %s", r.substr(0, r.find('\n') + 1).c_str());
      assert(r.find("confidence high") == std::string::npos && r.find("also plausible: ~") != std::string::npos);
    }
    {  // a short low note that starts a wide leap (D up a tenth) is melody, not accompaniment: keep it (#7)
      Song leap = importAbc("X:1\nT:l\nM:4/4\nL:1/8\nK:D\nD f a f b a f a|D f g a b2 a2|D f a f g f e f|D f a g f4|]\n").song;
      Performance pl = perform(leap, 120, 0, false);
      std::vector<float> x((size_t)((pl.length + 1) * sr), 0.f);
      Synth sl;
      sl.sampleRate = sr;
      sl.metronome = false;
      long long at = 0;
      sl.render(pl, at, x.data(), (int)x.size());
      TranscribeOptions o;
      o.bpm = 120;
      auto got = melodyOf(transcribe(x, sr, o).song), want = melodyOf(leap);
      assert(std::count(got.begin(), got.end(), 62) == 4);
      assert(common(want, got) == (int)want.size() && got.size() == want.size());
    }
    {  // the same whistle tune over a louder low accompaniment: only a search limited to the whistle's range keeps it (#10)
      std::vector<float> loud(mix.size());
      for (size_t i = 0; i < mix.size(); i++) loud[i] = mix[i] + acc[i] * 2 * melodyRms / accRms;  // 8 dB over the melody
      TranscribeOptions o;
      o.bpm = 100;
      auto got0 = melodyOf(transcribe(loud, sr, o).song);
      o.recordingInstrument = "whistle";
      Transcription tr = transcribe(loud, sr, o);
      auto got = melodyOf(tr.song);
      int all = common(want, got0), lim = common(want, got);
      printf("transcribe (loud accompaniment): unlimited %d/%zu notes, %zu extra; limited to the whistle %d/%zu, %zu extra\n", all,
             want.size(), got0.size() - all, lim, want.size(), got.size() - lim);
      // the limited search still hears the chords' harmonics that fall in the whistle's range (low D, G in rests)
      assert(all < (int)want.size() * 7 / 10 && lim >= (int)want.size() * 9 / 10 && got.size() - lim < (got0.size() - all) / 2);
      assert(tr.report.find("pitch search: limited to the D whistle's sounding range, C5..C7\n") != std::string::npos);
      bool threw = false;
      o.recordingInstrument = "kazoo";
      try { transcribe(loud, sr, o); } catch (const std::runtime_error&) { threw = true; }
      assert(threw);
    }
    for (double bpm : {100.0, 0.0}) {
      TranscribeOptions o;
      o.bpm = bpm;
      Transcription tr = transcribe(mix, sr, o);
      auto got = melodyOf(tr.song);
      int hit = common(want, got);  // how many of the tune's notes came back, in order
      printf("transcribe (bpm %s): %d/%zu notes found, %zu extra, tempo %d, key %s\n", bpm > 0 ? "given" : "auto", hit,
             want.size(), got.size() - hit, tr.song.tempo, keyName(tr.song.key).c_str());
      assert(hit >= (int)want.size() * 9 / 10 && got.size() - hit <= want.size() / 10);
      assert(std::abs(tr.song.tempo - 100) <= 4);
      assert(tr.report.find(bpm > 0 ? "bpm (quarter notes), as given\n" : "bpm (quarter notes), confidence high\n") != std::string::npos);
      assert(tr.song.key.fifths == 1);
      // the ABC we hand the agent reads back to the same song
      Song back = importAbc(toAbc(tr.song)).song;
      assert(back.events.size() == tr.song.events.size());
      for (size_t i = 0; i < back.events.size(); i++)
        assert(back.events[i].midi == tr.song.events[i].midi && back.events[i].len == tr.song.events[i].len &&
               back.events[i].tie == tr.song.events[i].tie);
    }
  }

  // decoding: a 16-bit WAV of A5 written by hand, read back resampled to 22050 Hz mono
  {
    fs::create_directories(home);
    fs::path wav = home / "a5.wav";
    const int rate = 44100, n = rate / 2;
    FILE* f = fopen(wav.c_str(), "wb");
    auto u32 = [&](unsigned v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); u32(36 + n * 2); fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    fwrite("data", 1, 4, f); u32(n * 2);
    for (int i = 0; i < n; i++) u16((unsigned short)(short)(12000 * std::sin(2 * M_PI * 880 * i / rate)));
    fclose(f);
    auto x = decodeAudio(wav.string(), 22050);
    assert(std::abs((int)x.size() - 22050 / 2) < 100);
    assert(std::abs(detectPitch(x.data() + 2000, 2048, 22050) - 880) < 5);
    bool threw = false;
    try { decodeAudio((home / "missing.mp3").string(), 22050); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);
  }

  // analysis hints
  std::string an = analyze(s);
  assert(an.find("long roll") != std::string::npos);
  assert(an.find("cut (or tongue)") != std::string::npos);
  assert(an.find("tongue (phrase start") != std::string::npos);
  auto low = importAbc("X:1\nT:low\nM:4/4\nL:1/8\nK:C\nC2 D2 E4|\n").song;
  assert(analyze(low).find("OUT OF RANGE") != std::string::npos);

  // folders: saved only when set, standard ones first in course order, custom A-Z, unsorted last
  {
    Song f = s;
    assert(!toJson(f).contains("folder"));
    f.folder = "Intermediate";
    assert(fromJson(toJson(f)).folder == "Intermediate");
    std::vector<std::string> fs_ = {"", "Zebra", "Advanced", "Celtic", "Lessons", "Beginner"};
    std::sort(fs_.begin(), fs_.end(), folderBefore);
    assert((fs_ == std::vector<std::string>{"Lessons", "Beginner", "Advanced", "Celtic", "Zebra", ""}));
  }

  // listing and JSON round trip through disk
  s.id = "test-jig";
  saveSong(s);
  auto back = loadSong("test-jig");
  assert(back && toJson(*back) == toJson(s));
  assert(listing(s).find("XXO OOO") != std::string::npos);
  assert(!loadSong("../etc"));
  assert(listSongs() == std::vector<std::string>{"test-jig"});

  // lessons: installed once, a deleted lesson stays deleted, and every bundled lesson loads
  {
    fs::path src = home / "lesson-src";
    fs::create_directories(src);
    Song l = s;
    l.id = "lesson-test";
    { std::ofstream f(src / "lesson-test.json"); f << toJson(l).dump(); }
    assert(installLessons(src) == 1 && loadSong("lesson-test"));
    assert(installLessons(src) == 0);
    fs::remove(dataDir() / "songs" / "lesson-test.json");
    assert(installLessons(src) == 0 && !loadSong("lesson-test"));
    int bundled = 0;
    for (auto& e : fs::directory_iterator(lessonsDir())) {
      std::ifstream f(e.path());
      Song b = fromJson(json::parse(f));
      assert(!b.events.empty() && !b.notes.empty());
      for (auto& ev : b.events) assert(ev.kind != Kind::Note || finger(b.whistle, ev.midi).ok);  // all playable
      bundled++;
    }
    assert(bundled >= 8);
  }

  saveCustomTechnique({"bounce", "Bounce", "b", "test"});
  bool found = false;
  for (auto& x : loadTechniques()) found |= x.id == "bounce";
  assert(found);

  // settings: no file -> defaults; round trip; missing keys -> defaults; garbage -> all defaults
  {
    auto write = [&](const char* text) { std::ofstream(home / "settings.json") << text; };
    assert(loadSettings() == Settings{});
    Settings s;
    s.hints = false; s.zoom = 12; s.light = true; s.tempo = 90; s.countIn = false; s.metronome = false;
    s.volume = 0.3f; s.sound = "piano"; s.gate = 0.02f; s.latencyMs = 120; s.tolerance = 25; s.instrument = "violin";
    saveSettings(s);
    assert(loadSettings() == s);
    write(R"({"tempo": 80, "future_setting": 1})");
    Settings d;
    d.tempo = 80;
    assert(loadSettings() == d);
    write("{not json");
    assert(loadSettings() == Settings{});
    write(R"({"hints": "yes"})");  // wrong type
    assert(loadSettings() == Settings{});
    write(R"({"sound": "banjo", "instrument": "kazoo", "zoom": 0, "volume": 9})");  // values the GUI can't use
    Settings c = loadSettings();
    assert(c.sound.empty() && c.instrument == "whistle" && c.zoom == 6 && c.volume == 1);
    fs::remove(home / "settings.json");
  }

  fs::remove_all(home);
  puts("core tests passed");
}
