// Core tests: bundled songs (lessons/, demo/) and the settings file.
#include "test.hpp"

  // lessons: installed once, a deleted lesson stays deleted, and every bundled lesson loads
TEST(bundled_songs) {
    Song s = importAbc(JIG).song;
    fs::path src = home / "lesson-src";
    fs::create_directories(src);
    Song l = s;
    l.id = "lesson-test";
    { std::ofstream f(src / "lesson-test.json"); f << toJson(l).dump(); }
    assert(installSongs(src) == 1 && loadSong("lesson-test"));
    assert(installSongs(src) == 0);
    fs::remove(dataDir() / "songs" / "lesson-test.json");
    assert(installSongs(src) == 0 && !loadSong("lesson-test"));
    // each bundled folder has its own marker: a "demo" folder installs even though lesson-src was done
    fs::path demo = home / "demo";
    fs::create_directories(demo);
    l.id = "demo-test";
    { std::ofstream f(demo / "demo-test.json"); f << toJson(l).dump(); }
    assert(installSongs(demo) == 1 && loadSong("demo-test") && fs::exists(dataDir() / "demo-installed.txt"));
    fs::remove(dataDir() / "songs" / "demo-test.json");
    assert(installSongs(demo) == 0 && !loadSong("demo-test"));
    // smart update: a copy still as installed is replaced by a new bundled version; a copy the user changed is kept and
    // only gets lyrics added since, when its notes are the bundled ones up to a transposition; a deleted one stays gone
    {
      fs::path up = home / "up";
      fs::create_directories(up);
      const char* ids[] = {"up-same", "up-tech", "up-key", "up-rhythm", "up-own", "up-gone"};
      auto bundle = [&](const fs::path& dir, const std::vector<std::string>& which, const std::string& abc,
                        const std::string& notes) {
        for (auto& id : which) {
          Song b = importAbc(abc).song;
          b.id = id;
          b.notes = notes;
          std::ofstream f(dir / (id + ".json"));
          f << toJson(b).dump();
        }
      };
      auto read = [](const fs::path& p) {
        std::ifstream f(p);
        return std::string(std::istreambuf_iterator<char>(f), {});
      };
      const std::string v1 = "X:1\nT:t\nM:3/4\nL:1/4\nK:D\nD E F|G3|]\n", v2 = v1 + "w:Sing-ing a song\nW:More words.\n";
      bundle(up, {std::begin(ids), std::end(ids)}, v1, "old advice");
      assert(installSongs(up) == 6);
      Song tech = *loadSong("up-tech");
      tech.events[0].techniques = {"cut"};
      saveSong(tech);
      Song key = *loadSong("up-key");
      transposeToKey(key, 9, instrumentOf(key));  // "Play in key": D major to A major
      assert(keyName(key.key) == "A major" && key.events[0].midi != 62);
      saveSong(key);
      Song rhythm = key;
      rhythm.id = "up-rhythm";
      std::swap(rhythm.events[0].len, rhythm.events[4].len);  // transposed and rhythm edited
      saveSong(rhythm);
      Song own = *loadSong("up-own");
      own.events[0].lyric = "mine";
      saveSong(own);
      fs::remove(dataDir() / "songs" / "up-gone.json");
      bundle(up, {std::begin(ids), std::end(ids)}, v2, "new advice");
      assert(installSongs(up) == 0);
      Song want = fromJson(json::parse(read(up / "up-same.json")));
      want.id = "up-same";
      assert(toJson(*loadSong("up-same")) == toJson(want) && loadSong("up-same")->events[0].lyricHyphen);  // replaced
      Song t = *loadSong("up-tech");  // not replaced (advice and cut kept), but the same notes get the lyrics
      assert(t.notes == "old advice" && t.events[0].techniques == tech.events[0].techniques && t.events[0].lyric == "Sing");
      Song k = *loadSong("up-key");  // keeps its key and notes, gains the lyrics and words
      assert(toJson(k) != toJson(key) && keyName(k.key) == "A major" && k.notes == "old advice" && k.words == "More words.");
      for (size_t i = 0; i < k.events.size(); i++) {
        assert(k.events[i].midi == key.events[i].midi && k.events[i].len == key.events[i].len);
        assert(k.events[i].lyric == want.events[i].lyric && k.events[i].lyricHyphen == want.events[i].lyricHyphen);
      }
      assert(toJson(*loadSong("up-rhythm")) == toJson(rhythm));
      assert(toJson(*loadSong("up-own")) == toJson(own));
      assert(!loadSong("up-gone"));
      // a marker line from before the hash: a copy equal to the current bundle (whitespace aside) gets the hash, any
      // other copy counts as changed (not replaced, still gets lyrics)
      fs::path old = home / "old";
      fs::create_directories(old);
      bundle(old, {"old-equal", "old-other"}, v2, "new advice");
      { std::ofstream f(dataDir() / "old-installed.txt"); f << "old-equal\nold-other\n"; }
      { std::ofstream f(dataDir() / "songs" / "old-equal.json"); f << json::parse(read(old / "old-equal.json")).dump(4); }
      bundle(dataDir() / "songs", {"old-other"}, v1, "old advice");
      std::string equalBefore = read(dataDir() / "songs" / "old-equal.json");
      assert(installSongs(old) == 0);
      std::string m = read(dataDir() / "old-installed.txt");
      assert(m.find("old-equal\t") != std::string::npos && m.find("old-other\n") != std::string::npos);
      assert(read(dataDir() / "songs" / "old-equal.json") == equalBefore);
      Song o = *loadSong("old-other");
      assert(o.notes == "old advice" && o.events[0].lyric == "Sing");
      // running it again changes nothing: same count, marker and files, not even rewritten
      for (auto dir : {up, old}) {
        auto state = [&] {  // the marker and every song file: content and modification time
          std::map<fs::path, std::pair<std::string, fs::file_time_type>> r;
          fs::path marker = dataDir() / (dir.filename().string() + "-installed.txt");
          r[marker] = {read(marker), fs::last_write_time(marker)};
          for (auto& e : fs::directory_iterator(dataDir() / "songs")) r[e.path()] = {read(e.path()), fs::last_write_time(e.path())};
          return r;
        };
        auto before = state();
        assert(installSongs(dir) == 0 && state() == before);
      }
    }
    // #47: the title, notes and every comment have a pt-BR translation (re-translate after editing the English)
    auto translated = [](const Song& b) {
      auto has = [&](const std::string& t) {
        auto l = b.i18n.find("pt-BR");
        bool ok = l != b.i18n.end() && l->second.count(t) && !l->second.at(t).empty();
        if (!ok) fprintf(stderr, "%s: no pt-BR for \"%s\"\n", b.id.c_str(), t.c_str());
        return ok;
      };
      bool ok = has(b.title) && has(b.notes);
      for (auto& ev : b.events) ok &= ev.comment.empty() || has(ev.comment);
      return ok;
    };
    int bundled = 0;
    for (auto& e : fs::directory_iterator(lessonsDir())) {
      std::ifstream f(e.path());
      Song b = fromJson(json::parse(f));
      assert(!b.events.empty() && !b.notes.empty() && translated(b));
      for (auto& ev : b.events) assert(ev.kind != Kind::Note || finger(b.whistle, ev.midi).ok);  // all playable
      assert(playability(b, instrumentOf(b)).verdict == Playability::AsWritten);  // listed under Playable on my D whistle (#84)
      bundled++;
    }
    assert(bundled >= 8);
    // release packages keep lessons/ and fonts/ next to the executable; dev builds use the build-time paths
#ifdef _WIN32
    assert(john::bundled("john-tests.exe", "/built") != "/built");  // this test binary sits next to itself
#else
    assert(john::bundled("john-tests", "/built") != "/built");  // this test binary sits next to itself
#endif
    assert(john::bundled("no-such-file", "/built") == "/built");
    assert(lessonsDir().filename() == "lessons");  // installSongs keeps using lessons-installed.txt
    // demo/ (web starter songs): every song loads, has notes, a source and a folder, and fits its instrument
    int demos = 0;
    for (auto& e : fs::directory_iterator(lessonsDir().parent_path() / "demo")) {
      std::ifstream f(e.path());
      Song b = fromJson(json::parse(f));
      assert(!b.events.empty() && !b.notes.empty() && !b.source.empty() && !b.folder.empty() && translated(b));
      assert(findInstrument(b.instrument, b.whistle));
      for (auto& ev : b.events) assert(ev.kind != Kind::Note || instrumentOf(b).plays(ev.midi));
      assert(playability(b, instrumentOf(b)).verdict == Playability::AsWritten);
      demos++;
    }
    assert(demos >= 12);
}

  // settings: no file -> defaults; round trip; missing keys -> defaults; garbage -> all defaults
