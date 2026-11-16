// john-mcp: MCP server (stdio, newline-delimited JSON-RPC) exposing the tin whistle tools to an agent.
#include "john.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

using namespace john;

static const char* GUIDE = R"(John turns tunes into tin whistle tab sheets with technique markings. Songs are saved to the John data folder and appear live in the John GUI (staff + fingering charts + technique symbols).

Getting a tune into John (always via ABC -> import_abc):
- Photo/PDF of sheet music: read the image and transcribe it to ABC carefully (K: key signature, M: meter, L: unit, every bar). After importing, compare get_song bar by bar against the image.
- thesession.org: curl -s 'https://thesession.org/tunes/search?q=NAME&format=json', then curl -s 'https://thesession.org/tunes/ID?format=json'. Each settings[] item has an "abc" body (a lone "!" is a line break) and "key" like "Dmajor". Add headers yourself: X:1, T:name, M: from the tune type (reel/hornpipe/barndance/strathspey 4/4, jig 6/8, slip jig 9/8, slide 12/8, polka 2/4, waltz/mazurka 3/4, three-two 3/2), L:1/8, K:key. Pass source = the setting URL.
- Tin whistle tabs (letters or hole diagrams): convert to ABC pitches for the given whistle (D whistle: XXXXXX=D, XXXXXO=E, XXXXOO=F#, XXXOOO=G, XXOOOO=A, XOOOOO=B, OXXOOO=c natural, OOOOOO=c#, + or 'blow harder' = next octave, lowercase in ABC). If the tab has no rhythm, ask the user or infer from the tune type and say so.
- ABC can already carry techniques: ~ or !roll! = roll, {g}A = cut, {F}G = tap, !tap! !cran! !slide! !tongue! !breath! (any technique id works as !id!).

Then:
1. analyze_song: range check, best transposition, and candidate spots for cuts, rolls, crans, slides, breaths, tonguing. These are hints only.
2. If notes are out of range or need many half-holes, update_song with transpose (keep the key the user expects unless it is unplayable).
3. Decide techniques with musical judgment for the tune type: jigs/reels/hornpipes get rolls on long notes and cuts on repeated notes; slow airs get slides, vibrato and few cuts; don't ornament every candidate, and leave beginners room to breathe. Use set_techniques with a short comment on each ornamented note explaining why (the player is learning to read music).
4. update_song notes = 2-4 sentences of overall advice (style, tempo, where to breathe).
5. Tell the user the song id; it is visible in the John GUI.
Missing a technique? define_technique adds it to the catalog (shown in the GUI legend).)";

static json textResult(const std::string& t, bool err = false) {
  json r = {{"content", json::array({{{"type", "text"}, {"text", t}}})}};
  if (err) r["isError"] = true;
  return r;
}

static json tool(const char* name, const char* desc, json props, std::vector<std::string> required = {}) {
  json schema = {{"type", "object"}, {"properties", props}};
  if (!required.empty()) schema["required"] = required;
  return {{"name", name}, {"description", desc}, {"inputSchema", schema}};
}

static json toolList() {
  json str = {{"type", "string"}};
  json strArr = {{"type", "array"}, {"items", str}};
  return json::array({
      tool("list_songs", "List saved songs (id, title, key, meter).", json::object()),
      tool("import_abc",
           "Create a song from ABC notation (full tune with X/T/M/L/K headers). Returns warnings and the note listing. "
           "Techniques in the ABC (~, {g}A grace cuts, !id! decorations) are kept.",
           {{"abc", str},
            {"id", {{"type", "string"}, {"description", "optional song id; default derived from the title"}}},
            {"source", {{"type", "string"}, {"description", "where the tune came from (URL, 'photo of ...')"}}},
            {"whistle", {{"type", "string"}, {"description", "whistle key, default D"}}},
            {"overwrite", {{"type", "boolean"}, {"description", "replace an existing song with the same id"}}}},
           {"abc"}),
      tool("get_song",
           "Show a song as a table: event index, bar, beat, note, length, whistle fingering (X covered, O open, H half, + upper octave), techniques, comment.",
           {{"id", str}}, {"id"}),
      tool("analyze_song",
           "Deterministic hints: whistle range check, best transpositions, and candidate notes for cuts, rolls, crans, slides, tonguing and breaths.",
           {{"id", str}}, {"id"}),
      tool("set_techniques",
           "Edit techniques on notes, by event index from get_song. Each edit may replace the list ('techniques'), or 'add'/'remove' ids, and set a 'comment' explaining the choice. All edits are validated before any is applied.",
           {{"id", str},
            {"edits",
             {{"type", "array"},
              {"items",
               {{"type", "object"},
                {"properties", {{"index", {{"type", "integer"}}}, {"techniques", strArr}, {"add", strArr}, {"remove", strArr}, {"comment", str}}},
                {"required", {"index"}}}}}}},
           {"id", "edits"}),
      tool("update_song",
           "Change song info: title, source, notes (overall advice shown in the GUI), tempo (quarter notes per minute), whistle key, or transpose by semitones.",
           {{"id", str}, {"title", str}, {"source", str}, {"notes", str}, {"tempo", {{"type", "integer"}}}, {"whistle", str}, {"transpose", {{"type", "integer"}}}},
           {"id"}),
      tool("list_techniques", "List the technique catalog: id, GUI symbol, name, how to play it on the whistle.", json::object()),
      tool("define_technique", "Add or replace a technique in the catalog (for techniques the user wants documented).",
           {{"id", {{"type", "string"}, {"description", "lowercase id like 'bounce'"}}}, {"name", str},
            {"symbol", {{"type", "string"}, {"description", "short text drawn above the note, 1-3 chars"}}},
            {"description", str}},
           {"id", "name", "symbol", "description"}),
  });
}

