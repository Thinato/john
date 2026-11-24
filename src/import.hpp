// ABC and MusicXML import (abc.cpp, musicxml.cpp).
#pragma once
#include "song.hpp"

namespace john {

struct ImportResult { Song song; std::vector<std::string> warnings; };
ImportResult importAbc(const std::string& abc);
// MusicXML (musicxml.cpp): .musicxml/.xml text or the bytes of a zipped .mxl. The first part's first voice, chords
// reduced to their top note, one key and meter (the first); what is dropped or rounded becomes a warning. Throws when
// the file can't be read or has no notes.
ImportResult importMusicXml(const std::string& data);
inline constexpr size_t MXL_MAX_ENTRY = 64u << 20;  // bigger .mxl entries are refused before unzipping (zip bombs)
bool isMusicXml(const std::string& data);         // an .mxl (zip) or text with a <score-partwise>/<score-timewise> root
// Image import (#75): copisteria sometimes misses the key signature of the first line only. In the reader's MusicXML
// (lines start at <print new-system/new-page>): the first line's key differs from one key that starts on line 2 and
// holds to the end. Only when the first line has no sharps or flats, and no written accidental that the later key would
// make unneeded (a sign of a real key change), is the first line given the later key (`fixed`): its notes without a
// written accidental take the key's sharps or flats, and `xml` is rewritten. nullopt: nothing to report.
struct FirstLineKey { int first = 0, rest = 0; bool fixed = false; };  // fifths of the first line and of the rest
std::optional<FirstLineKey> checkFirstLineKey(std::string& xml);
std::string graceTechnique(const std::vector<int>& graces, int midi);  // grace pitches before a note: cut, tap, roll, double_cut
// Import MusicXML (isMusicXml) as one song, or every tune (X:) of ABC, as new saved songs: instrument, whistle, harp and
// folder from `like`, source too when set (else the ABC's C:/S:/Z:, the MusicXML composer). Ids come from
// uniqueId(title); the first song's from like.id when set (slug(like.id) as is with overwrite). Warnings name their
// tune when there are several. An ABC tune needs a K: line and notes; throws if none has. Used by import_abc,
// import_musicxml and the viewer's Import song.
std::vector<ImportResult> importSongs(const std::string& data, const Song& like, bool overwrite = false);

}  // namespace john
