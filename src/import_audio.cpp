// Audio files to songs: transcribeFile() (with Demucs separation), Import audio, and the saved pitch tracks.
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // CreateProcessW
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace john {

// ---------------- audio import ----------------

// Runs argv[0] (found on PATH, no shell, so arguments are passed as they are) with stdin from the null device and
// stdout + stderr into `log`: nothing it prints may reach the MCP server's stdout, nor may it read the server's stdin.
// Returns its exit status, or -1 if it couldn't be started.
static int runQuiet(const std::vector<std::string>& argv, const fs::path& log) {
#if defined(_WIN32)
  auto wide = [](const std::string& u) {
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), w.data(), (int)w.size());
    return w;
  };
  std::wstring cmd;  // quoted so that CommandLineToArgvW / the C runtime split it back into argv
  for (auto& a : argv) {
    cmd += cmd.empty() ? L"\"" : L" \"";
    size_t bs = 0;
    for (wchar_t c : wide(a)) {
      if (c == L'\\') { bs++; continue; }
      cmd.append(c == L'"' ? bs * 2 + 1 : bs, L'\\');
      cmd += c;
      bs = 0;
    }
    cmd.append(bs * 2, L'\\');
    cmd += L'"';
  }
  SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
  HANDLE out = CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, nullptr);
  HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = in;
  si.hStdOutput = si.hStdError = out;
  PROCESS_INFORMATION pi{};
  BOOL ok = out != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE &&
            CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
  if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
  if (!ok) return -1;
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return (int)code;
#elif defined(__EMSCRIPTEN__)
  (void)argv, (void)log;
  return -1;
#else
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&fa, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fa, 1, 2);
  std::vector<char*> args;
  for (auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  pid_t pid;
  int err = posix_spawnp(&pid, args[0], &fa, nullptr, args.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  if (err) return -1;
  int st = 0;
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
  return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
#endif
}

// Demucs on a 44.1 kHz stereo section: the `stem` it separates, mono at `rate`, or {} if it couldn't (`why` says
// which). It writes <out>/<model>/<input name>/<stem>.wav (and no_<stem>.wav). Temp files are always removed.
static std::vector<float> demucsStem(const std::vector<float>& stereo, const std::string& stem, int rate, std::string& why) {
  const char* env = getenv("JOHN_DEMUCS");
  std::string exe = env && *env ? env : "demucs";
  fs::path dir = fs::temp_directory_path() / ("john-demucs-" + std::to_string(std::random_device{}()));
  fs::create_directories(dir);
  struct Cleanup {
    fs::path d;
    ~Cleanup() { std::error_code ec; fs::remove_all(d, ec); }
  } cleanup{dir};
  fs::path in = dir / "section.wav", out = dir / "out", log = dir / "log.txt";
  writeWav(in.string(), stereo, 44100, 2);
  int st = runQuiet({exe, "--two-stems=" + stem, "-o", out.string(), in.string()}, log);
  auto lastLine = [&] {  // of its output; progress bars end their lines in \r
    std::ifstream f(log, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(f)), {}), last;
    std::istringstream lines(text);
    for (std::string l; std::getline(lines, l, '\n');)
      for (std::istringstream parts(l); std::getline(parts, l, '\r');)
        if (l.find_first_not_of(" \t") != std::string::npos) last = l;
    return last.empty() ? "" : " (" + last.substr(0, 200) + ")";
  };
  const std::string mix = "; transcribed the full mix";
  if (st == -1) {
    why = (env && *env ? "JOHN_DEMUCS (" + exe + ") can't be run" : "demucs not found (pip install demucs, or set JOHN_DEMUCS)") + mix;
    return {};
  }
  if (st != 0) {
    why = "demucs failed with exit status " + std::to_string(st) + lastLine() + mix;
    return {};
  }
  std::error_code ec;
  for (fs::recursive_directory_iterator it(out, ec), e; !ec && it != e; it.increment(ec))
    if (it->path().filename() == stem + ".wav") {
      why = "transcribed the demucs " + stem + " stem";
      return decodeAudio(it->path().string(), rate);
    }
  why = "demucs wrote no " + stem + ".wav" + lastLine() + mix;  // it exits 0 when it can't read a track
  return {};
}