static Song mustLoad(const json& a) {
  auto s = loadSong(a.value("id", ""));
  if (!s) throw std::runtime_error("no song with id '" + a.value("id", "") + "' (see list_songs)");
  return *s;
}

static std::string callTool(const std::string& name, const json& a) {
  if (name == "list_songs") {
    std::string r;
    for (auto& id : listSongs())
      if (auto s = loadSong(id)) r += id + " - " + s->title + " (" + keyName(s->key) + ", " + s->meter + ")\n";
    return r.empty() ? "No songs yet. Data folder: " + dataDir().string() : r;
  }
  if (name == "import_abc") {
    ImportResult ir = importAbc(a.value("abc", ""));
    Song& s = ir.song;
    if (s.events.empty()) throw std::runtime_error("no notes found in the ABC");
    s.source = a.value("source", s.source);
    s.whistle = a.value("whistle", "D");
    if (whistleBase(s.whistle) < 0) throw std::runtime_error("unknown whistle key '" + s.whistle + "' (use a note name like D, C, Bb)");
    std::string base = slug(a.value("id", s.title));
    s.id = base;
    if (!a.value("overwrite", false))
      for (int n = 2; loadSong(s.id); n++) s.id = base + "-" + std::to_string(n);
    saveSong(s);
    std::string w;
    for (auto& x : ir.warnings) w += "warning: " + x + "\n";
    return "Saved as '" + s.id + "'.\n" + w + "\n" + listing(s);
  }
  if (name == "get_song") return listing(mustLoad(a));
  if (name == "analyze_song") return analyze(mustLoad(a));
  if (name == "set_techniques") {
    Song s = mustLoad(a);
    std::set<std::string> known;
    for (auto& t : loadTechniques()) known.insert(t.id);
    const json edits = a.value("edits", json::array());
    if (!edits.is_array()) throw std::runtime_error("edits must be an array");
    for (auto& e : edits) {  // validate everything first
      long long i = e.is_object() && e.contains("index") && e["index"].is_number_integer() ? e["index"].get<long long>() : -1;
      if (i < 0 || i >= (long long)s.events.size() || s.events[i].kind != Kind::Note)
        throw std::runtime_error("index " + std::to_string(i) + " is not a note (see get_song)");
      for (const char* k : {"techniques", "add", "remove"})
        for (auto& t : e.value(k, std::vector<std::string>{}))
          if (!known.count(t)) throw std::runtime_error("unknown technique '" + t + "' (see list_techniques or define_technique)");
    }
    std::set<int> touched;
    for (auto& e : edits) {
      Event& ev = s.events[e["index"].get<int>()];
      if (e.contains("techniques")) ev.techniques = e["techniques"].get<std::vector<std::string>>();
      for (auto& t : e.value("add", std::vector<std::string>{}))
        if (std::find(ev.techniques.begin(), ev.techniques.end(), t) == ev.techniques.end()) ev.techniques.push_back(t);
      for (auto& t : e.value("remove", std::vector<std::string>{}))
        ev.techniques.erase(std::remove(ev.techniques.begin(), ev.techniques.end(), t), ev.techniques.end());
      if (e.contains("comment")) ev.comment = e["comment"].get<std::string>();
      touched.insert(e["index"].get<int>());
    }
    saveSong(s);
    std::string out = "Updated " + std::to_string(touched.size()) + " note(s).\n";
    std::istringstream lines(listing(s));
    for (std::string l; std::getline(lines, l);) {
      int idx;
      if (sscanf(l.c_str(), "#%d", &idx) == 1 && touched.count(idx)) out += l + "\n";
    }
    return out;
  }
  if (name == "update_song") {
    Song s = mustLoad(a);
    s.title = a.value("title", s.title);
    s.source = a.value("source", s.source);
    s.notes = a.value("notes", s.notes);
    s.tempo = a.value("tempo", s.tempo);
    if (a.contains("whistle")) {
      if (whistleBase(a["whistle"].get<std::string>()) < 0) throw std::runtime_error("unknown whistle key (use a note name like D, C, Bb)");
      s.whistle = a["whistle"];
    }
    if (int t = a.value("transpose", 0)) {
      if (t < -24 || t > 24) throw std::runtime_error("transpose must be within -24..24 semitones");
      for (auto& e : s.events)
        if (e.kind == Kind::Note && (e.midi + t < 0 || e.midi + t > 127)) throw std::runtime_error("transposition leaves the MIDI range");
      transpose(s, t);
    }
    saveSong(s);
    return "Saved.\n" + listing(s);
  }
  if (name == "list_techniques") {
    std::string r;
    for (auto& t : loadTechniques()) r += t.id + " [" + t.symbol + "] " + t.name + ": " + t.description + "\n";
    return r;
  }
  if (name == "define_technique") {
    Technique t{a.value("id", ""), a.value("name", ""), a.value("symbol", ""), a.value("description", "")};
    if (t.id.empty() || !std::all_of(t.id.begin(), t.id.end(), [](char c) { return islower(c) || isdigit(c) || c == '_'; }))
      throw std::runtime_error("id must be lowercase letters, digits or _");
    saveCustomTechnique(t);
    return "Technique '" + t.id + "' saved.";
  }
  throw std::runtime_error("unknown tool " + name);
}

