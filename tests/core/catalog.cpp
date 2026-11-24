// Core tests: instruments (built-in and custom profiles), technique marks per instrument, custom techniques.
#include "test.hpp"

  // instruments: ranges, written vs sounding octave, JSON, playback pitch, range hints
TEST(instruments) {
    for (auto& id : instrumentIds()) assert(findInstrument(id));
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

    // harmonica keys (#67): the C harp is unchanged; G..B harps sit below C, Db..F# above
    Instrument ch = *findInstrument("harmonica");
    assert(ch.name == "Harmonica" && ch.low == 60 && ch.high == 96 && ch.octave == 0);
    assert(findInstrument("harmonica", "D", "C")->name == "Harmonica");
    Instrument ah = *findInstrument("harmonica", "D", "A");
    assert(ah.name == "A harmonica" && ah.low == 57 && ah.high == 93 && ah.octave == 0);  // starts on A3
    assert(findInstrument("harmonica", "D", "G")->low == 55 && findInstrument("harmonica", "D", "B")->low == 59);
    assert(findInstrument("harmonica", "D", "Db")->low == 61 && findInstrument("harmonica", "D", "F#")->low == 66);
    for (auto& k : HARP_KEYS) assert(findInstrument("harmonica", "D", k));
    assert(!findInstrument("harmonica", "D", "H") && !findInstrument("harmonica", "D", "C#"));
    assert(findInstrument("violin", "D", "A")->name == "Violin");  // the harp key only matters for the harmonica
    Song hs = v;
    hs.instrument = "harmonica";
    assert(!toJson(hs).contains("harp") && fromJson(toJson(hs)).harp == "C");  // C harp songs are saved as before
    hs.harp = "A";
    assert(toJson(hs)["harp"] == "A" && instrumentOf(fromJson(toJson(hs))).name == "A harmonica");
    hs.harp = "H";
    assert(instrumentOf(hs).name == "D whistle");  // unknown key: falls back like a bad whistle key
}

  // instruments as data (#85): the catalog from instruments/, the same results as the old hard-coded instruments, the
  // harmonica's real layout, custom instruments (save, override, broken files, deleted ones), instrument_name
