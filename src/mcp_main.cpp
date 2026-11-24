// john-mcp: MCP server (stdio, newline-delimited JSON-RPC) exposing the tin whistle tools to an agent. The tools are
// in mcp_<area>.cpp (mcp.hpp); this file has the JSON-RPC loop, initialize and the agent guide.
#include "mcp.hpp"
#include <iostream>
#include <set>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif


static const char* GUIDE = R"(John turns tunes into tin whistle tab sheets with technique markings. Songs are saved to the John data folder and appear live in the John GUI (staff + fingering charts + technique symbols).

Instruments: every song has one (instrument: whistle (default, with a whistle key), violin, piano, guitar, electric-guitar, harmonica (10-hole diatonic, with a harp key: G..B harps sit below C, Db..F# above), voice-soprano, voice-alto, voice-tenor, voice-bass, or a custom one). list_instruments shows them all with their range and hard notes (whistle half-holes, harmonica bends and overblows); define_instrument creates or replaces a custom one ("I play a C flute, low D to high A": name, low, high, key, chromatic or diatonic preset, per-note levels, techniques, sound), delete_instrument removes it. Pass it to import_abc / import_musicxml / transcribe_audio, or change it with update_song (that keeps the written notes, so the tune may sound in another octave: transpose by 12 if needed). Range checks and transpositions follow it; fingering charts are for the tin whistle, and analyze_song's technique candidates follow tin whistle fingering, offered as the user's instrument shows them (see below). Notes are written the way that instrument's music is written: whistle an octave below how it sounds, guitar an octave above, the others at pitch. A song's instrument is what it was written for; the user plays the instrument chosen in John's Settings, and list_songs / analyze_song say whether each song fits it as written, in another key (update_song transpose), or not at all (pass instrument to ask about another one). The GUI's song list shows only the songs that fit. Technique marks follow the user's instrument too: the sheet shows the marks its profile lists (list_instruments), plays others as their listed equivalent (piano: cuts and taps as grace notes, rolls and crans as turns; harmonica: cuts and taps as grace notes, rolls as shakes) and hides the rest; breath marks always show (a phrase end on instruments that don't breathe). The song file keeps every mark, so mark the tune for the instrument it was written for; analyze_song offers the candidates the chosen instrument shows.

Singers: the voices are instruments too (voice-soprano C4..A5, voice-alto G3..E5, voice-tenor and voice-bass written an octave above how they sound, treble clef, like guitar). Help a singer by asking which voice fits, or have them press Find my range in John's Settings (under My instrument, for a voice): they sing their lowest and highest comfortable notes and John saves a custom instrument my-voice ("My voice") with that range. Without the GUI, define_instrument with voice = true does the same (octave -1 for a man's voice, techniques breath, vibrato, slide). Then list_songs shows the songs that fit their range as written or in another key; to move one, use update_song transpose, or tell them to press Play in key next to the sheet. On the mic a note sung an octave off counts as right. Mark breaths (where to take air), slides and vibrato on long notes; finger ornaments (cuts, rolls) are hidden on a voice. Lyrics (w: lines, below) are drawn under the notes and the syllable being sung is highlighted.

Getting a tune into John (via ABC -> import_abc, with instrument = what the user will play it on; the song list shows it and list_songs filters by it):
- MusicXML file (.musicxml, .xml or zipped .mxl, exported by MuseScore, Finale, Sibelius, flat.io or an optical music reader): import_musicxml with path (same options as import_abc). Read its warnings: only the first part and voice are kept, chords keep the top note. Then check get_song.
- Photo/PDF of sheet music: read the image and transcribe it to ABC carefully (K: key signature, M: meter, L: unit, every bar). After importing, compare get_song bar by bar against the image.
- thesession.org: curl -s 'https://thesession.org/tunes/search?q=NAME&format=json', then curl -s 'https://thesession.org/tunes/ID?format=json'. Each settings[] item has an "abc" body (a lone "!" is a line break) and "key" like "Dmajor". Add headers yourself: X:1, T:name, M: from the tune type (reel/hornpipe/barndance/strathspey 4/4, jig 6/8, slip jig 9/8, slide 12/8, polka 2/4, waltz/mazurka 3/4, three-two 3/2), L:1/8, K:key. Pass source = the setting URL.
- Audio file (mp3/wav/flac): transcribe_audio gives a draft ABC plus a report. The draft is a machine guess (detected meter, sixteenths with (3 eighth triplets where they fit, the loudest pitched line); you are the musician who turns it into a sheet. Review it before importing:
  * Solo instrument: instrument = what the sheet is for (a piano recording can become a whistle sheet); it doesn't limit what is heard. If you know which instrument plays the melody in the recording (e.g. a whistle over an orchestra), also pass recording_instrument: the pitch search then stays in its range, so louder accompaniment outside it can't take over (the report's "pitch search" line shows the range).
  * Dense mix (a singer or lead over a full band, and the draft follows the accompaniment): re-run with separate = "vocals" for a sung melody, or "other" for a whistle, flute, fiddle or piano lead. That runs Demucs (an optional tool the user installs: pip install demucs, or JOHN_DEMUCS = its path) on the section first and transcribes only that stem. It is slow (a CPU can take longer than the section itself), so always pass start/end: only short sections of copyrighted recordings, for personal practice. The report's "separation" line says whether the stem or, when Demucs is missing or fails, the full mix was transcribed.
  * Meter: without a meter, the draft tries 4/4, 3/4, 6/8 and 9/8 and keeps the best fit; the report's meter line lists each score. Check it: do the repeating motifs line up with the bar lines, with long or accented notes on beat 1? If the scores are close (on real recordings they often are) or the meter doesn't suit the tune type (jig 6/8, slip jig 9/8, waltz 3/4, reel 4/4), re-run with meter and compare which version makes the phrases fit the bars. 2/4 and 12/8 aren't tried: expect 4/4 and 6/8.
  * Tempo: read the report's tempo line. Confidence low means the detected tempo may well be wrong: re-run with the 'also plausible' bpm (often half, double or 3:2; not always right either) or the song's usual tempo, and keep the version whose note lengths read naturally. If nearly every note is a sixteenth, the real tempo is probably double the detected one; if it is all half and whole notes, it's probably half. Re-run with bpm. If you know the song, use its usual tempo and feel.
  * Rubato: slow airs, film music and free playing speed up and slow down, so no steady tempo fits and note lengths come out wrong (the tempo line then usually says confidence low). Re-run with rubato: true: the beats follow the recording's local tempo and the tempo line gives the range (the song's tempo is the median). For a piece in a few steady sections at different tempos, transcribe each with start/end and its own bpm instead.
  * Rhythm: a "triplets" line counts the beats written as (3; check them by ear. A "swung eighths" line means long-short pairs are written as straight eighths: keep them straight and say "swing" in the notes. Swung tunes and tunes with many triplets often come out 6/8 without a meter: re-run with meter (e.g. 4/4) if the tune type is simple time.
  * Pickup and key: does the tune start on a pickup? Do the accidentals point to another key or mode (e.g. a steady C natural in D major = D mixolydian, minor vs dorian)? Fix K: in the ABC.
  * Range: if no octave keeps the notes in the instrument's range, the draft is transposed to the key that fits best (for the whistle also with the fewest half-holes) and the report's 'transposed' line says from which key. If the user wants the recording's key, re-run with transpose = 0 (notes out of range are then folded by an octave); transpose = N forces another shift.
  * Structure: write repeated sections once with |: :| and 1st/2nd endings so the sheet is short and readable. Trim intros, outros and accompaniment-only passages with start/end (or delete_bars after import).
  * Notes: fix obvious slips (a single note an octave off, one-sixteenth blips, a lone out-of-key note in a clear phrase). Never invent melody that the draft doesn't support; mark doubtful spots instead.
  * Ornaments: cuts, taps and rolls heard in the recording are left out of the melody and listed in the report's ornaments line (bar and beat). In a whistle draft they are already marked on the main note (!cut! !tap! !roll!). They are candidates: keep those that suit the tune, drop doubtful ones (on piano or dense mixes they are usually false).
  Then import_abc (source = the file name, and the same instrument you transcribed for). In update_song notes, say it is an automatic transcription, which bars are uncertain, and the feel (tempo, swing, where to breathe). Comment on notes you changed or doubt (set_techniques comment, e.g. "unclear in the recording, check by ear"). Finally ask the user to play it in John next to the recording and tell you what sounds wrong: they are the ears, you are the eyes.
- Tin whistle tabs (letters or hole diagrams): convert to ABC pitches for the given whistle (D whistle: XXXXXX=D, XXXXXO=E, XXXXOO=F#, XXXOOO=G, XXOOOO=A, XOOOOO=B, OXXOOO=c natural, OOOOOO=c#, + or 'blow harder' = next octave, lowercase in ABC). If the tab has no rhythm, ask the user or infer from the tune type and say so.
- ABC can already carry techniques: ~ or !roll! = roll, {g}A = cut, {F}G = tap, !tap! !cran! !slide! !tongue! !breath! (any technique id works as !id!).
- Lyrics: a w: line under a music line gives its notes syllables (ABC 2.1: "-" between syllables, "_" holds the previous syllable over a note, "*" skips a note, "~" joins words on one note, "\-" is a literal hyphen, "|" jumps to the next bar; rests and tied notes take none). First verse only. W: lines (words after the tune) are kept as text. MusicXML <lyric> verse 1 is imported too.

Then:
1. analyze_song: range check, best transposition, and candidate spots for cuts, rolls, crans, slides, breaths, tonguing. These are hints only.
2. If notes are out of range or need many half-holes, update_song with transpose (keep the key the user expects unless it is unplayable).
3. Decide techniques with musical judgment for the tune type: jigs/reels/hornpipes get rolls on long notes and cuts on repeated notes; slow airs get slides, vibrato and few cuts; don't ornament every candidate, and leave beginners room to breathe. Use set_techniques with a short comment on each ornamented note explaining why (the player is learning to read music). Store the technique of the instrument the song is written for: when analyze_song names an equivalent, like turn (the Piano's way to play: long roll ...), set the one in parentheses (roll), never the equivalent (turn); the sheet shows the equivalent by itself, and the song keeps its whistle marks for when the user switches back.
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

Tool tool(const char* name, const char* desc, json props, std::vector<std::string> required, Handler call) {
  json schema = {{"type", "object"}, {"properties", props}};
  if (!required.empty()) schema["required"] = required;
  return {{{"name", name}, {"description", desc}, {"inputSchema", schema}}, call};
}
Tool tool(const char* name, const char* desc, json props, Handler call) { return tool(name, desc, props, {}, call); }

static const std::vector<Tool>& tools() {  // every area's, in tools/list order
  static const std::vector<Tool> all = [] {
    std::vector<Tool> t;
    for (auto area : {songTools, audioTools, techniqueTools, instrumentTools})
      for (auto& x : area()) t.push_back(x);
    return t;
  }();
  return all;
}

static json toolList() {
  json list = json::array();
  for (auto& t : tools()) list.push_back(t.schema);
  return list;
}

static std::string callTool(const std::string& name, const json& a) {
  reloadInstruments();  // the GUI may have added, changed or deleted a custom instrument
  loadTechniques();      // and another agent a technique (builtinTechnique() reads the custom ids it caches)
  for (auto& t : tools())
    if (t.schema.at("name") == name) return t.call(a);
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