static void send(const json& j) {
  std::cout << j.dump(-1, ' ', false, json::error_handler_t::replace) << "\n" << std::flush;
}

int main() {
  std::ios::sync_with_stdio(false);
  static const std::set<std::string> versions = {"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};
  for (std::string line; std::getline(std::cin, line);) {
    if (line.empty()) continue;
    json msg = json::parse(line, nullptr, false);
    if (msg.is_discarded()) {
      send({{"jsonrpc", "2.0"}, {"id", nullptr}, {"error", {{"code", -32700}, {"message", "parse error"}}}});
      continue;
    }
    if (!msg.is_object()) {
      send({{"jsonrpc", "2.0"}, {"id", nullptr}, {"error", {{"code", -32600}, {"message", "invalid request (batches are not supported)"}}}});
      continue;
    }
    if (!msg.contains("id") || !msg.contains("method")) continue;  // notifications and client responses need no reply
    json id = msg["id"];
    json reply = {{"jsonrpc", "2.0"}, {"id", id}};
    if (!msg["method"].is_string()) {
      reply["error"] = {{"code", -32600}, {"message", "method must be a string"}};
      send(reply);
      continue;
    }
    std::string method = msg["method"];
    json params = msg.contains("params") && msg["params"].is_object() ? msg["params"] : json::object();
    if (method == "initialize") {
      std::string v = params.contains("protocolVersion") && params["protocolVersion"].is_string() ? params["protocolVersion"].get<std::string>() : "";
      reply["result"] = {{"protocolVersion", versions.count(v) ? v : "2025-11-25"},
                         {"capabilities", {{"tools", json::object()}}},
                         {"serverInfo", {{"name", "john"}, {"version", "0.1.0"}}},
                         {"instructions", GUIDE}};
    } else if (method == "ping") {
      reply["result"] = json::object();
    } else if (method == "tools/list") {
      reply["result"] = {{"tools", toolList()}};
    } else if (method == "tools/call") {
      try {
        json args = params.contains("arguments") && params["arguments"].is_object() ? params["arguments"] : json::object();
        reply["result"] = textResult(callTool(params.value("name", ""), args));
      } catch (const json::exception& e) {
        reply["result"] = textResult(std::string("Error: bad argument type (") + e.what() + ")", true);
      } catch (const std::exception& e) {
        reply["result"] = textResult(std::string("Error: ") + e.what(), true);
      }
    } else {
      reply["error"] = {{"code", -32601}, {"message", "method not found: " + method}};
    }
    send(reply);
  }
}
