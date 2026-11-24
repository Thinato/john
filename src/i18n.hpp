// Viewer language (i18n.cpp).
#pragma once
#include "song.hpp"

namespace john {

// ---- viewer language (gettext style: the English text is the key) ----
// Only the GUI calls setLanguage()/setSolfege(); until then tr() returns its argument and note names are letters,
// so the MCP server, which never calls them, stays English.
fs::path i18nDir();                                 // i18n/ next to the executable, else the repo's: one <code>/ folder per language
bool setLanguage(const std::string& code);          // "en" or a folder in i18nDir(), e.g. "pt-BR"; false = English
std::string systemLanguage();                       // LC_ALL/LANG (navigator.language on the web; on macOS, if both are empty,
                                                    // the preferred system language) matched to a folder; "en" if none
std::string matchLanguage(std::string want);        // "pt_BR.UTF-8", "pt-PT" -> "pt-BR" (a folder in i18nDir()); "en" if none
std::vector<std::pair<std::string, std::string>> languages();  // (code, own name) of every folder in i18nDir()
// One language's strings: every .json file in `dir` (i18nDir() / code) merged, in file name order. A key defined twice
// (in two files, or twice in one) and a file that isn't a JSON object are added to `problems`. null if there are none.
json loadStrings(const fs::path& dir, std::vector<std::string>* problems = nullptr);
const char* tr(const char* english);                // translation, or `english` when there is none
std::string tr(const std::string& english);
#ifdef __GNUC__
#define JOHN_PRINTF_ARGS __attribute__((format(printf, 1, 2)))  // GCC/Clang check trf()'s arguments
#else
#define JOHN_PRINTF_ARGS  // MSVC
#endif
std::string trf(const char* english, ...) JOHN_PRINTF_ARGS;  // tr() then printf
std::string songText(const Song& s, const std::string& english);  // s.i18n entry for the current language, else english
// notes: noteName() says Do Re Mi instead of C D E. keys: keyName() does ("Ré maior"), but only in a language other
// than English; English keys are always letters ("D major").
void setSolfege(bool notes, bool keys);

}  // namespace john
