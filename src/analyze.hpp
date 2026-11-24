// Range, playability and technique hints for a song on an instrument (analyze.cpp).
#pragma once
#include "catalog.hpp"

namespace john {

// Notes out of range (or impossible) and hard notes (in's note table) if s were transposed by semis. Written pitches are
// compared, whatever instrument the song is for. half/cross count hard notes played by half-holing / cross-fingering,
// other the rest (bends, overblows, ...). hard() weighs them all, 10 per weight-1 note: a whistle half-hole is 10, its
// cross-fingered C 2. weight defaults to that formula so a literal RangeCost{out, half, cross} means what it did.
struct RangeCost {
  int out = 0, half = 0, cross = 0;
  int weight = half * 10 + cross * 2;
  int other = 0;
  int hard() const { return weight; }
};
RangeCost rangeCost(const Song& s, const Instrument& in, int semis);
// Play the song in the key whose tonic is pitch class `tonic` (same mode): transposes by the smallest interval, then
// moves whole octaves to the octave that fits `in` best (rangeCost: fewest out, then hard()). On a tie, the octave
// closest to the original sheet (s.transposed nearest 0), so the original key restores it exactly. Returns the shift.
int transposeToKey(Song& s, int tonic, const Instrument& in);
// The share of notes, in percent, that makes a song hard to play: hard notes (RangeCost::hard() / 10, so a half-hole
// counts 1 and a cross-fingered C 1/5) at or above it and the song doesn't fit (playability); out-of-range notes at or
// above it and Import song points to Play in key.
constexpr int PLAYABLE_PERCENT = 10;
bool fits(const RangeCost& c, int notes);  // no note out of range and hard notes under PLAYABLE_PERCENT
// Can `in` play the song (written pitches, see rangeCost)? AsWritten: it fits at shift 0. Otherwise every key in the
// same mode is tried at its best octave (as transposeToKey); the best is the one with fewest notes out, then one that
// fits, then the nearest key (the same key an octave away first), then fewest hard notes. OtherKey: transposing by
// `shift` fits, `key` is the key after it (the same key when shift is a whole octave). NoFit: nothing fits, `cost.out`
// notes stay out at best.
struct Playability {
  enum Verdict { AsWritten, OtherKey, NoFit } verdict = AsWritten;
  int shift = 0;
  Key key;
  RangeCost cost;  // at shift
  int notes = 0;
};
Playability playability(const Song& s, const Instrument& in);
// Deterministic technique candidates + range report (on the song's instrument). The candidates are for `player`
// (default the song's instrument): marks it lists as they are, equivalents named as such, hidden ones left out (#86).
std::string analyze(const Song& s, const std::optional<Instrument>& player = std::nullopt);

}  // namespace john