TEST(instrument_profiles) {
    assert((instrumentIds() == std::vector<std::string>{"whistle", "violin", "piano", "guitar", "electric-guitar", "harmonica",
                                                        "voice-soprano", "voice-alto", "voice-tenor", "voice-bass"}));  // voices together (#87)
    assert(instrumentWarnings().empty());
    for (auto& in : instruments())
      assert(in.builtin && !in.custom && in.sound == (in.voice ? "violin" : in.id) && in.hints == (in.id == "whistle") && in.voice == !in.id.rfind("voice-", 0));
    assert(parseNote("D4") == 62 && parseNote("C#6") == 85 && parseNote("Bb3") == 58 && parseNote("A0") == 21 && parseNote("Cb4") == 59);
    assert(parseNote("H4") < 0 && parseNote("D") < 0 && parseNote("D4x") < 0 && parseNote("d4") < 0 && parseNote("G9") == 127 && parseNote("A9") < 0);

    // The old code, verbatim in behaviour: findInstrument() and rangeCost() before #85.
    auto oldInstrument = [](const std::string& id, const std::string& wk, const std::string& hk) -> std::optional<Instrument> {
      Instrument r;
      r.id = id;
      if (id == "whistle") {
        int b = whistleBase(wk);
        if (b < 0) return std::nullopt;
        r.name = wk + " whistle", r.low = b, r.high = b + 23, r.octave = 1, r.key = wk;
        return r;
      }
      if (id == "harmonica" && hk != "C") {
        if (std::find(HARP_KEYS.begin(), HARP_KEYS.end(), hk) == HARP_KEYS.end()) return std::nullopt;
        int b = whistleBase(hk);
        if (b >= 67) b -= 12;
        r.name = hk + " harmonica", r.low = b, r.high = b + 36;
        return r;
      }
      static const std::tuple<const char*, const char*, int, int, int> FIXED[] = {
          {"violin", "Violin", 55, 100, 0}, {"piano", "Piano", 21, 108, 0}, {"guitar", "Guitar", 52, 96, -1},
          {"electric-guitar", "Electric guitar", 52, 100, -1}, {"harmonica", "Harmonica", 60, 96, 0}};
      for (auto& [i, n, lo, hi, o] : FIXED)
        if (id == i) { r.name = n, r.low = lo, r.high = hi, r.octave = o; return r; }
      return std::nullopt;
    };
    auto oldCost = [](const Song& s, const Instrument& in, int semis) {
      RangeCost c;
      for (auto& e : s.events) {
        if (e.kind != Kind::Note) continue;
        if (in.id != "whistle") { c.out += !in.plays(e.midi + semis); continue; }
        Fingering f = finger(in.key, e.midi + semis);
        if (!f.ok) c.out++;
        else if (f.holes.find('H') != std::string::npos) c.half++;
        else if (f.holes == "OXXOOO") c.cross++;
      }
      return std::tuple(c.out, c.half, c.cross, c.half * 10 + c.cross * 2);
    };
    auto newCost = [](const Song& s, const Instrument& in, int semis) {
      RangeCost c = rangeCost(s, in, semis);
      return std::tuple(c.out, c.half, c.cross, c.hard());
    };
    std::vector<std::string> keys = {"", "H", "Dm", "D#", "E#", "Fb", "B#", "G#", "A#", "C##"};
    for (const char* l : {"C", "D", "E", "F", "G", "A", "B"})
      for (const char* acc : {"", "#", "b"}) keys.push_back(std::string(l) + acc), keys.push_back(char(std::tolower(*l)) + std::string(acc));
    keys.push_back("DB"), keys.push_back("bB");
    auto spelled = [](std::string k) {  // keys match case-insensitively (#85): the old code got the spelling the list has
      for (size_t i = 0; i < k.size(); i++) k[i] = char(i ? std::tolower((unsigned char)k[i]) : std::toupper((unsigned char)k[i]));
      return k;
    };
    Song sweep, tune = importAbc("X:1\nT:t\nM:4/4\nL:1/8\nK:D\nDEFG ABcd|efga bc'd'e'|=c=F^G_B c'=f'g2|\n").song;
    for (int m = 30; m <= 115; m++) { Event e; e.midi = m; e.len = WHOLE / 8; sweep.events.push_back(e); }
    for (auto& id : instrumentIds())
      for (auto& k : keys) {
        if (!id.rfind("voice-", 0)) continue;  // the voices came after #85 (#87)
        auto was = oldInstrument(id, spelled(k), spelled(k)), now = findInstrument(id, k, k);
        assert(was.has_value() == now.has_value());
        if (!was) continue;
        assert(was->name == now->name && was->low == now->low && was->high == now->high && was->octave == now->octave);
        if (id == "whistle") assert(was->key == now->key);
        if (id == "harmonica") continue;  // the harmonica gets its real layout below (#85)
        for (int m = 30; m <= 115; m++) {  // every pitch on its own
          Song one;
          one.events = {sweep.events[m - 30]};
          assert(oldCost(one, *was, 0) == newCost(one, *now, 0));
        }
        for (int t = -14; t <= 14; t++) assert(oldCost(tune, *was, t) == newCost(tune, *now, t) && oldCost(sweep, *was, t) == newCost(sweep, *now, t));
      }
    assert(findInstrument("whistle", "c")->low == 60 && findInstrument("whistle", "c")->name == "C whistle" && findInstrument("whistle", "bb")->key == "Bb");
    assert(findInstrument("harmonica", "D", "a")->low == 57 && !findInstrument("harmonica", "D", "c#"));  // only the listed harps
    // songs saved before #85 load and save back unchanged (no instrument_name for built-ins)
    for (const fs::path& d : {lessonsDir(), lessonsDir().parent_path() / "demo"})
      for (auto& e : fs::directory_iterator(d)) {
        std::ifstream f(e.path());
        json j = json::parse(f);
        json back = toJson(fromJson(j));
        if (!j.contains("instrument")) back.erase("instrument");  // the lessons leave out the default
        assert(back == j);
      }

    // the harmonica (10-hole Richter): blow and draw notes easy, bends hard, overblows and overdraws hard x3
    const std::set<int> easy = {0, 2, 4, 7, 11, 12, 14, 16, 17, 19, 21, 23, 24, 26, 28, 29, 31, 33, 36};
    const std::set<int> bends = {1, 5, 6, 8, 9, 10, 13, 20, 27, 30, 34, 35}, overblows = {3, 15, 18, 22, 25, 32};
    for (const char* hk : {"C", "A"}) {
      Instrument h = *findInstrument("harmonica", "D", hk);
      assert(h.high - h.low == 36);
      for (int at = 0; at <= 36; at++) {
        NoteLevel n = h.level(h.low + at);
        if (easy.count(at)) assert(n.level == NoteLevel::Easy);
        if (bends.count(at)) assert(n.level == NoteLevel::Hard && n.how == "bend" && n.weight == 1);
        if (overblows.count(at)) assert(n.level == NoteLevel::Hard && n.how == "overblow" && n.weight == 3);
        assert(h.plays(h.low + at) && easy.count(at) + bends.count(at) + overblows.count(at) == 1);
      }
    }
    Instrument ch = *findInstrument("harmonica");
    assert(ch.level(60).level == NoteLevel::Easy && ch.level(61).how == "bend" && ch.level(63).how == "overblow");  // C4, Db4, Eb4
    assert(ch.level(69).how == "bend" && ch.level(81).level == NoteLevel::Easy && ch.level(87).how == "bend");      // A4 3-draw, A5, Eb6
    Instrument ah = *findInstrument("harmonica", "D", "A");
    assert(ah.low == 57 && ah.level(62).how == "bend" && ah.level(60).how == "overblow" && ah.level(64).level == NoteLevel::Easy);
    // a scale in the bottom octave needs bends (F, A on a C harp; D, F# on an A harp); the octave up is all blow/draw
    auto p = playability(importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:C\nCDEF GABc|\n").song, ch);
    assert(p.verdict == Playability::OtherKey && p.shift == 12 && p.cost.other == 0 && p.cost.out == 0);
    RangeCost low = rangeCost(importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:C\nCDEF GABc|\n").song, ch, 0);
    assert(low.other == 2 && low.hard() == 20 && low.half == 0);
    p = playability(importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:A\nA,B,CD EFGA|\n").song, ah);
    assert(p.verdict == Playability::OtherKey && p.shift == 12);
    assert(analyze([&] { Song x = importAbc("X:1\nT:s\nM:4/4\nL:1/8\nK:C\nCDEF GABc|\n").song; x.instrument = "harmonica"; return x; }())
               .find("current: 0 out of range, 2 hard") != std::string::npos);

    // a custom instrument: "a C flute, low D to high A", diatonic
    fs::path dir = home / "instruments";
    Instrument fl = parseInstrument({{"id", "c-flute"}, {"name", "C flute"}, {"low", "D4"}, {"high", "A6"}, {"key", "C"},
                                     {"techniques", {"cut", "roll"}}, {"sound", "whistle"}});
    applyPreset(fl, true);
    assert(fl.low == 62 && fl.high == 93 && fl.level(66).level == NoteLevel::Hard && fl.level(67).level == NoteLevel::Easy);  // F#4, G4
    assert(fl.notes.size() == 13 && !fl.custom);  // 5 of every 12 in D4..A6
    saveInstrument(fl);
    assert(fs::exists(dir / "c-flute.json") && !toJson(fl).contains("id"));
    Instrument got = *findInstrument("c-flute");
    assert(got.custom && !got.builtin && got.name == "C flute" && got.notes == fl.notes && got.techniques == fl.techniques);
    assert(instrumentIds().back() == "c-flute");  // custom ones after the built-ins
    p = playability(importAbc("X:1\nT:f\nM:4/4\nL:1/8\nK:C\nDEFG ABcd|\n").song, got);
    assert(p.verdict == Playability::AsWritten);
    p = playability(importAbc("X:1\nT:f\nM:4/4\nL:1/8\nK:D\nDEFG ABcd|\n").song, got);  // F#, C#: in C it fits
    assert(p.verdict == Playability::OtherKey && keyName(p.key) == "C major");
    Instrument imp = got;
    imp.notes[0] = {NoteLevel::Impossible};
    assert(!imp.plays(62) && imp.plays(64) && rangeCost(importAbc("X:1\nT:f\nM:4/4\nL:1/8\nK:C\nDE|\n").song, imp, 0).out == 1);
    // songs save the name of a custom instrument, never of a built-in
    Song fs1 = importAbc("X:1\nT:f\nM:4/4\nL:1/8\nK:C\nDEFG|\n").song;
    assert(!toJson(fs1).contains("instrument_name"));
    fs1.instrument = "c-flute";
    assert(toJson(fs1)["instrument_name"] == "C flute" && fromJson(toJson(fs1)).instrumentName == "C flute");
    assert(instrumentOf(fs1).name == "C flute" && instrumentLabel(fs1) == "C flute");
    Settings mine;
    mine.instrument = "c-flute";
    mine.whistle = "C";
    saveSettings(mine);
    assert(loadSettings().instrument == "c-flute");
    // deleted: the song keeps working on the D whistle, named "(deleted)"; Settings fall back to the D whistle
    Song kept = fromJson(toJson(fs1));
    assert(deleteInstrument("c-flute") && !deleteInstrument("c-flute") && !deleteInstrument("violin") && !findInstrument("c-flute"));
    assert(instrumentOf(kept).name == "D whistle" && instrumentLabel(kept) == "C flute (deleted)" && listing(kept).find("instrument: C flute (deleted)") != std::string::npos);
    assert(toJson(kept)["instrument_name"] == "C flute");  // still saved, so it stays named
    assert(loadSettings().instrument == "whistle" && loadSettings().whistle == "D");  // deleteInstrument() reset it
    // a custom file that is only broken or missing for a while leaves the Settings alone
    saveInstrument(fl);
    saveSettings(mine);
    std::string saved;
    { std::ifstream f(home / "settings.json"); saved.assign(std::istreambuf_iterator<char>(f), {}); }
    std::ofstream(dir / "c-flute.json") << "{broken";
    reloadInstruments();
    assert(!findInstrument("c-flute") && loadSettings() == mine);
    fs::remove(dir / "c-flute.json");
    reloadInstruments();
    assert(loadSettings() == mine);
    { std::ifstream f(home / "settings.json"); assert(std::string(std::istreambuf_iterator<char>(f), {}) == saved); }
    saveInstrument(fl);
    assert(loadSettings() == mine && findInstrument("c-flute"));
    deleteInstrument("c-flute");
    fs::remove(home / "settings.json");
    // saveInstrument never writes a file the loader would skip (the editor shows the reason)
    Instrument bad = *findInstrument("whistle");
    bad.id = "my-whistle", bad.key = "H";
    bool threw = false;
    try { saveInstrument(bad); } catch (const std::runtime_error& e) { threw = std::string(e.what()).find("key must be") == 0; }
    assert(threw && !fs::exists(dir / "my-whistle.json"));
    bad.key = "G", bad.keys = {"C", "D"};  // a key its own list doesn't have
    threw = false;
    try { saveInstrument(bad); } catch (const std::runtime_error& e) { threw = std::string(e.what()) == "keys must include key"; }
    assert(threw && !fs::exists(dir / "my-whistle.json"));
    // a hard note's how and weight survive a trip through easy in the editor's map, and easy entries aren't saved
    Instrument cyc = *findInstrument("whistle");
    cyc.id = "cyc", cyc.notes[1].level = NoteLevel::Easy;
    assert(!toJson(cyc)["notes"].contains("1") && cyc.notes[1].how == "half-hole");

    // a custom file with a built-in's id overrides it, in the built-in's place; deleting it brings the built-in back
    std::ofstream(dir / "violin.json") << R"({"name": "My violin", "low": "C3", "high": "E7", "sound": "violin"})";
    reloadInstruments();
    Instrument mv = *findInstrument("violin");
    assert(mv.name == "My violin" && mv.low == 48 && mv.custom && mv.builtin && instrumentIds()[1] == "violin");
    Song vs = fs1;
    vs.instrument = "violin";
    assert(!toJson(vs).contains("instrument_name"));
    assert(deleteInstrument("violin") && findInstrument("violin")->name == "Violin" && !findInstrument("violin")->custom);

    // a broken file drops only that instrument, with a warning naming the file and the problem
    std::ofstream(dir / "kazoo.json") << "{not json";
    std::ofstream(dir / "flute.json") << R"({"name": "Flute", "low": "H4", "high": "A6"})";
    std::ofstream(dir / "lute.json") << R"({"name": "Lute", "low": "C3", "high": "C6", "notes": {"40": "hard"}})";
    std::ofstream(dir / "pipe.json") << R"({"name": "Pipe", "low": "D4", "high": "D6", "notes": {"1": {"level": "hard", "how": "lip"}}})";
    std::ofstream(dir / "harp2.json") << R"({"name": "Harp", "low": "C4", "high": "C7", "notes": {"1": "bend"}, "sound": "harp"})";
    std::ofstream(dir / "fife.json") << R"({"name": "Fife", "low": 74, "high": 98, "notes": {"1": "hard", "2": {"level": "impossible"}}})";
    reloadInstruments();
    auto warn = instrumentWarnings();
    auto warned = [&](const char* file, const char* what) {
      return std::any_of(warn.begin(), warn.end(), [&](auto& w) { return w.find(file) != std::string::npos && w.find(what) != std::string::npos; });
    };
    assert(warn.size() == 5 && warned("kazoo.json", "not valid JSON") && warned("flute.json", "low must be a note"));
    assert(warned("lute.json", "notes \"40\"") && warned("pipe.json", "how must be") && warned("harp2.json", "level must be"));
    assert(findInstrument("fife") && findInstrument("fife")->level(75).level == NoteLevel::Hard && !findInstrument("fife")->plays(76));
    assert(!findInstrument("kazoo") && !findInstrument("flute") && findInstrument("whistle") && instrumentIds().size() == 11);
    for (const char* f : {"kazoo", "flute", "lute", "pipe", "harp2", "fife"}) fs::remove(dir / (std::string(f) + ".json"));
    reloadInstruments();
    assert(instrumentWarnings().empty() && instrumentIds().size() == 10);
    // clear errors for each field (MCP define_instrument shows them)
    auto err = [](const json& j) {
      try { parseInstrument(j); } catch (const std::runtime_error& e) { return std::string(e.what()); }
      return std::string();
    };
    json ok = {{"id", "x"}, {"name", "X"}, {"low", 60}, {"high", 72}};
    assert(err(ok).empty());
    auto with = [&](const char* k, json v) { json j = ok; j[k] = v; return err(j); };
    assert(with("id", "Bad Id").find("id must be") == 0 && with("name", "").find("name is missing") == 0);
    for (const char* dev : {"con", "prn", "aux", "nul", "com1", "lpt9"}) assert(with("id", dev).find("id must be") == 0);
    assert(with("id", "com").empty() && with("id", "console").empty() && with("id", "com10").empty());
    assert(with("high", 50).find("low must not be above high") == 0 && with("octave", 3).find("octave must be") == 0);
    assert(with("key", "H").find("key must be") == 0 && with("sound", "harp").find("sound must be") == 0);
    assert(with("techniques", "cut").find("techniques must be") == 0 && with("notes", {{"x", "hard"}}).find("notes \"x\"") == 0);
    assert(with("notes", {{"3", {{"level", "hard"}, {"weight", 0}}}}).find("weight must be") != std::string::npos);
    assert(err({{"id", "x"}, {"name", "X"}, {"high", 72}}).find("low is missing") == 0);
}

  // marks per instrument (#86): my instrument shows the marks it lists, its equivalents (instead), breath always,
  // custom techniques always; the rest is hidden. The song never changes.
TEST(marks_per_instrument) {
    const Instrument wh = *findInstrument("whistle"), vn = *findInstrument("violin"), pn = *findInstrument("piano"),
                     hm = *findInstrument("harmonica");
    using V = std::vector<std::string>;
    const V all = {"cut", "tap", "double_cut", "roll", "short_roll", "cran", "slide", "tongue", "breath", "vibrato"};
    assert(marks(all, wh).shown == all && marks(all, wh).hidden.empty());
    assert((marks(all, pn).shown == V{"grace", "turn", "breath"}));  // duplicates merged; breath stays as a phrase mark
    assert((marks(all, pn).hidden == V{"slide", "tongue", "vibrato"}));
    assert((marks(all, hm).shown == V{"grace", "shake", "slide", "tongue", "breath", "vibrato"}));
    assert((marks(all, hm).hidden == V{"double_cut", "cran"}));
    assert((marks(all, vn).hidden == V{"tongue"}) && marks({"breath"}, vn).shown == V{"breath"});
    assert((marks({"roll", "bounce"}, pn).shown == V{"turn", "bounce"}));  // not built-in: no profile can list it
    // the profiles' equivalents: piano cut, tap, double_cut -> grace, rolls and crans -> turn; harmonica cut, tap -> grace, rolls -> shake
    assert((pn.instead == std::map<std::string, std::string>{{"cut", "grace"}, {"tap", "grace"}, {"double_cut", "grace"},
                                                           {"roll", "turn"}, {"short_roll", "turn"}, {"cran", "turn"}}));
    assert((hm.instead == std::map<std::string, std::string>{{"cut", "grace"}, {"tap", "grace"}, {"roll", "shake"}, {"short_roll", "shake"}}));
    for (const char* t : {"grace", "turn", "shake"}) assert(builtinTechnique(t));
    // hidden counts over the whole song, which keeps its marks
    Song tune = importAbc("X:1\nT:m\nM:4/4\nL:1/4\nK:D\n{g}A !tongue!B !slide!c !breath!d|~e2 !cran!d2|\n").song;
    const Song before = tune;
    assert(hiddenMarks(tune, wh) == 0 && hiddenMarks(tune, vn) == 1 && hiddenMarks(tune, pn) == 2 && hiddenMarks(tune, hm) == 1);
    assert(toJson(tune) == toJson(before));
    // analyze_song offers the candidates as my instrument shows them; the whistle's stay as they were
    const std::string forPiano = analyze(tune, pn), forWhistle = analyze(tune);
    assert(forWhistle == analyze(tune, wh) && forWhistle.find(": long roll (long note on the beat)\n") != std::string::npos);
    assert(forWhistle.find("tongue (phrase start") != std::string::npos && forWhistle.find("doesn't breathe") == std::string::npos);
    assert(forPiano.find(": turn (the Piano's way to play: long roll (long note on the beat))\n") != std::string::npos);
    assert(forPiano.find("tongue (phrase start") == std::string::npos && forPiano.find("E5: long roll") == std::string::npos);
    assert(forPiano.find("(the Piano doesn't breathe: a breath mark shows as a phrase end, a short lift)") != std::string::npos);
    // instead: the target must be one the instrument lists; saving checks every id against the catalog
    json flute = {{"id", "fl"}, {"name", "Fl"}, {"low", 62}, {"high", 86}, {"techniques", {"grace", "slide"}}};
    auto err = [](const json& j) {
      try { parseInstrument(j); } catch (const std::runtime_error& e) { return std::string(e.what()); }
      return std::string();
    };
    json f = flute;
    f["instead"] = {{"cut", "grace"}};
    assert(err(f).empty() && parseInstrument(f).instead.at("cut") == "grace" && toJson(parseInstrument(f))["instead"]["cut"] == "grace");
    f["instead"] = {{"cut", "turn"}};
    assert(err(f).find("instead \"cut\": \"turn\" must be one of the instrument's techniques") == 0);
    f["instead"] = {{"cut", 3}};
    assert(err(f).find("instead \"cut\" must be a technique id") == 0);
    f["instead"] = json::array();
    assert(err(f).find("instead must be an object") == 0);
    Instrument in = parseInstrument(flute);
    auto saveErr = [&](const Instrument& x) {
      try { saveInstrument(x); } catch (const std::runtime_error& e) { return std::string(e.what()); }
      return std::string();
    };
    in.instead = {{"wobble", "grace"}};
    assert(saveErr(in) == "unknown technique 'wobble' (see list_techniques or define_technique)" && !findInstrument("fl"));
    in.instead = {{"cut", "grace"}, {"roll", "grace"}};
    assert(saveErr(in).empty() && findInstrument("fl")->instead == in.instead);
    assert((marks({"roll", "tap"}, *findInstrument("fl")).shown == V{"grace"}));
    deleteInstrument("fl");
    // an instead key the instrument lists itself would never apply: refused
    f["instead"] = {{"slide", "grace"}};
    assert(err(f).find("instead \"slide\": the instrument lists slide itself") == 0);
    // the fallback whistle (no whistle.json) lists what whistle.json lists: every built-in but the equivalents
    {
      std::set<std::string> builtins, listed(wh.techniques.begin(), wh.techniques.end());
      for (auto& t : defaultTechniques()) if (t.id != "grace" && t.id != "turn" && t.id != "shake") builtins.insert(t.id);
      assert(listed == builtins);
    }
    // a user's own technique with a built-in's id (a grace defined before #86) is custom: always shown
    assert(builtinTechnique("grace") && marks({"grace"}, wh).hidden == V{"grace"});
    saveCustomTechnique({"grace", "Grace", "g", "my own grace"});
    assert(!builtinTechnique("grace") && marks({"grace"}, wh).shown == V{"grace"});
    fs::remove(home / "techniques.json");
    assert(!builtinTechnique("grace"));  // the cache follows loadTechniques(), as the GUI's poll and each MCP call do
    loadTechniques();
    assert(builtinTechnique("grace") && marks({"grace"}, wh).hidden == V{"grace"});
    // playback follows the player's marks: a whistle tune on the piano plays grace notes and turns, on the harmonica a shake
    Song o = importAbc("X:1\nT:o\nM:4/4\nL:1/4\nK:D\n{g}A ~B2 !tongue!c|\n").song;
    auto onPiano = perform(o, 60, 0, false, "", pn).voices, onHarp = perform(o, 60, 0, false, "", hm).voices;
    assert((onPiano[0].graces == std::vector<std::pair<double, int>>{{0, 2}}));               // grace for the cut
    assert((onPiano[1].graces == std::vector<std::pair<double, int>>{{0, 2}, {0.07, -2}}));   // turn for the roll
    assert(onHarp[1].graces.size() > 4 && onHarp[1].graces[0].second == 3);                    // shake for the roll
    assert(perform(o, 60, 0, false, "", wh).voices[1].graces.size() == 2);                     // the whistle's own roll
    const int sr = 48000, n = 1024;
    auto render = [&](const Performance& p) {
      Synth syn;
      syn.metronome = false;
      syn.volume = 1;
      long long at = 0;
      std::vector<float> x((size_t)((p.length + 0.5) * sr), 0.f);
      syn.render(p, at, x.data(), (int)x.size());
      for (float v : x) assert(std::isfinite(v) && std::abs(v) < 1);
      return x;
    };
    auto pitch = [&](const std::vector<float>& x, double at, int len) {  // written MIDI heard at `at` seconds (octave 0)
      float hz = detectPitch(x.data() + (int)(at * sr), len, sr, 150, 2000);
      return hz > 0 ? (int)std::lround(writtenMidiFromFreq(hz, 0)) : -1;
    };
    // a piano note with a grace / turn, a harmonica note with a shake: the grace pitch, then the note
    Song g = importAbc("X:1\nT:g\nM:4/4\nL:1/4\nK:C\n!grace!A4|\n").song, t = g, k = g;
    t.events[0].techniques = {"turn"};
    k.events[0].techniques = {"shake"};
    std::vector<float> xg = render(perform(g, 60, 0, false, "piano", pn)), xt = render(perform(t, 60, 0, false, "piano", pn)),
                       xk = render(perform(k, 60, 0, false, "harmonica", hm));
    assert(pitch(xg, 0.008, 1000) == 71 && pitch(xg, 0.3, n) == 69);   // B4 grace, then A4
    assert(pitch(xt, 0.008, 1000) == 71 && pitch(xt, 0.078, 1000) == 67 && pitch(xt, 0.3, n) == 69);  // above, below, the note
    assert(pitch(xk, 0.46, 1500) == 72 && pitch(xk, 0.53, 1500) == 69 && pitch(xk, 0.6, 1500) == 72);  // C5 / A4 alternate
    // on a short note the grace notes shrink to a quarter of what sounds, so the main note is still heard (a sixteenth at
    // 120 bpm, 125 ms, minus the piano's 12 ms damp)
    Song sg = importAbc("X:1\nT:s\nM:4/4\nL:1/16\nK:C\n!grace!A !turn!A z2 z12|\n").song;
    Performance ps = perform(sg, 120, 0, false, "piano", pn);
    const double heard = 0.125 - 0.012, gl = heard / 4;
    assert(std::abs(ps.voices[0].graceLen - gl) < 1e-9 && std::abs(ps.voices[1].graceLen - gl) < 1e-9);
    assert(std::abs(ps.voices[1].graces[1].first - 2 * gl) < 1e-9);
    std::vector<float> xs = render(ps);
    auto shortPitch = [&](double at, int len) {
      float hz = detectPitch(xs.data() + (int)(at * sr), len, sr, 300, 2000);
      return hz > 0 ? (int)std::lround(writtenMidiFromFreq(hz, 0)) : -1;
    };
    assert(shortPitch(0.002, 1000) == 71 && shortPitch(gl + 0.005, 3000) == 69);  // grace, then the note for 3/4
    assert(shortPitch(0.125 + 2 * gl + 0.002, 1000) == 67 && shortPitch(0.125 + 3 * gl + 0.002, 900) == 69);  // turn: below, the note
}

TEST(custom_technique) {
  saveCustomTechnique({"bounce", "Bounce", "b", "test"});
  bool found = false;
  for (auto& x : loadTechniques()) found |= x.id == "bounce";
  assert(found);
}
