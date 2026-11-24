// Viewer language: tr()/trf() lookups, language files, the system language.
#include "john.hpp"
#include "internal.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // GetUserPreferredUILanguages
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace john {

bool gLocalized = false;  // a language other than English is loaded

// ---------------- viewer language ----------------

static std::map<std::string, std::string, std::less<>> gTr;  // English -> current language; empty = English
static std::string gLang;  // code of the loaded language; empty = English

fs::path i18nDir() {
#ifdef JOHN_I18N
  return bundled("i18n", JOHN_I18N);
#else
  return {};
#endif
}

json loadStrings(const fs::path& dir, std::vector<std::string>* problems) {
  std::vector<fs::path> files;
  std::error_code ec;
  for (auto& f : fs::directory_iterator(dir, ec))
    if (f.path().extension() == ".json") files.push_back(f.path());
  std::sort(files.begin(), files.end());  // the file system's order varies
  json r;
  std::map<std::string, std::string> from;  // key -> the file that defines it
  for (auto& f : files) {
    const std::string name = f.filename().string();
    std::ifstream in(f);
    json j = json::parse(in, [&](int depth, json::parse_event_t e, json& key) {
      if (e == json::parse_event_t::key && depth == 1) {
        auto [it, fresh] = from.try_emplace(key.get<std::string>(), name);
        if (!fresh && problems) problems->push_back("\"" + it->first + "\" is in " + it->second + " and " + name);
      }
      return true;
    }, false);
    if (!j.is_object()) {
      if (problems) problems->push_back(name + " is not a JSON object");
      continue;
    }
    r.update(j);
  }
  return r;
}

bool setLanguage(const std::string& code) {
  gTr.clear();
  std::vector<std::string> problems;
  json j = loadStrings(i18nDir() / code, &problems);
  for (auto& p : problems) fprintf(stderr, "John: i18n/%s: %s\n", code.c_str(), p.c_str());
  if (j.is_object())  // else "en", unknown or unreadable: English
    for (auto& [k, v] : j.items())
      if (v.is_string() && !v.get_ref<const std::string&>().empty()) gTr[k] = v.get<std::string>();
  gLang = gTr.empty() ? "" : code;
  return gLocalized = !gTr.empty();
}

std::vector<std::pair<std::string, std::string>> languages() {
  std::vector<std::pair<std::string, std::string>> r;
  std::error_code ec;
  for (auto& f : fs::directory_iterator(i18nDir(), ec)) {
    if (!f.is_directory(ec)) continue;
    std::string code = f.path().filename().string();
    json j = loadStrings(f.path());
    auto name = j.is_object() ? j.find("@language") : j.end();
    r.push_back({code, j.is_object() && name != j.end() && name->is_string() ? name->get<std::string>() : code});
  }
  std::sort(r.begin(), r.end());
  return r;
}

std::string systemLanguage() {
#ifdef __EMSCRIPTEN__
  std::string want = emscripten_run_script_string("navigator.language || ''");
#else
  const char* v = std::getenv("LC_ALL");
  if (!v || !*v) v = std::getenv("LANG");
  std::string want = v ? v : "";
#ifdef __APPLE__
  if (want.empty())  // started from Finder/Dock: no LANG, use System Settings > Language & Region
    if (CFArrayRef pref = CFLocaleCopyPreferredLanguages()) {
      char buf[64];
      if (CFArrayGetCount(pref) > 0 &&
          CFStringGetCString((CFStringRef)CFArrayGetValueAtIndex(pref, 0), buf, sizeof buf, kCFStringEncodingUTF8))
        want = buf;  // "pt-BR", "en-US", "zh-Hans-CN"
      CFRelease(pref);
    }
#elif defined(_WIN32)
  ULONG n = 0, size = 64;
  wchar_t buf[64];  // "pt-BR\0en-US\0\0": the display languages, first is the one in use
  if (want.empty() && GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &n, buf, &size) && n) want = fs::path(buf).string();
#endif
#endif
  return matchLanguage(want);
}

std::string matchLanguage(std::string want) {
  want = want.substr(0, want.find('.'));  // pt_BR.UTF-8 -> pt-BR
  std::replace(want.begin(), want.end(), '_', '-');
  auto langs = languages();
  for (auto& l : langs) if (l.first == want) return l.first;
  auto base = [](const std::string& c) { return c.substr(0, c.find('-')); };  // pt-PT -> the pt-BR file
  for (auto& l : langs) if (base(l.first) == base(want)) return l.first;
  return "en";
}

const char* tr(const char* english) {
  auto it = gTr.find(std::string_view(english));
  return it != gTr.end() ? it->second.c_str() : english;
}

std::string tr(const std::string& english) { return tr(english.c_str()); }

std::string songText(const Song& s, const std::string& english) {
  auto l = s.i18n.find(gLang);
  if (l == s.i18n.end()) return english;
  auto t = l->second.find(english);  // keyed by the exact English: an edited text simply stops matching
  return t != l->second.end() && !t->second.empty() ? t->second : english;
}

std::string trf(const char* english, ...) {
  const char* f = tr(english);
  va_list ap, ap2;
  va_start(ap, english);
  va_copy(ap2, ap);
  int n = std::max(0, vsnprintf(nullptr, 0, f, ap));
  std::string r(n, '\0');
  vsnprintf(r.data(), n + 1, f, ap2);
  va_end(ap2);
  va_end(ap);
  return r;
}

}  // namespace john
