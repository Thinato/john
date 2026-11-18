// john-mcp: MCP server (stdio, newline-delimited JSON-RPC) exposing the tin whistle tools to an agent.
#include "john.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace john;

static const char* GUIDE = R"(John turns tunes into tin whistle tab sheets with technique markings. Songs are saved to the John data folder and appear live in the John GUI (staff + fingering charts + technique symbols).

Instruments: every song has one (instrument: whistle (default, with a whistle key), violin, piano, guitar, electric-guitar, harmonica). Pass it to import_abc / transcribe_audio, or change it with update_song (that keeps the written notes, so the tune may sound in another octave: transpose by 12 if needed). Range checks and transpositions follow it; fingering charts and the technique candidates are for the tin whistle. Notes are written the way that instrument's music is written: whistle an octave below how it sounds, guitar an octave above, the others at pitch.

Getting a tune into John (always via ABC -> import_abc, with instrument = what the user will play it on; the song list shows it and list_songs filters by it):
- Photo/PDF of sheet music: read the image and transcribe it to ABC carefully (K: key signature, M: meter, L: unit, every bar). After importing, compare get_song bar by bar against the image.
- thesession.org: curl -s 'https://thesession.org/tunes/search?q=NAME&format=json', then curl -s 'https://thesession.org/tunes/ID?format=json'. Each settings[] item has an "abc" body (a lone "!" is a line break) and "key" like "Dmajor". Add headers yourself: X:1, T:name, M: from the tune type (reel/hornpipe/barndance/strathspey 4/4, jig 6/8, slip jig 9/8, slide 12/8, polka 2/4, waltz/mazurka 3/4, three-two 3/2), L:1/8, K:key. Pass source = the setting URL.
- Audio file (mp3/wav/flac): transcribe_audio gives a draft ABC plus a report. The draft is a machine guess (detected meter, sixteenths with (3 eighth triplets where they fit, the loudest pitched line); you are the musician who turns it into a sheet. Review it before importing:
  * Solo instrument: instrument = what the sheet is for (a piano recording can become a whistle sheet); it doesn't limit what is heard. If you know which instrument plays the melody in the recording (e.g. a whistle over an orchestra), also pass recording_instrument: the pitch search then stays in its range, so louder accompaniment outside it can't take over (the report's "pitch search" line shows the range).
  * Meter: without a meter, the draft tries 4/4, 3/4, 6/8 and 9/8 and keeps the best fit; the report's meter line lists each score. Check it: do the repeating motifs line up with the bar lines, with long or accented notes on beat 1? If the scores are close (on real recordings they often are) or the meter doesn't suit the tune type (jig 6/8, slip jig 9/8, waltz 3/4, reel 4/4), re-run with meter and compare which version makes the phrases fit the bars. 2/4 and 12/8 aren't tried: expect 4/4 and 6/8.
  * Tempo: read the report's tempo line. Confidence low means the detected tempo may well be wrong: re-run with the 'also plausible' bpm (often half, double or 3:2; not always right either) or the song's usual tempo, and keep the version whose note lengths read naturally. If nearly every note is a sixteenth, the real tempo is probably double the detected one; if it is all half and whole notes, it's probably half. Re-run with bpm. If you know the song, use its usual tempo and feel.
  * Rhythm: a "triplets" line counts the beats written as (3; check them by ear. A "swung eighths" line means long-short pairs are written as straight eighths: keep them straight and say "swing" in the notes. Swung tunes and tunes with many triplets often come out 6/8 without a meter: re-run with meter (e.g. 4/4) if the tune type is simple time.
  * Pickup and key: does the tune start on a pickup? Do the accidentals point to another key or mode (e.g. a steady C natural in D major = D mixolydian, minor vs dorian)? Fix K: in the ABC.
  * Range: if no octave keeps the notes in the instrument's range, the draft is transposed to the key that fits best (for the whistle also with the fewest half-holes) and the report's 'transposed' line says from which key. If the user wants the recording's key, re-run with transpose = 0 (notes out of range are then folded by an octave); transpose = N forces another shift.
  * Structure: write repeated sections once with |: :| and 1st/2nd endings so the sheet is short and readable. Trim intros, outros and accompaniment-only passages with start/end (or delete_bars after import).
  * Notes: fix obvious slips (a single note an octave off, one-sixteenth blips, a lone out-of-key note in a clear phrase). Never invent melody that the draft doesn't support; mark doubtful spots instead.
  * Ornaments: cuts, taps and rolls heard in the recording are left out of the melody and listed in the report's ornaments line (bar and beat). In a whistle draft they are already marked on the main note (!cut! !tap! !roll!). They are candidates: keep those that suit the tune, drop doubtful ones (on piano or dense mixes they are usually false).
  Then import_abc (source = the file name, and the same instrument you transcribed for). In update_song notes, say it is an automatic transcription, which bars are uncertain, and the feel (tempo, swing, where to breathe). Comment on notes you changed or doubt (set_techniques comment, e.g. "unclear in the recording, check by ear"). Finally ask the user to play it in John next to the recording and tell you what sounds wrong: they are the ears, you are the eyes.
- Tin whistle tabs (letters or hole diagrams): convert to ABC pitches for the given whistle (D whistle: XXXXXX=D, XXXXXO=E, XXXXOO=F#, XXXOOO=G, XXOOOO=A, XOOOOO=B, OXXOOO=c natural, OOOOOO=c#, + or 'blow harder' = next octave, lowercase in ABC). If the tab has no rhythm, ask the user or infer from the tune type and say so.
- ABC can already carry techniques: ~ or !roll! = roll, {g}A = cut, {F}G = tap, !tap! !cran! !slide! !tongue! !breath! (any technique id works as !id!).

Then:
1. analyze_song: range check, best transposition, and candidate spots for cuts, rolls, crans, slides, breaths, tonguing. These are hints only.
2. If notes are out of range or need many half-holes, update_song with transpose (keep the key the user expects unless it is unplayable).
3. Decide techniques with musical judgment for the tune type: jigs/reels/hornpipes get rolls on long notes and cuts on repeated notes; slow airs get slides, vibrato and few cuts; don't ornament every candidate, and leave beginners room to breathe. Use set_techniques with a short comment on each ornamented note explaining why (the player is learning to read music).
4. update_song notes = 2-4 sentences of overall advice (style, tempo, where to breathe).
5. update_song folder by difficulty on a D whistle: Beginner = mostly low octave, slow or moderate, simple rhythm, no half-holes; Intermediate = second octave, C natural or half-holes, cuts, quicker tunes or syncopation; Advanced = fast dance tunes, rolls and crans, frequent half-holes, tricky rhythm. ("Lessons" is the bundled course.)
6. Tell the user the song id; it is visible in the John GUI.
Missing a technique? define_technique adds it to the catalog (shown in the GUI legend).
Only want part of a tune (e.g. just the chorus)? delete_bars removes bars by number; re-read get_song afterwards because event indexes shift.)";

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
  json instrumentSchema = {{"type", "string"}, {"enum", INSTRUMENTS}, {"description", "instrument the song is for, default whistle"}};
  return json::array({
      tool("list_songs", "List saved songs by folder (id, title, key, meter, instrument).",
           {{"instrument", {{"type", "string"}, {"enum", INSTRUMENTS}, {"description", "only songs for this instrument (any whistle key); default all"}}}}),
      tool("import_abc",
           "Create a song from ABC notation (full tune with X/T/M/L/K headers). Returns warnings and the note listing. "
           "Techniques in the ABC (~, {g}A grace cuts, !id! decorations) are kept.",
           {{"abc", str},
            {"id", {{"type", "string"}, {"description", "optional song id; default derived from the title"}}},
            {"source", {{"type", "string"}, {"description", "where the tune came from (URL, 'photo of ...')"}}},
            {"instrument", {{"type", "string"}, {"enum", INSTRUMENTS}, {"description", "instrument the song is for, default: the one chosen in John's Settings (whistle unless changed)"}}},
            {"whistle", {{"type", "string"}, {"description", "whistle key, default D"}}},
            {"overwrite", {{"type", "boolean"}, {"description", "replace an existing song with the same id"}}},
            {"folder", {{"type", "string"}, {"description", "Beginner, Intermediate, Advanced or Lessons (any name works)"}}}},
           {"abc"}),
      tool("get_song",
           "Show a song as a table: event index, bar, beat, note, length, whistle fingering (X covered, O open, H half, + upper octave; other instruments: out-of-range marks), techniques, comment.",
           {{"id", str}}, {"id"}),
      tool("analyze_song",
           "Deterministic hints: range check on the song's instrument, best transpositions, and candidate notes for cuts, rolls, crans, slides, tonguing and breaths.",
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
           "Change song info: title, source, notes (overall advice shown in the GUI), folder (song list group), tempo (quarter notes per minute), instrument, whistle key, or transpose by semitones.",
           {{"id", str}, {"title", str}, {"source", str}, {"notes", str}, {"folder", str}, {"tempo", {{"type", "integer"}}}, {"instrument", instrumentSchema}, {"whistle", str}, {"transpose", {{"type", "integer"}}}},
           {"id"}),
      tool("delete_bars",
           "Delete bars first..last (inclusive, bar numbers as get_song shows them; a pickup is bar 0) with their notes, rests and techniques. "
           "Repeat/end signs and volta marks closing the deleted bars move to the previous bar line. Event indexes change afterwards.",
           {{"id", str}, {"first", {{"type", "integer"}}}, {"last", {{"type", "integer"}, {"description", "default: same as first"}}}},
           {"id", "first"}),
      tool("transcribe_audio",
           "Transcribe the melody of an audio file (mp3, wav, flac) into ABC for an instrument (default D whistle): pitch tracking, beat tracking, "
           "sixteenth-note grid (eighth triplets where they fit, swing detected and written straight), key detection, octave fitted to the instrument's range (transposed to another key if no octave fits; see transpose). Works best on solo instruments/voice; on full mixes it "
           "follows the strongest pitched line. Returns a report and a draft ABC in the detected meter (4/4, 3/4, 6/8 or 9/8, scores in the report). "
           "Don't import it blindly: check the meter (do motifs fit the bars?), tempo (the report's tempo confidence; doubled/halved note lengths?), pickup, key/mode "
           "and repeats, re-run with meter/bpm/start/end as needed, tidy the ABC, then import_abc, write song notes about what's "
           "uncertain, and ask the user to compare it by ear (see the server instructions).",
           {{"path", {{"type", "string"}, {"description", "absolute path (or ~/...) to the audio file"}}},
            {"bpm", {{"type", "number"}, {"description", "quarter notes per minute; default: detect"}}},
            {"meter", {{"type", "string"}, {"description", "e.g. 4/4, 3/4, 6/8; default: detect"}}},
            {"start", {{"type", "number"}, {"description", "seconds into the file to start at (default 0)"}}},
            {"end", {{"type", "number"}, {"description", "seconds into the file to stop at (default: end)"}}},
            {"title", str},
            {"instrument", instrumentSchema},
            {"recording_instrument", {{"type", "string"}, {"enum", INSTRUMENTS},
                                      {"description", "instrument playing the melody in the recording (whistle = D whistle); limits the "
                                                      "pitch search to its range so accompaniment outside it is ignored. Default: unset"}}},
            {"transpose", {{"type", "integer"}, {"minimum", -12}, {"maximum", 12},
                           {"description", "semitones to transpose the melody by before fitting the octave (the draft's K: follows); "
                                           "0 keeps the recording's key. Default: automatic, only when no octave keeps the notes "
                                           "in the instrument's range (the report's 'transposed' line says so)"}}}},
           {"path"}),
      tool("list_techniques", "List the technique catalog: id, GUI symbol, name, how to play it on the whistle.", json::object()),
      tool("define_technique", "Add or replace a technique in the catalog (for techniques the user wants documented).",
           {{"id", {{"type", "string"}, {"description", "lowercase id like 'bounce'"}}}, {"name", str},
            {"symbol", {{"type", "string"}, {"description", "short text drawn above the note, 1-3 chars"}}},
            {"description", str}},
           {"id", "name", "symbol", "description"}),
  });
}

static void checkInstrument(const Song& s) {
  if (whistleBase(s.whistle) < 0) throw std::runtime_error("unknown whistle key '" + s.whistle + "' (use a note name like D, C, Bb)");
  if (!findInstrument(s.instrument, s.whistle)) {
    std::string ids;
    for (auto& i : INSTRUMENTS) ids += (ids.empty() ? "" : ", ") + i;
    throw std::runtime_error("unknown instrument '" + s.instrument + "' (use one of: " + ids + ")");
  }
}

static Song mustLoad(const json& a) {
  auto s = loadSong(a.value("id", ""));
  if (!s) throw std::runtime_error("no song with id '" + a.value("id", "") + "' (see list_songs)");
  return *s;
}

static std::string callTool(const std::string& name, const json& a) {
  if (name == "list_songs") {
    std::string inst = a.value("instrument", "");
    if (!inst.empty() && !findInstrument(inst)) throw std::runtime_error("unknown instrument '" + inst + "'");
    std::map<std::string, std::string, decltype(&folderBefore)> byFolder(&folderBefore);
    for (auto& id : listSongs())
      if (auto s = loadSong(id); s && (inst.empty() || instrumentOf(*s).id == inst))
        byFolder[s->folder] += "  " + id + " - " + s->title + " (" + keyName(s->key) + ", " + s->meter + ", " + instrumentOf(*s).name + ")\n";
    std::string r;
    for (auto& [f, lines] : byFolder) r += (f.empty() ? "Unsorted" : f) + ":\n" + lines;
    if (r.empty() && !inst.empty()) return "No songs for instrument '" + inst + "'.";
    return r.empty() ? "No songs yet. Data folder: " + dataDir().string() : r;
  }
  if (name == "import_abc") {
    ImportResult ir = importAbc(a.value("abc", ""));
    Song& s = ir.song;
    if (s.events.empty()) throw std::runtime_error("no notes found in the ABC");
    s.source = a.value("source", s.source);
    s.instrument = a.value("instrument", loadSettings().instrument);
    s.whistle = a.value("whistle", "D");
    s.folder = a.value("folder", "");
    checkInstrument(s);
    s.id = a.value("overwrite", false) ? slug(a.value("id", s.title)) : uniqueId(a.value("id", s.title));
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
    s.folder = a.value("folder", s.folder);
    s.tempo = a.value("tempo", s.tempo);
    if (a.contains("instrument") || a.contains("whistle")) {  // a song with a bad stored value can still be edited
      s.instrument = a.value("instrument", s.instrument);
      s.whistle = a.value("whistle", s.whistle);
      checkInstrument(s);
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
  if (name == "delete_bars") {
    Song s = mustLoad(a);
    int first = a.value("first", 0);
    int n = deleteBars(s, first, a.value("last", first));
    saveSong(s);
    return "Deleted " + std::to_string(n) + " note(s)/rest(s).\n" + listing(s);
  }
  if (name == "transcribe_audio") {
    std::string path = a.value("path", "");
    if (!findInstrument(a.value("instrument", "whistle"))) throw std::runtime_error("unknown instrument '" + a.value("instrument", "") + "'");
    if (a.contains("recording_instrument") && !findInstrument(a.value("recording_instrument", "")))
      throw std::runtime_error("unknown recording_instrument '" + a.value("recording_instrument", "") + "'");
    TranscribeOptions o;
    o.meter = a.value("meter", o.meter);
    if (!o.meter.empty() && (o.meter = parseMeter(o.meter)).empty())
      throw std::runtime_error("meter must be N/D with D = 2, 4, 8 or 16 (e.g. 4/4, 3/4, 6/8), or C or C|");
    o.bpm = a.value("bpm", 0.0);
    o.title = a.value("title", fs::path(path).stem().string());
    o.instrument = a.value("instrument", o.instrument);
    o.recordingInstrument = a.value("recording_instrument", "");
    if (a.contains("transpose")) {
      const json& t = a["transpose"];
      if (!t.is_number_integer() || t < -12 || t > 12)
        throw std::runtime_error("transpose must be a whole number within -12..12 semitones");
      o.transpose = a["transpose"].get<int>();
    }
    Transcription t = transcribeFile(path, a.value("start", 0.0), a.value("end", 0.0), o);
    return t.report + "\nDraft ABC (review, fix, then import_abc with source = the file name and instrument = " + o.instrument +
           "):\n" + toAbc(t.song);
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
#ifdef _WIN32  // messages end in a bare \n: no \r\n translation on either stream
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  std::ios::sync_with_stdio(false);
  try { installSongs(lessonsDir()); } catch (...) {}  // nothing may reach stdout but JSON-RPC
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
