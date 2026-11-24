// The viewer's shared state (App) and what more than one GUI file uses: declared here, never copied. Each window or
// panel lives in its own gui_*.cpp (kb/notes/where-things-go.md).
#pragma once
#include "audio.hpp"
#include "john.hpp"
#include <imgui.h>
#include <cfloat>
#ifdef __EMSCRIPTEN__
#include <GLFW/glfw3.h>  // GLuint
#else
#include <future>
#endif

using namespace john;

extern ImFont* gMusic;  // Bravura (SMuFL)
extern ImFont* gText;
extern float gMusicScale;  // ImGui sizes fonts by line height; Bravura's is tall, so rescale to 1 em = size

// SMuFL code points
enum : unsigned {
  Brace = 0xE000, GClef = 0xE050, GClef8vb = 0xE052, FClef = 0xE062, TimeSig0 = 0xE080, TimeCommon = 0xE08A, TimeCut = 0xE08B,
  HeadWhole = 0xE0A2, HeadHalf = 0xE0A3, HeadBlack = 0xE0A4, Dot = 0xE1E7,
  Flag8Up = 0xE240, Flag8Down = 0xE241,  // +2 per extra flag
  Flat = 0xE260, Natural = 0xE261, Sharp = 0xE262,
  RestWhole = 0xE4E3, RestHalf = 0xE4E4, RestQuarter = 0xE4E5,  // +1 per shorter rest
  BreathComma = 0xE4CE,
};

// gui_main.cpp: text and names every window uses
std::string utf8(unsigned cp);
std::string L(const char* english);
std::string keyLabel(const std::string& k);
std::string instName(const Instrument& in);
Instrument myInstrument(const Settings& s);
std::string instName(const std::string& id, const std::string& whistle = "D", const std::string& harp = "C");

// ---------------- app state ----------------
#ifdef __EMSCRIPTEN__
// The first page of an image import (#75), shown next to the sheet to compare. Kept until the page is closed.
struct Original { GLuint tex = 0; int w = 0, h = 0; };
#else
struct Original;  // web only
#endif
struct App {
  std::vector<std::string> ids;
  struct Meta { fs::file_time_type mtime{}; std::string folder, title; Instrument in; std::string deleted; Playability play; };
  std::vector<Instrument> catalog;   // the instruments meta was computed with: a change (editor, agent) judges every song again
  std::optional<Instrument> editing; // Settings' instrument editor (#85): the custom instrument being created or edited
  bool editingNew = false;
  char editName[64] = "";
  std::string editError;
  int deleteSongs = 0;               // songs written for the instrument the delete popup asks about
  std::map<std::string, Meta> meta;  // song list info, re-read only when a file changes or my instrument does
  std::string playFor;               // my instrument (Settings) that meta's playability was computed for
  std::vector<Technique> techs;
  std::optional<Song> song;
  std::optional<PitchTrack> track;  // the open song's pitch track (Import audio), drawn under each line of music
  std::string current;
  fs::file_time_type songMtime{}, newestSeen{};
  int sel = -1;
  Settings set = loadSettings();  // edited in place (Settings window, sidebar, transport); saved when it changes
  Settings saved = set;
  bool showSettings = false;
  bool english = true;          // the viewer's language is English (set by main() when the language changes)
  bool follow = true;
  bool busy = false;            // Import audio is transcribing on a worker thread (see importWindow)
#ifndef __EMSCRIPTEN__
  bool showImport = false;
  char importPath[1024] = "", importStart[16] = "", importEnd[16] = "", importTempo[16] = "", importMeter[16] = "";
  std::string importInst, importRec, importError;
  int importSep = 0;            // Demucs stem: 0 off, 1 vocals, 2 other
  bool importRubato = false;
  std::string importNote;       // the last import's separation: line, shown until the next import
  std::future<Song> importing;  // valid while transcribing
#endif
  bool showAbc = false;         // Import song (ABC), see abcWindow
  std::vector<char> abcText = std::vector<char>(1 << 17);  // the paste box (heap: the web's stack is small)
  char abcPath[1024] = "";
  std::string abcError, abcResult;
  // Import song's instrument (this session only, not saved): "" = Settings' instrument / whistle / harp until the user picks one
  std::string abcInst, abcWhistle, abcHarp;
#ifdef __EMSCRIPTEN__
  std::vector<Original> originals;              // ponytail: textures live until the page closes; few imports per visit
  std::map<std::string, size_t> originalOf;     // song id -> its original in `originals`
  bool showOriginal = true;                     // the original instead of the details panel, for songs that have one
  float originalZoom = 1;
#endif
  char comment[2048] = "";
  double lastPoll = -10;

