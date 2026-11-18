// Performance order, backing whistle synth and microphone pitch detection. No audio device code here (see audio.cpp).
#include "john.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace john {

static const double TAU = 6.283185307179586;

static bool endingHas(const std::string& list, int pass) {  // "1", "2", "1,3", "1-2"
  int a = 0, b = 0;
  for (size_t i = 0; i < list.size();) {
    int n = sscanf(list.c_str() + i, "%d-%d", &a, &b);
    if (n >= 1 && pass >= a && pass <= (n == 2 ? b : a)) return true;
    size_t comma = list.find(',', i);
    if (comma == std::string::npos) break;
    i = comma + 1;
  }
  return false;
}

std::vector<int> playOrder(const Song& s) {
  std::vector<int> order;
  std::set<size_t> jumped;  // repeat signs already taken once
  size_t start = 0, i = 0;  // start: where the current repeat goes back to
  int pass = 1;
  bool skipping = false, inEnding = false;
  auto newSection = [&](size_t at) { start = at; pass = 1; skipping = inEnding = false; };
  while (i < s.events.size() && order.size() < s.events.size() * 4) {
    const Event& e = s.events[i];
    if (e.kind != Kind::Bar) {
      if (!skipping) order.push_back((int)i);
      i++;
      continue;
    }
    bool repeatEnd = e.bar == ":|" || e.bar == "::";
    if (repeatEnd && !skipping && !jumped.count(i)) {
      jumped.insert(i);
      pass++;
      i = start;
      continue;
    }
    if (!e.ending.empty()) { inEnding = true; skipping = !endingHas(e.ending, pass); }
    else if (e.bar == "|:" || repeatEnd) newSection(i + 1);  // repeat done (or an implied 1st ending closed)
    else if ((e.bar == "||" || e.bar == "|]") && inEnding) newSection(i + 1);
    i++;
  }
  return order;
}

double soundingFreq(int midi, int octave) { return 440.0 * std::pow(2.0, (midi + 12 * octave - 69) / 12.0); }
double writtenMidiFromFreq(double hz, int octave) { return 69 + 12 * std::log2(hz / 440.0) - 12 * octave; }

static bool has(const Event& e, const char* t) { return std::find(e.techniques.begin(), e.techniques.end(), t) != e.techniques.end(); }

Performance perform(const Song& s, double bpm, int fromOrder, bool countIn) {
  Performance p;
  p.octave = instrumentOf(s).octave;
  bpm = std::clamp(bpm, 20.0, 400.0);
  const double secPerTick = 60.0 / bpm / (WHOLE / 4);
  const int bar = meterTicks(s.meter);
  const int beat = compoundMeter(s.meter) ? WHOLE * 3 / 8 : WHOLE / 4;
  auto order = playOrder(s);
  fromOrder = std::clamp(fromOrder, 0, (int)order.size());
  auto pos = positions(s);
  const int off = fromOrder < (int)order.size() ? pos[order[fromOrder]].tick % beat : 0;  // first note's offset into its beat
  if (countIn) {  // one bar of clicks on the same beat grid as the music (pickups start mid-beat)
    p.countIn = (bar + off) * secPerTick;
    for (int t = 0; t < bar; t += beat) p.clicks.push_back({t * secPerTick, t == 0});
  }
  double t = p.countIn;
  int lastEv = -1;  // last note event merged into voices.back()
  for (size_t k = fromOrder; k < order.size(); k++) {
    const Event& e = s.events[order[k]];
    double dur = e.len * secPerTick;
    if (e.kind == Kind::Note) {
      if (!p.voices.empty() && s.events[lastEv].tie && p.voices.back().midi == e.midi &&
          std::abs(p.voices.back().start + p.voices.back().dur - t) < 1e-6) {
        p.voices.back().dur += dur;  // tied: extend the previous note
        lastEv = order[k];
      } else {
        lastEv = order[k];
        Voice v;
        v.ev = order[k];
        v.ord = (int)k;
        v.midi = e.midi;
        v.start = t;
        v.dur = dur;
        v.tongued = has(e, "tongue") || has(e, "accent") || p.voices.empty();
        v.vibrato = has(e, "vibrato");
        // ponytail: grace pitches are approximate (+4 / -2 semitones); real cuts depend on the finger used
        if (has(e, "cut")) v.graces.push_back({0, 4});
        if (has(e, "double_cut")) { v.graces.push_back({0, 4}); v.graces.push_back({0.05, 4}); }
        if (has(e, "tap")) v.graces.push_back({0, -2});
        if (has(e, "roll")) { v.graces.push_back({dur / 3, 4}); v.graces.push_back({dur * 2 / 3, -2}); }
        if (has(e, "short_roll")) { v.graces.push_back({0, 4}); v.graces.push_back({dur / 2, -2}); }
        if (has(e, "cran")) for (int i = 0; i < 3; i++) v.graces.push_back({dur * i / 4, 4 + (i == 1 ? 3 : 0)});
        if (has(e, "slide")) v.graces.push_back({-1, -2});  // -1 marks a slide from 2 semitones below
        if (has(e, "staccato")) v.gap = dur / 2;
        if (has(e, "breath")) v.gap = std::max(v.gap, std::min(0.25, dur * 0.3));
        if (!p.voices.empty()) {  // articulate the previous note if this one is tongued or repeats its pitch
          Voice& prev = p.voices.back();
          bool samePitch = prev.midi == v.midi && v.graces.empty();
          if ((v.tongued || samePitch) && prev.gap < 0.03) prev.gap = 0.03;
        }
        p.voices.push_back(v);
      }
    }
    t += dur;
  }
  // metronome clicks for the whole performance, aligned to bars (pickup bars start mid-bar)
  if (fromOrder < (int)order.size()) {
    int firstTick = pos[order[fromOrder]].tick;
    double barStart = p.countIn - firstTick * secPerTick;
    for (double bt = barStart; bt < t; bt += bar * secPerTick)
      for (int b = 0; b < bar; b += beat) {
        double ct = bt + b * secPerTick;
        if (ct >= p.countIn - off * secPerTick - 1e-6 && ct < t) p.clicks.push_back({ct, b == 0});
      }
  }
  p.length = t;
  return p;
}

