// MCP tools for audio: transcribe_audio.
#include "mcp.hpp"

static std::string transcribe_audio(const json& a) {
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
  o.separate = a.value("separate", "");
  if (!o.separate.empty() && o.separate != "vocals" && o.separate != "other")
    throw std::runtime_error("separate must be \"vocals\" or \"other\"");
  if (a.contains("rubato") && !a["rubato"].is_boolean()) throw std::runtime_error("rubato must be true or false");
  o.rubato = a.value("rubato", false);
  Transcription t = transcribeFile(path, a.value("start", 0.0), a.value("end", 0.0), o);
  return t.report + "\nDraft ABC (review, fix, then import_abc with source = the file name and instrument = " + o.instrument +
         "):\n" + toAbc(t.song);
}

std::vector<Tool> audioTools() {
  return {
      tool("transcribe_audio",
           "Transcribe the melody of an audio file (mp3, wav, flac) into ABC for an instrument (default D whistle): pitch tracking, beat tracking, "
           "sixteenth-note grid (eighth triplets where they fit, swing detected and written straight), key detection, octave fitted to the instrument's range (transposed to another key if no octave fits; see transpose). Works best on solo instruments/voice; on full mixes it "
           "follows the strongest pitched line. Returns a report and a draft ABC in the detected meter (4/4, 3/4, 6/8 or 9/8, scores in the report). "
           "Don't import it blindly: check the meter (do motifs fit the bars?), tempo (the report's tempo confidence; doubled/halved note lengths?), pickup, key/mode "
           "and repeats, re-run with meter/bpm/start/end (or rubato for a drifting tempo) as needed, tidy the ABC, then import_abc, write song notes about what's "
           "uncertain, and ask the user to compare it by ear (see the server instructions).",
           {{"path", {{"type", "string"}, {"description", "absolute path (or ~/...) to the audio file"}}},
            {"bpm", {{"type", "number"}, {"description", "quarter notes per minute; default: detect"}}},
            {"meter", {{"type", "string"}, {"description", "e.g. 4/4, 3/4, 6/8; default: detect"}}},
            {"start", {{"type", "number"}, {"description", "seconds into the file to start at (default 0)"}}},
            {"end", {{"type", "number"}, {"description", "seconds into the file to stop at (default: end)"}}},
            {"title", str},
            {"instrument", instrumentSchema},
            {"recording_instrument", {{"type", "string"},
                                      {"description", "instrument playing the melody in the recording (whistle = D whistle); limits the "
                                                      "pitch search to its range so accompaniment outside it is ignored. Default: unset"}}},
            {"transpose", {{"type", "integer"}, {"minimum", -12}, {"maximum", 12},
                           {"description", "semitones to transpose the melody by before fitting the octave (the draft's K: follows); "
                                           "0 keeps the recording's key. Default: automatic, only when no octave keeps the notes "
                                           "in the instrument's range (the report's 'transposed' line says so)"}}},
            {"separate", {{"type", "string"}, {"enum", {"vocals", "other"}},
                          {"description", "for dense mixes: first separate this Demucs stem (vocals = sung melody; other = whistle, flute, "
                                          "fiddle, piano lead) and transcribe it. Needs Demucs installed (pip install demucs, or "
                                          "JOHN_DEMUCS); slow, so pass start/end, and only short sections of copyrighted recordings, "
                                          "for personal practice. Without Demucs the full mix is transcribed and the report says why. "
                                          "Default: no separation"}}},
            {"rubato", {{"type", "boolean"},
                        {"description", "the tempo drifts (slow airs, film music, free playing): beats follow the recording's "
                                        "local tempo instead of a steady one; the report gives the tempo range. Default: false"}}}},
           {"path"}, transcribe_audio),
  };
}
