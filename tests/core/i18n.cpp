// Core tests: viewer language and the pt-BR strings.
#include "test.hpp"

  // viewer language: English until the GUI sets one (the MCP server never does), then pt-BR with fallback
TEST(viewer_language) {
    Song s = importAbc(JIG).song;
    Key d{2, "maj"};
    Event dq{Kind::Note, 62, WHOLE * 3 / 8}, trip{Kind::Note, 62, WHOLE / 12, false, 3, 2};
    assert(!strcmp(tr("Play"), "Play") && keyName({2, "maj"}) == "D major" && durationName(dq) == "dotted quarter");
    assert(defaultTechniques()[0].name == "Cut");
    // song translations (#47): s.i18n round-trips, English until a language is set, exact-English match only
    Song ts = s;
    ts.title = "Hello";
    ts.i18n["pt-BR"]["Hello"] = "Olá";
    assert(fromJson(toJson(ts)).i18n == ts.i18n && fromJson(toJson(s)).i18n.empty());
    assert(songText(ts, ts.title) == "Hello");
    assert(setLanguage("pt-BR"));
    assert(songText(ts, ts.title) == "Olá" && songText(ts, "Hello!") == "Hello!");  // edited English: shown as is
    assert(!strcmp(tr("Play"), "Tocar") && tr(std::string("Settings")) == "Configurações");
    const char* missing = "No such text";
    assert(tr(missing) == missing);  // fallback: the English itself
    assert(trf("%s (mine)", "Piano") == "Piano (meu instrumento)");
    assert(keyName({2, "maj"}) == "D maior" && keyName({1, "dor"}) == "A dórico" && keyName({3, "min"}) == "F# menor");
    assert(durationName(dq) == "semínima pontuada" && durationName(trip) == "colcheia (quiáltera de 3)");
    assert(durationName({Kind::Note, 62, WHOLE * 7 / 8}) == "mínima com dois pontos");
    assert(defaultTechniques()[0].name == "Corte (cut)");
    setSolfege(false, true);  // keys as Do Re Mi, note names letters
    assert(keyName({2, "maj"}) == "Ré maior" && keyName({-2, "maj"}) == "Sib maior" && keyName({4, "min"}) == "Dó# menor");
    assert(tonicName(*parseKey("D")) == "Ré" && tonicName(*parseKey("Bb")) == "Sib");  // the whistle's key
    assert(noteName(spell(d, 66)) == "F#4");
    setSolfege(true, false);  // note names Do Re Mi, keys letters
    assert(keyName({2, "maj"}) == "D maior" && tonicName(*parseKey("D")) == "D" && noteName(spell(d, 66)) == "Fá#4");
    assert(!setLanguage("en"));
    setSolfege(true, true);
    assert(keyName({2, "maj"}) == "D major" && tonicName(*parseKey("D")) == "D");  // English keys are always letters
    setSolfege(false, false);
    assert(!strcmp(tr("Play"), "Play") && keyName({2, "maj"}) == "D major");
    assert(!setLanguage("xx"));  // no such file: English
    assert(!strcmp(tr("Play"), "Play") && songText(ts, ts.title) == "Hello");

    // automatic language: LC_ALL/LANG matched to a file in i18n/, else English
    auto sys = [](const char* v) { setenv("LC_ALL", v, 1); return systemLanguage(); };
    assert(sys("pt_BR.UTF-8") == "pt-BR" && sys("pt_PT") == "pt-BR" && sys("de_DE.UTF-8") == "en" && sys("C") == "en");
    unsetenv("LC_ALL");
    // the matching behind it, also used for the macOS preferred language when LANG/LC_ALL are empty (#54)
    assert(matchLanguage("pt-BR") == "pt-BR" && matchLanguage("pt_BR") == "pt-BR" && matchLanguage("pt-PT") == "pt-BR");
    assert(matchLanguage("en-US") == "en" && matchLanguage("") == "en" && matchLanguage("zh-Hans-CN") == "en");
    bool listed = false;
    for (auto& [code, name] : languages()) listed |= code == "pt-BR" && name == "Português (Brasil)";
    assert(listed);

    // i18n/pt-BR/*.json: merged by loadStrings(), which reports a file that doesn't parse and a key defined twice
    {
      fs::path dir = home / "strings";
      fs::create_directories(dir);
      std::ofstream(dir / "a.json") << R"({"Play": "Tocar", "Stop": "Parar"})";
      std::ofstream(dir / "b.json") << R"({"Play": "Toque", "Twice": "1", "Twice": "2"})";
      std::ofstream(dir / "c.json") << R"({"Stop": "x", not json)";  // a broken file's keys count for nothing
      std::ofstream(dir / "notes.txt") << "not a strings file";
      std::vector<std::string> problems;
      json m = loadStrings(dir, &problems);
      assert((problems == std::vector<std::string>{"\"Play\" is in a.json and b.json", "\"Twice\" is twice in b.json",
                                                   "c.json is not a JSON object"}));
      assert(m.size() == 3 && m["Stop"] == "Parar" && m.contains("Play") && m.contains("Twice"));
      assert(loadStrings(home / "none").is_null());
    }
    // pt-BR: parses, no key defined twice, no empty values, the same printf conversions as the English key
    std::vector<std::string> problems;
    json j = loadStrings(i18nDir() / "pt-BR", &problems);
    for (auto& p : problems) fprintf(stderr, "i18n/pt-BR: %s\n", p.c_str());
    assert(problems.empty() && j.is_object());
    auto specs = [](const std::string& t) {
      static const std::regex conv("%[-+ #0]*[0-9]*(\\.[0-9]+)?[a-zA-Z]");
      std::string r;
      for (std::sregex_iterator it(t.begin(), t.end(), conv), end; it != end; ++it) r += it->str();
      return r;
    };
    for (auto& [k, v] : j.items()) {
      if (!v.is_string() || v.get<std::string>().empty() || specs(k) != specs(v.get<std::string>()))
        fprintf(stderr, "i18n/pt-BR: bad value for \"%s\"\n", k.c_str());
      assert(v.is_string() && !v.get<std::string>().empty() && specs(k) == specs(v.get<std::string>()));
    }
    // every literal tr("..."), trf("...") and L("...") in the viewer and core has a Portuguese entry
    static const std::regex call("\\b(?:tr|trf|L)\\(\"((?:[^\"\\\\]|\\\\.)*)\"");
    for (auto& e : fs::directory_iterator(i18nDir().parent_path() / "src")) {
      if (e.path().extension() != ".cpp") continue;
      const std::string file = e.path().filename().string();
      std::ifstream in(e.path());
      std::string src((std::istreambuf_iterator<char>(in)), {});
      assert(!src.empty());
      for (std::sregex_iterator it(src.begin(), src.end(), call), end; it != end; ++it) {
        std::string key, lit = (*it)[1];
        for (size_t i = 0; i < lit.size(); i++) key += lit[i] == '\\' && ++i < lit.size() ? (lit[i] == 'n' ? '\n' : lit[i]) : lit[i];
        if (!j.contains(key)) fprintf(stderr, "i18n/pt-BR: missing \"%s\" (%s)\n", key.c_str(), file.c_str());
        assert(j.contains(key));
      }
    }
}