void Synth::render(const Performance& p, long long& pos, float* out, int frames) {
  const double dt = 1.0 / sampleRate;
  for (int i = 0; i < frames; i++, pos++) {
    double t = pos * dt;
    while (vi < p.voices.size() && p.voices[vi].start + p.voices[vi].dur <= t) vi++;  // forward only: new Synth per playback
    while (ci < p.clicks.size() && p.clicks[ci].first + 0.03 <= t) ci++;
    double target = 0, freq = 0, breathNoise = 0;
    if (vi < p.voices.size() && p.voices[vi].start <= t) {
      const Voice& v = p.voices[vi];
      double into = t - v.start;
      if (into < v.dur - v.gap) {
        target = 1;
        double semis = 0;
        for (auto [at, g] : v.graces) {
          if (at < 0 && into < 0.12) semis = g * (1 - into / 0.12);  // slide up
          else if (at >= 0 && into >= at && into < at + 0.035) semis = g;
        }
        if (v.vibrato && into > 0.2) semis += 0.25 * std::sin(TAU * 5.5 * into);
        freq = soundingFreq(v.midi, p.octave) * std::pow(2.0, semis / 12);
        if (v.tongued && into < 0.012) breathNoise = 0.5;
      }
    }
    // amplitude follows the target: fast attack, slower release; legato notes keep the amplitude
    double rate = target > amp ? 0.004 : 0.0015;
    amp += (target - amp) * std::min(1.0, dt / rate);
    if (freq > 0) phase += TAU * freq * dt;
    if (phase > TAU) phase = std::fmod(phase, TAU);
    rng = rng * 1664525u + 1013904223u;
    double white = (rng >> 8) / double(1 << 24) * 2 - 1;
    noise += (white - noise) * 0.2;  // softened breath noise
    double tone = std::sin(phase) + 0.18 * std::sin(2 * phase) + 0.06 * std::sin(3 * phase);
    double sample = melody ? amp * (0.8 * tone + 0.06 * noise) + breathNoise * noise : 0;
    if (metronome && ci < p.clicks.size() && t >= p.clicks[ci].first) {
      double ct = t - p.clicks[ci].first;
      sample += 0.5 * std::exp(-ct * 150) * std::sin(TAU * (p.clicks[ci].second ? 2000 : 1400) * ct);
    }
    out[i] = float(sample * volume * 0.35);
  }
}

float rms(const float* x, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += x[i] * x[i];
  return n ? float(std::sqrt(s / n)) : 0;
}

float detectPitch(const float* x, int n, int sr, float fmin, float fmax) {
  int tauMin = std::max(2, int(sr / fmax)), tauMax = std::min(int(sr / fmin), n / 2);
  if (tauMax <= tauMin + 2) return 0;
  int w = n - tauMax - 1;
  std::vector<float> d(tauMax + 2, 0);
  for (int tau = 1; tau <= tauMax + 1; tau++) {
    double sum = 0;
    for (int j = 0; j < w; j++) { double v = x[j] - x[j + tau]; sum += v * v; }
    d[tau] = float(sum);
  }
  // cumulative mean normalized difference
  double running = 0;
  d[0] = 1;
  for (int tau = 1; tau <= tauMax + 1; tau++) {
    running += d[tau];
    d[tau] = running > 0 ? float(d[tau] * tau / running) : 1;
  }
  int best = -1;
  for (int tau = tauMin; tau <= tauMax; tau++)
    if (d[tau] < 0.15f) {
      while (tau + 1 <= tauMax && d[tau + 1] < d[tau]) tau++;
      best = tau;
      break;
    }
  if (best < 0) return 0;
  double a = d[best - 1], b = d[best], c = d[best + 1];
  double shift = (a - 2 * b + c) != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0;
  return float(sr / (best + std::clamp(shift, -1.0, 1.0)));
}

}  // namespace john
