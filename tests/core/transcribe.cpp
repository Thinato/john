// Core tests: audio transcription and decoding.
#include "test.hpp"

// Share of the track's voiced frames, placed on the song through PitchTrack::sync, that land on a note of the same
// pitch class (the written octave differs from the recording's by the instrument).
static double aligned(const Song& s, const PitchTrack& t) {
  std::vector<int> at(s.events.size() + 1, 0);
  for (size_t i = 0; i < s.events.size(); i++) at[i + 1] = at[i] + (s.events[i].kind == Kind::Bar ? 0 : s.events[i].len);
  int same = 0, all = 0;
  size_t i = 0;
  for (auto& p : t.points) {
    double tick = trackTick(t, p.time);
    while (i < s.events.size() && at[i + 1] <= tick) i++;
    if (i == s.events.size()) break;
    if (tick < 0 || !p.midi || s.events[i].kind != Kind::Note) continue;
    all++;
    same += (p.midi - s.events[i].midi) % 12 == 0;
  }
  return all ? (double)same / all : 0;
}

  // transcription: John's synth playing a tune over a louder piano-like accompaniment, then back to notes
TEST(transcription) {
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
    {  // D minor from low C to high A fits no octave of the D whistle (low C is 19% of the time): transposed +2 to
       // E minor before folding (#9). transpose = 0 keeps the key and folds low C up; transpose = 4 is used as given.
      Song dm = importAbc("X:1\nT:m\nM:4/4\nL:1/4\nK:Dm\nC D F A|d f a f|e c A F|C4|D F A d|f e d c|A F E C|D4|]\n").song;
      Performance pm = perform(dm, 100, 0, false);
      std::vector<float> x((size_t)((pm.length + 1) * sr), 0.f);
      Synth sm;
      sm.sampleRate = sr;
      sm.metronome = false;
      long long at = 0;
      sm.render(pm, at, x.data(), (int)x.size());
      auto shifted = [&](int semis, bool foldC) {
        std::vector<int> v = melodyOf(dm);
        for (int& m : v) m += m == 60 && foldC ? 12 : semis;
        return v;
      };
      TranscribeOptions o;
      o.bpm = 100;
      Transcription tr = transcribe(x, sr, o);
      Song heard = tr.song;
      transpose(heard, -2);
      std::string line = tr.report.substr(tr.report.find("transposed"));
      printf("transcribe (D minor, low C to high A): %s", line.substr(0, line.find('\n') + 1).c_str());
      assert(melodyOf(tr.song) == shifted(2, false));
      assert(tr.report.find("\ntransposed +2 semitones (" + keyName(heard.key) + " -> " + keyName(tr.song.key) +
                            ") to fit the D whistle: no octave kept the notes in range\n") != std::string::npos);
      assert(tr.report.find("\nkey: " + keyName(heard.key) + " (fit") != std::string::npos && tr.report.find("moved an octave") == std::string::npos);
      o.transpose = 0;
      tr = transcribe(x, sr, o);
      assert(melodyOf(tr.song) == shifted(0, true) && tr.report.find("transposed") == std::string::npos);
      assert(tr.report.find("\n3 note(s) moved an octave") != std::string::npos);
      o.transpose = 4;
      tr = transcribe(x, sr, o);
      assert(melodyOf(tr.song) == shifted(4, false) && tr.report.find("\ntransposed +4 semitones (") != std::string::npos &&
             tr.report.find("), as given\n") != std::string::npos);
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
    {  // the meter is detected: a jig is 6/8 (a beat of three eighths), a waltz 3/4 (#1); the 4/4 mix stays 4/4 (below)
      const std::pair<const char*, const char*> tunes[] = {
          {"X:1\nT:j\nM:6/8\nL:1/8\nK:D\nd2f a2f|g2e c2A|dfa afd|e3 e2A|d2f a2f|g2e c2e|dcB AFA|d3 d3|]\n", "6/8"},
          {"X:1\nT:w\nM:3/4\nL:1/8\nK:D\nd4 f2|a4 f2|g4 e2|c6|d4 f2|a3 g f2|e2 d2 c2|d6|]\n", "3/4"}};
      for (auto [abc, meter] : tunes) {
        Song t = importAbc(abc).song;
        Performance pt = perform(t, 150, 0, false);
        std::vector<float> x((size_t)((pt.length + 1) * sr), 0.f);
        Synth st;
        st.sampleRate = sr;
        st.metronome = false;
        long long at = 0;
        st.render(pt, at, x.data(), (int)x.size());
        Transcription tr = transcribe(x, sr, {});
        std::string r = tr.report.substr(tr.report.find("meter: "));
        printf("transcribe (%s tune): %s", meter, r.substr(0, r.find('\n') + 1).c_str());
        assert(tr.song.meter == meter && r.rfind("meter: " + std::string(meter) + " (detected; " + meter + " ", 0) == 0);
        assert(common(melodyOf(t), melodyOf(tr.song)) >= (int)melodyOf(t).size() * 9 / 10);
        TranscribeOptions o;
        o.meter = "4/4";
        tr = transcribe(x, sr, o);
        assert(tr.song.meter == "4/4" && tr.report.find("\nmeter: 4/4, as given\n") != std::string::npos);
      }
    }
    {  // notes that cross a beat are split into tied values, the middle of a 4/4 bar stays visible; notes on a beat
       // spanning whole beats (half on beat 1 or 3, dotted half, whole, dotted quarter in 6/8) stay single (#3)
      auto written = [](const Song& s) {
        std::vector<std::tuple<int, int, bool>> out;  // pitch, length, tie
        for (auto& e : s.events) if (e.kind == Kind::Note) out.push_back({e.midi, e.len, e.tie});
        return out;
      };
      auto abcOf = [](const std::string& meter, const char* body) { return "X:1\nT:s\nL:1/8\nM:" + meter + "\nK:D\n" + body; };
      // 6 eighths a bar, read as 6/8 and as 3/4; narrow, so the whistle takes the low octave
      const char* jig = "d2 e2 f2|g3 a3|b2 a4|f6|d e2 f2 g|a3 g2 e|d6|]\n";
      const std::tuple<const char*, const char*, const char*> tunes[] = {
          {"4/4", "A2 d4 A2|d3 e3 f2|g8|f4 e4|d e2 f2 g2 a|b6 g2|e2 d6|f8|]\n",
           "A2 d2- d2 A2|d3 e- e2 f2|g8|f4 e4|d e- e f- f g- g a|b6 g2|e2 d2- d4|f8|]\n"},
          {"6/8", jig, "D2 E- E F2|G3 A3|B2 A- A3|F6|D E2 F2 G|A3 G2 E|D6|]\n"},
          {"3/4", jig, "D2 E2 F2|G3 A- A2|B2 A4|F6|D E- E F- F G|A3 G- G E|D6|]\n"}};
      for (auto [meter, abc, want] : tunes) {
        Song t = importAbc(abcOf(meter, abc)).song;
        Performance ps = perform(t, 100, 0, false);
        std::vector<float> x((size_t)((ps.length + 1) * sr), 0.f);
        Synth ss;
        ss.sampleRate = sr;
        ss.metronome = false;
        long long at = 0;
        ss.render(ps, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = 100;
        o.meter = meter;
        assert(written(transcribe(x, sr, o).song) == written(importAbc(abcOf(meter, want)).song));
      }
    }
    {  // eighth triplets come back as (3 groups among straight eighths and sixteenths; swung eighths (2:1) come back
       // straight, with a report line (#2)
      auto written = [](const Song& s) {
        std::vector<std::tuple<int, int, bool, int, bool>> out;  // pitch, length, tie, tuplet, tuplet start
        for (auto& e : s.events) if (e.kind == Kind::Note) out.push_back({e.midi, e.len, e.tie, e.tuplet, e.tupletStart});
        return out;
      };
      auto render = [&](const Song& t, double bpm) {
        Performance p = perform(t, bpm, 0, false);
        std::vector<float> x((size_t)((p.length + 1) * sr), 0.f);
        Synth sy;
        sy.sampleRate = sr;
        sy.metronome = false;
        long long at = 0;
        sy.render(p, at, x.data(), (int)x.size());
        return x;
      };
      Song trip = importAbc("X:1\nT:t\nM:4/4\nL:1/8\nK:D\nD2 (3EFG A2 (3GFE|D2 F2 (3GAB A2|(3BAG (3FED E4|D E F/G/A (3Bcd e2|]\n").song;
      TranscribeOptions o;
      o.bpm = 100;
      o.meter = "4/4";
      Transcription tr = transcribe(render(trip, 100), sr, o);
      std::string r = tr.report;
      size_t at = r.find("triplets: 6 beat(s) written as eighth triplets (3\n");
      assert(at != std::string::npos && r.find("rhythm:") == std::string::npos);
      printf("transcribe (triplets): %s", r.substr(at, r.find('\n', at) + 1 - at).c_str());
      assert(written(tr.song) == written(trip));
      std::string abc = toAbc(tr.song);  // written lengths in the group: (3E2 F2 G2
      assert(std::regex_search(abc, std::regex(R"(\(3=?[A-Ga-g]2 =?[A-Ga-g]2 =?[A-Ga-g]2 )")));
      assert(written(importAbc(abc).song) == written(tr.song));

      // the same straight eighths played long-short
      Song even = importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:D\nD E F G A B c d|e d c B A G F E|D F A d c A F D|E G B d e4|]\n").song;
      Song swung = even;
      for (size_t i = 0, tick = 0; i + 1 < swung.events.size(); tick += swung.events[i++].len)
        if (swung.events[i].len == 24 && tick % 48 == 0 && swung.events[i + 1].len == 24) swung.events[i].len = 32, swung.events[i + 1].len = 16;
      tr = transcribe(render(swung, 100), sr, o);
      r = tr.report;
      at = r.find("rhythm: swung eighths, about ");
      assert(at != std::string::npos);
      printf("transcribe (swing): %s", r.substr(at, r.find('\n', at) + 1 - at).c_str());
      double ratio = std::atof(r.c_str() + at + 29);
      assert(ratio > 1.7 && ratio < 2.3 && written(tr.song) == written(even) && r.find("triplets:") == std::string::npos);
    }
    {  // a ritardando and accelerando (80 -> 50 -> 80 bpm): with rubato the note values come back, without it they
       // don't (#11)
      Song t = importAbc("X:1\nT:r\nM:4/4\nL:1/8\nK:D\nD2 F2 A2 d2|c2 A2 B2 G2|F A d c B2 A2|G2 E2 F4|"
                         "A2 d2 f2 e2|d c B A G2 F2|E2 A2 c2 e2|d8|f2 e2 d2 c2|B A G F E2 D2|"
                         "F2 A2 d2 A2|B4 A4|G B d B A F D F|E2 G2 c2 e2|d2 c2 B2 c2|d8|]\n").song;
      Performance p = perform(t, 60, 0, false);  // a beat a second, then warped: tempo 65 - 15 cos(2 pi beat / beats)
      const double beats = p.length;
      auto at = [&](double b) {  // seconds at beat b
        double s = 0;
        for (int i = 0; i < 1000; i++) {
          double u = b * (i + 0.5) / 1000;
          s += b / 1000 * 60 / (65 + 15 * std::cos(2 * M_PI * u / beats));
        }
        return s;
      };
      for (auto& v : p.voices) {
        double end = at(v.start + v.dur);
        v.gap = end - at(v.start + v.dur - v.gap);
        v.start = at(v.start);
        v.dur = end - v.start;
      }
      p.length = at(beats);
      std::vector<float> x((size_t)((p.length + 1) * sr), 0.f);
      Synth sy;
      sy.sampleRate = sr;
      sy.metronome = false;
      long long pos = 0;
      sy.render(p, pos, x.data(), (int)x.size());
      auto lengths = [](const Song& s) {  // (pitch, length) of each played note, tied pieces merged
        std::vector<std::pair<int, int>> out;
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind != Kind::Note) continue;
          if (tied) out.back().second += e.len;
          else out.push_back({e.midi, e.len});
          tied = e.tie;
        }
        return out;
      };
      auto want = lengths(t);
      auto same = [&](const Song& s) {  // notes with the right pitch and length, in order (longest common subsequence)
        auto got = lengths(s);
        std::vector<std::vector<int>> L(want.size() + 1, std::vector<int>(got.size() + 1, 0));
        for (size_t i = 1; i <= want.size(); i++)
          for (size_t j = 1; j <= got.size(); j++)
            L[i][j] = want[i - 1] == got[j - 1] ? L[i - 1][j - 1] + 1 : std::max(L[i - 1][j], L[i][j - 1]);
        return L[want.size()][got.size()];
      };
      TranscribeOptions o;
      o.meter = "4/4";
      Transcription steady = transcribe(x, sr, o);
      o.rubato = true;
      Transcription rub = transcribe(x, sr, o);
      std::string r = rub.report.substr(0, rub.report.find('\n') + 1);
      printf("transcribe (rubato): %d of %zu notes right, %d without rubato; %s", same(rub.song), want.size(), same(steady.song),
             r.c_str());
      assert(lengths(rub.song) == want && same(steady.song) < (int)want.size() * 3 / 4);
      assert(steady.report.find("confidence low") != std::string::npos && steady.report.find("re-run with rubato: true\n") != std::string::npos);
      int med = 0, lo = 0, hi = 0;  // the tempo line: the median and the range of the beats
      assert(sscanf(r.c_str(), "tempo: %d bpm (quarter notes, the median); follows the recording: %d-%d bpm (rubato)\n", &med, &lo, &hi) == 3);
      assert(med >= 60 && med <= 70 && lo >= 45 && lo <= 55 && hi >= 75 && hi <= 85);
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
    {  // the pitch search reaches E7: a violin line up to D7/E7 and the whistle's top C#7 come back at pitch (#40)
      const std::pair<const char*, const char*> tunes[] = {
          {"X:1\nT:v\nM:4/4\nL:1/8\nK:D\nd'2 f'2 a'2 d''2|e''2 d''2 c''2 b'2|a'2 b'2 c''2 d''2|e''4 d''4|]\n", "violin"},
          {"X:1\nT:w\nM:4/4\nL:1/8\nK:D\nD4 A4|d2 f2 a2 b2|c'2 b2 a2 f2|g2 a2 b2 c'2|b4 a4|]\n", "whistle"}};
      for (auto [abc, inst] : tunes) {
        Song hi = importAbc(abc).song;
        hi.instrument = inst;
        Performance ph = perform(hi, 100, 0, false);
        std::vector<float> x((size_t)((ph.length + 1) * sr), 0.f);
        Synth sh;
        sh.sampleRate = sr;
        sh.metronome = false;
        long long at = 0;
        sh.render(ph, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = 100;
        o.instrument = inst;
        assert(melodyOf(transcribe(x, sr, o).song) == melodyOf(hi));
      }
    }
    {  // ornaments the synth plays (cuts and taps between repeated notes, long rolls) come back as techniques on the
       // main notes, with no grace notes left in the melody; only a whistle draft marks them, the report lists them (#12)
      Song orn = importAbc("X:1\nT:o\nM:4/4\nL:1/8\nK:D\nd2 !cut!d2 B2 !tap!B2|~A3 B A2 F2|G2 !cut!G2 !tap!G2 F2|~D3 E F4|]\n").song;
      auto notes = [](const Song& s) {
        std::vector<std::tuple<int, int, std::vector<std::string>>> out;  // pitch, length, techniques; ties merged
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind != Kind::Note) continue;
          if (tied) std::get<1>(out.back()) += e.len;
          else out.push_back({e.midi, e.len, e.techniques});
          tied = e.tie;
        }
        return out;
      };
      const char* where = "2 cuts (bar 1 beat 2, bar 3 beat 2); 2 taps (bar 1 beat 4, bar 3 beat 3); 2 rolls (bar 2 beat 1, bar 4 beat 1)";
      for (double bpm : {100.0, 120.0}) {
        Performance po = perform(orn, bpm, 0, false);
        std::vector<float> x((size_t)((po.length + 1) * sr), 0.f);
        Synth so;
        so.sampleRate = sr;
        so.metronome = false;
        long long at = 0;
        so.render(po, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = bpm;
        o.meter = "4/4";
        Transcription tr = transcribe(x, sr, o);
        assert(notes(tr.song) == notes(orn));
        assert(tr.report.find("\nornaments (candidates, check by ear): " + std::string(where) + "; marked in the ABC\n") != std::string::npos);
        assert(notes(importAbc(toAbc(tr.song)).song) == notes(orn));  // the draft ABC carries them to import_abc
        // a roll right after a cut on the same pitch, not re-articulated: split where the roll starts
        Song cutRoll = importAbc("X:1\nT:c\nM:4/4\nL:1/8\nK:D\nd2 !cut!d2 ~d3 B|A2 !cut!A2 ~A3 F|]\n").song;
        Performance pc = perform(cutRoll, bpm, 0, false);
        std::vector<float> y((size_t)((pc.length + 1) * sr), 0.f);
        Synth sc;
        sc.sampleRate = sr;
        sc.metronome = false;
        at = 0;
        sc.render(pc, at, y.data(), (int)y.size());
        assert(notes(transcribe(y, sr, o).song) == notes(cutRoll));
        if (bpm != 100) continue;
        o.instrument = "violin";
        tr = transcribe(x, sr, o);
        for (auto& e : tr.song.events) assert(e.techniques.empty());
        assert(tr.report.find(std::string(where) + "; not marked: the ABC is for the Violin\n") != std::string::npos);
      }
    }
    {  // piano notes fade under the voicing threshold before the next one: sustain them, no note + rest pairs (#6);
       // a melody with real rests keeps them
      auto played = [](const Song& s) {
        std::vector<std::tuple<Kind, int, int>> out;  // notes and rests (kind, pitch, length); ties and rests merged
        bool tied = false;
        for (auto& e : s.events) {
          if (e.kind == Kind::Bar) continue;
          if (!out.empty() && (tied || (e.kind == Kind::Rest && std::get<0>(out.back()) == Kind::Rest))) std::get<2>(out.back()) += e.len;
          else out.push_back({e.kind, e.midi, e.len});
          tied = e.kind == Kind::Note && e.tie;
        }
        if (std::get<0>(out.back()) == Kind::Rest) out.pop_back();  // the draft fills its last bar with a rest
        return out;
      };
      const std::pair<const char*, double> tunes[] = {{"X:1\nT:p\nM:4/4\nL:1/4\nK:G\nd2 B2|G4|A2 B c|d4|]\n", 40},
                                                      {"X:1\nT:r\nM:4/4\nL:1/8\nK:G\nd2 z2 B2 z2|G2 B2 d2 z2|g4 z4|e2 z2 c2 z2|]\n", 80}};
      for (auto [abc, bpm] : tunes) {
        Song t = importAbc(abc).song;
        Performance pp = perform(t, bpm, 0, false, "piano");
        std::vector<float> x((size_t)((pp.length + 1) * sr), 0.f);
        Synth sp;
        sp.sampleRate = sr;
        sp.metronome = false;
        long long at = 0;
        sp.render(pp, at, x.data(), (int)x.size());
        TranscribeOptions o;
        o.bpm = bpm;
        assert(played(transcribe(x, sr, o).song) == played(t));
      }
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
      assert(tr.report.find("pitch search: limited to the D whistle's sounding range, C5..D#7\n") != std::string::npos);
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
      assert(tr.song.meter == "4/4" && tr.report.find("\nmeter: 4/4 (detected; 4/4 ") != std::string::npos);
      assert(tr.report.find("triplets:") == std::string::npos && tr.report.find("rhythm:") == std::string::npos);  // straight (#2)
      // the per-frame pitch track comes back too, and its beat times put it under the right notes
      assert(tr.track.points.size() > mix.size() / 300 && tr.track.sync.size() > 20);
      for (auto& [tick, sec] : tr.track.sync) assert(std::abs(trackTick(tr.track, sec) - tick) < 1e-6);
      printf("transcribe (bpm %s): pitch track %zu frames, %.0f%% on its notes\n", bpm > 0 ? "given" : "auto", tr.track.points.size(),
             aligned(tr.song, tr.track) * 100);
      assert(aligned(tr.song, tr.track) >= 0.85);
      // the ABC we hand the agent reads back to the same song
      Song back = importAbc(toAbc(tr.song)).song;
      assert(back.events.size() == tr.song.events.size());
      for (size_t i = 0; i < back.events.size(); i++)
        assert(back.events[i].midi == tr.song.events[i].midi && back.events[i].len == tr.song.events[i].len &&
               back.events[i].tie == tr.song.events[i].tie);
    }
    {  // Import audio: a WAV through the same core function as the viewer gives a new song and its pitch track
      fs::create_directories(home);
      fs::path wav = home / "my tune.wav";
      writeWav(wav.string(), mix, sr);
      Song s = importAudio(wav.string(), 0, 0, {});
      auto saved = loadSong(s.id);
      assert(saved && saved->id == "my-tune" && saved->title == "my tune" && saved->source == "my tune.wav" && saved->folder.empty());
      assert(saved->notes.rfind("Automatic transcription\ntempo: ", 0) == 0 && saved->notes.find("\nmeter: 4/4") != std::string::npos &&
             saved->notes.find("\nkey: G major") != std::string::npos);
      assert(common(want, melodyOf(*saved)) >= (int)want.size() * 9 / 10);
      // notes the recording is unclear about: only the tune's rest in bar 4 (beats 3-4), which the transcription fills
      // with the accompaniment's G, and in the second import below also the faint bars 7 and 8
      auto unclearBars = [](const Song& x, int firstBar) {
        std::set<std::pair<int, bool>> r;  // (bar of the tune, in its second half)
        auto p = positions(x);
        for (size_t i = 0; i < x.events.size(); i++)
          if (x.events[i].comment == "unclear in the recording, check by ear") r.insert({p[i].bar + firstBar, p[i].tick >= 96});
        return r;
      };
      auto t = loadTrack(s.id);
      assert(t && t->points.size() > mix.size() / 300 && t->sync.size() > 20);
      printf("import audio: '%s', %zu notes, pitch track %.0f%% on its notes\n", s.id.c_str(), melodyOf(*saved).size(), aligned(*saved, *t) * 100);
      assert(aligned(*saved, *t) >= 0.85);
      for (auto [bar, late] : unclearBars(*saved, 0)) assert(bar == 4 && late);
      // a second import gets its own id; start/end trim the recording (here: the first two bars, 4.8 s, are left out).
      // The last two bars are made faint: their notes get the "check by ear" comment.
      std::vector<float> faint = mix;
      for (size_t i = (size_t)(6 * 2.4 * sr); i < faint.size(); i++) faint[i] *= 0.3f;
      writeWav(wav.string(), faint, sr);
      Song s2 = importAudio(wav.string(), 4.8, 0, {});
      assert(s2.id == "my-tune-2" && loadTrack("my-tune-2") && melodyOf(s2).size() < melodyOf(*saved).size());
      auto u2 = unclearBars(s2, 2);
      for (auto [bar, late] : u2) assert(bar >= 7 || (bar == 4 && late));
      assert(u2.count({7, false}) && u2.count({7, true}) && u2.count({8, false}));
      bool threw = false;
      try { importAudio((home / "missing.wav").string(), 0, 0, {}); } catch (const std::runtime_error&) { threw = true; }
      assert(threw && !loadTrack("missing") && !loadSong("missing"));
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / "my-tune.json");
      fs::remove(home / "songs" / "my-tune-2.json");
    }
    {  // separate (#5): Demucs runs on the section and its stem is transcribed; without it the full mix, and the report says why
      fs::create_directories(home);
      fs::path wav = home / "dense mix.wav";
      writeWav(wav.string(), mix, sr);
      TranscribeOptions plain;
      plain.bpm = 100;
      Transcription ref = transcribeFile(wav.string(), 0, 0, plain);
      TranscribeOptions o = plain;
      o.separate = "vocals";
      auto sameAsMix = [&](const Transcription& t, const std::string& line) {
        assert(t.report == "separation: " + line + "; transcribed the full mix\n" + ref.report);
        assert(toAbc(t.song) == toAbc(ref.song));
      };
      std::string path = getenv("PATH") ? getenv("PATH") : "";
      setenv("PATH", home.string().c_str(), 1);  // no demucs here, even where one is installed
      unsetenv("JOHN_DEMUCS");
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs not found (pip install demucs, or set JOHN_DEMUCS)");
      // Import audio (#69) copies the separation line into the song's notes, so the viewer shows why the full mix was used
      Song imp = importAudio(wav.string(), 0, 0, o);
      assert(imp.notes.rfind("Automatic transcription\nseparation: demucs not found (pip install demucs, or set JOHN_DEMUCS); "
                             "transcribed the full mix\ntempo: ", 0) == 0);
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / (imp.id + ".json"));
      setenv("PATH", path.c_str(), 1);
      std::string missing = (home / "no demucs").string();
      setenv("JOHN_DEMUCS", missing.c_str(), 1);
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "JOHN_DEMUCS (" + missing + ") can't be run");
#ifndef _WIN32  // the stub is a shell script
      // a stub in a folder with a space, that writes another tune where Demucs puts the stem
      Song other = importAbc("X:1\nT:b\nM:4/4\nL:1/8\nK:D\nA2 F2 D2 F2|A2 d2 c2 B2|A2 F2 G2 E2|D8|]\n").song;
      Performance po = perform(other, 100, 0, false);
      std::vector<float> stem((size_t)((po.length + 1) * sr), 0.f);
      Synth so;
      so.sampleRate = sr;
      so.metronome = false;
      long long at = 0;
      so.render(po, at, stem.data(), (int)stem.size());
      fs::path dir = home / "demucs stub";
      fs::create_directories(dir);
      writeWav((dir / "stem.wav").string(), stem, sr);
      fs::path stub = dir / "demucs";
      {
        std::ofstream f(stub);
        f << "#!/bin/sh\n"
             "echo 'stub noise on stdout'; echo 'stub noise on stderr' >&2\n"
             "[ -n \"$STUB_EXIT\" ] && exit \"$STUB_EXIT\"\n"
             "for a; do case $a in --two-stems=*) stem=${a#--two-stems=};; esac; done\n"
             "here=$(dirname \"$0\"); echo \"$4\" > \"$here/input\"\n"
             "[ -n \"$STUB_NOSTEM\" ] && exit 0\n"
             "mkdir -p \"$3/htdemucs/$(basename \"$4\" .wav)\"\n"
             "cp \"$here/stem.wav\" \"$3/htdemucs/$(basename \"$4\" .wav)/$stem.wav\"\n";
      }
      fs::permissions(stub, fs::perms::owner_all, fs::perm_options::add);
      setenv("JOHN_DEMUCS", stub.string().c_str(), 1);
      Transcription t = transcribeFile(wav.string(), 0, 0, o);
      assert(t.report.rfind("separation: transcribed the demucs vocals stem\ntempo: ", 0) == 0);
      Song imp2 = importAudio(wav.string(), 0, 0, o);
      assert(imp2.notes.rfind("Automatic transcription\nseparation: transcribed the demucs vocals stem\ntempo: ", 0) == 0);
      fs::remove_all(home / "tracks");
      fs::remove(home / "songs" / (imp2.id + ".json"));
      int fromStem = common(melodyOf(other), melodyOf(t.song)), fromMix = common(want, melodyOf(t.song));
      printf("separate (stub): %d/%zu notes of the stem, %d/%zu of the mix\n", fromStem, melodyOf(other).size(), fromMix, want.size());
      assert(fromStem >= (int)melodyOf(other).size() * 9 / 10 && fromMix < (int)want.size() / 2);
      std::string input;
      std::getline(std::ifstream(dir / "input"), input);
      assert(fs::path(input).filename() == "section.wav" && !fs::exists(fs::path(input).parent_path()));  // temp files removed
      setenv("STUB_NOSTEM", "1", 1);  // real Demucs exits 0 when it can't read a track
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs wrote no vocals.wav (stub noise on stderr)");
      unsetenv("STUB_NOSTEM");
      setenv("STUB_EXIT", "3", 1);
      sameAsMix(transcribeFile(wav.string(), 0, 0, o), "demucs failed with exit status 3 (stub noise on stderr)");
      unsetenv("STUB_EXIT");
#endif
      unsetenv("JOHN_DEMUCS");
      fs::remove(wav);
    }
}

  // decoding: a 16-bit WAV of A5 written by hand, read back resampled to 22050 Hz mono
TEST(decoding) {
    fs::create_directories(home);
    fs::path wav = home / "a5.wav";
    const int rate = 44100, n = rate / 2;
    std::vector<float> a5(n);
    for (int i = 0; i < n; i++) a5[i] = (float)(12000 / 32767.0 * std::sin(2 * M_PI * 880 * i / rate));
    writeWav(wav.string(), a5, rate);
    auto x = decodeAudio(wav.string(), 22050);
    assert(std::abs((int)x.size() - 22050 / 2) < 100);
    assert(std::abs(detectPitch(x.data() + 2000, 2048, 22050) - 880) < 5);
    // a non-ASCII path arrives as UTF-8 (JSON); Windows reads it as UTF-8 only with src/utf8.manifest
    fs::copy_file(wav, home / fs::path(u8"l\u00e1.wav"));
    assert(decodeAudio(home.string() + "/l\xc3\xa1.wav", 22050).size() == x.size());
    bool threw = false;
    try { decodeAudio((home / "missing.mp3").string(), 22050); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);
}