Transcription transcribeFile(std::string path, double start, double end, TranscribeOptions o) {
  if (path.rfind("~/", 0) == 0) path = homeDir() + path.substr(1);
  constexpr int SR = 22050;
  auto section = [&](int rate, int ch) {  // decoded and trimmed to start..end
    std::vector<float> x = decodeAudio(path, rate, ch);
    size_t n = x.size() / ch;
    size_t from = (size_t)std::clamp(start * rate, 0.0, (double)n);
    size_t to = end > 0 ? (size_t)std::clamp(end * rate, (double)from, (double)n) : n;
    return std::vector<float>(x.begin() + from * ch, x.begin() + to * ch);
  };
  if (o.separate.empty()) return transcribe(section(SR, 1), SR, o);
  std::string why;
  std::vector<float> x = demucsStem(section(44100, 2), o.separate, SR, why);  // Demucs works on 44.1 kHz stereo
  Transcription t = transcribe(x.empty() ? section(SR, 1) : x, SR, o);
  t.report = "separation: " + why + "\n" + t.report;
  return t;
}

double trackTick(const PitchTrack& t, double sec) {
  const auto& y = t.sync;
  if (y.size() < 2) return -1;
  size_t k = std::upper_bound(y.begin(), y.end(), sec, [](double v, auto& p) { return v < p.second; }) - y.begin();
  k = std::clamp(k, (size_t)1, y.size() - 1);  // the first or last beat interval beyond the ends
  auto [t0, s0] = y[k - 1];
  auto [t1, s1] = y[k];
  return t0 + (sec - s0) / (s1 - s0) * (t1 - t0);
}

Song importAudio(const std::string& path, double start, double end, const TranscribeOptions& opt) {
  TranscribeOptions o = opt;
  o.title = fs::path(path).stem().string();
  Transcription t = transcribeFile(path, start, end, o);
  Song& s = t.song;
  s.id = uniqueId(s.title);
  s.source = fs::path(path).filename().string();
  s.folder = "";
  s.notes = tr("Automatic transcription");
  std::istringstream report(t.report);
  for (std::string l; std::getline(report, l);)
    if (l.rfind("separation:", 0) == 0 || l.rfind("tempo:", 0) == 0 || l.rfind("meter:", 0) == 0 || l.rfind("key:", 0) == 0 || l.rfind("transposed", 0) == 0) s.notes += "\n" + l;
  // mean salience of each note (tied notes together) from the pitch track; below 0.35, as the report's unclear bars
  std::vector<int> at(s.events.size() + 1, 0), head(s.events.size(), -1);
  for (size_t i = 0, h = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    at[i + 1] = at[i] + (e.kind == Kind::Bar ? 0 : e.len);
    if (e.kind != Kind::Note) continue;
    bool cont = h < i && s.events[h].tie && head[h] >= 0;  // h: the previous note
    head[i] = cont ? head[h] : (int)i;
    h = i;
  }
  std::vector<double> sum(s.events.size(), 0), n(s.events.size(), 0);
  size_t i = 0;
  for (auto& p : t.track.points) {
    double tick = trackTick(t.track, p.time);
    while (i < s.events.size() && at[i + 1] <= tick) i++;
    if (i == s.events.size()) break;
    if (tick >= 0 && head[i] >= 0) sum[head[i]] += p.conf, n[head[i]]++;
  }
  for (size_t k = 0; k < s.events.size(); k++)
    if (n[k] > 0 && sum[k] / n[k] < 0.35) s.events[k].comment = tr("unclear in the recording, check by ear");
  json pts = json::array(), sync = json::array();
  for (auto& p : t.track.points) pts.push_back({p.time, p.midi, p.conf});
  for (auto& [tick, sec] : t.track.sync) sync.push_back({tick, sec});
  fs::create_directories(dataDir() / "tracks");
  writeJson(dataDir() / "tracks" / (s.id + ".json"), {{"sync", sync}, {"points", pts}}, -1);  // before the song: the GUI may open it at once
  saveSong(s);
  return s;
}

std::optional<PitchTrack> loadTrack(const std::string& id) {
  if (id.empty() || id.find('/') != std::string::npos || id.find("..") != std::string::npos) return std::nullopt;
  json j = readJson(dataDir() / "tracks" / (id + ".json"));
  if (!j.is_object()) return std::nullopt;
  try {
    PitchTrack t;
    for (auto& p : j.at("points")) t.points.push_back({p.at(0).get<float>(), p.at(1).get<int>(), p.at(2).get<float>()});
    for (auto& y : j.at("sync")) t.sync.push_back({y.at(0).get<int>(), y.at(1).get<double>()});
    return t;
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

}  // namespace john