TEST(settings_file) {
    auto write = [&](const char* text) { std::ofstream(home / "settings.json") << text; };
    assert(loadSettings() == Settings{} && Settings{}.lyrics);  // lyrics show by default
    Settings s;
    s.hints = false; s.zoom = 12; s.light = true; s.tempo = 90; s.countIn = false; s.metronome = false;
    s.volume = 0.3f; s.sound = "piano"; s.gate = 0.02f; s.latencyMs = 120; s.tolerance = 25; s.instrument = "violin"; s.whistle = "C"; s.allSongs = true;
    s.language = "pt-BR"; s.solfege = true; s.solfegeKeys = false; s.harp = "A"; s.allMarks = true; s.lyrics = false;
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
    write(R"({"sound": "banjo", "instrument": "kazoo", "songFilter": "lute", "zoom": 0, "volume": 9, "harp": "H", "whistle": "Dm"})");  // values the GUI can't use; songFilter is the old filter (#84), now ignored
    Settings c = loadSettings();
    // an unknown instrument stays (#85: a custom file may be missing for a while; the readers fall back to the D whistle)
    assert(c.sound.empty() && c.instrument == "kazoo" && c.whistle == "D" && !c.allSongs && c.zoom == 6 && c.volume == 1 && c.harp == "C");
    fs::remove(home / "settings.json");
}