  // ---- playback & practice ----
  enum Grade : char { None, Good, Miss, Octave };
  enum Mode : int { Wait, PlayAlong };
  Audio audio;
  std::string practiceFor;      // song id the state below belongs to
  float bpm = 100;
  bool listen = false;
  int mode = Wait;
  std::vector<Grade> grades;    // per event
  std::vector<int> order;       // playOrder of the song
  int cursor = -1, cursorSys = -1;
  int playFrom = 0;             // order index playback started at
  int lastSel = -1;
  int waitK = 0;                // position in `order` while waiting for the player
  double stable = 0;
  bool needRelease = false;
  int dropFrames = 0;
  float held = 0;               // mic level while a note is held, to spot tongued re-articulation
  size_t gradeVi = 0;
  int frames = 0, hits = 0, octs = 0;
  float heard = 0;              // written MIDI heard on the mic (fractional), 0 = nothing
  double silentFor = 0;

  // ---- Find my range (#87, rangeWindow): a take of the lowest, then the highest comfortable note ----
  bool showRange = false;
  Instrument rangeFrom;              // the voice the test started on: My voice gets its octave, techniques and sound
  int rangeTake = -1;                // the take being recorded: 0 lowest, 1 highest; -1 none
  long long rangeMark = 0, rangeLast = 0;  // mic samples when Sing was pressed, at the last recorded frame
  double rangeClick = 0;             // when Sing was pressed: no audio for a while means the mic is blocked
  std::vector<float> rangeHz;        // the take's pitch, one per frame (0 = none)
  std::optional<int> rangeNote[2];   // each take's note, sounding MIDI (sungNote)
  std::string rangeError;
};

// Drawing with the music and text fonts, sizes in staff spaces: the score and Find my range's staff.
struct Score {
  ImDrawList* dl;
  float sp;
  ImU32 ink = IM_COL32(20, 20, 20, 255);

  void glyph(unsigned cp, float x, float y, ImU32 col) const {  // (x, y) = SMuFL glyph origin (baseline)
    float size = sp * 4 * gMusicScale;  // SMuFL: 1 em = 4 staff spaces
    std::string s = utf8(cp);
    dl->AddText(gMusic, size, ImVec2(x, y - gMusic->GetFontBaked(size)->Ascent), col, s.c_str());
  }
  float glyphW(unsigned cp) const { return gMusic->CalcTextSizeA(sp * 4 * gMusicScale, FLT_MAX, 0, utf8(cp).c_str()).x; }
  void text(const std::string& t, float cx, float top, float size, ImU32 col) const {
    ImVec2 sz = gText->CalcTextSizeA(size, FLT_MAX, 0, t.c_str());
    dl->AddText(gText, size, ImVec2(cx - sz.x / 2, top), col, t.c_str());
  }
  void line(float x0, float y0, float x1, float y1, float w, ImU32 col) const { dl->AddLine({x0, y0}, {x1, y1}, col, w); }
};

// gui_panels.cpp: the main window (song list, details panel) and the open song
fs::path songPath(const std::string& id);
void open(App& a, const std::string& id);
void poll(App& a);
const Original* songList(App& a);  // the left column; returns the original image to show instead of the details (web)
void songArea(App& a, const Original* original);  // transport, score, and the details panel or the original image

// gui_score.cpp
void drawScore(App& a, Song& s);

// gui_practice.cpp: playback, the mic, wait-for-me and play-along
void practiceUpdate(App& a, float dt);
void transportBar(App& a);

// gui_settings.cpp: Settings, the instrument editor, Find my range
void instrumentCombo(const char* label, std::string& v, const char* none, const std::string& whistle = "D",
                     const std::vector<std::string>& ids = instrumentIds());
void settingsWindow(App& a);
void instrumentEditor(App& a);
void rangeWindow(App& a);

// gui_import.cpp: Import song (ABC, MusicXML; images and PDFs on the web), Import audio (desktop)
void abcWindow(App& a);
#ifdef __EMSCRIPTEN__
void originalPanel(App& a, const Original& o);
#else
extern std::string gDropped;  // a file dropped on the window (GLFW drop callback), for Import audio
void openImport(App& a);
void importWindow(App& a);
#endif
