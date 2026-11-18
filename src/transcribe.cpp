// Audio -> melody: predominant pitch (harmonic summation + Viterbi), beat tracking (Ellis DP), quantization to
// a sixteenth grid, key detection (Krumhansl), octave fit to the instrument. Works on solo lines and on mixes where the
// melody is the strongest pitched voice.
#include "john.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <map>
#include <numeric>
#include <stdexcept>

namespace john {

std::vector<float> decodeAudio(const std::string& path, int sampleRate) {
  ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, (ma_uint32)sampleRate);
  ma_decoder dec;
  if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS)
    throw std::runtime_error("can't open or decode '" + path + "' (mp3, wav or flac)");
  std::vector<float> out;
  float buf[8192];
  for (;;) {
    ma_uint64 got = 0;
    ma_result r = ma_decoder_read_pcm_frames(&dec, buf, 8192, &got);
    out.insert(out.end(), buf, buf + got);
    if (r != MA_SUCCESS || got == 0) break;
  }
  ma_decoder_uninit(&dec);
  return out;
}

static void fft(std::vector<std::complex<float>>& a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; i++) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2 * M_PI / (double)len;
    for (size_t k = 0; k < len / 2; k++) {
      const std::complex<float> w((float)std::cos(ang * k), (float)std::sin(ang * k));
      for (size_t i = 0; i < n; i += len) {
        auto u = a[i + k], v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
      }
    }
  }
}

namespace {
constexpr int LO = 48, HI = 100;  // melody search range, sounding MIDI (C3..E7; a D whistle reaches D7)
constexpr int M = HI - LO + 1;   // semitone states
constexpr int CENTS = 10;        // fine salience resolution
constexpr int NB = (HI - LO) * 100 / CENTS + 1;
constexpr int HARMONICS = 8;

struct RawNote { int a, b, m; double conf; };  // frames [a, b), sounding MIDI, mean salience
struct QNote { int q0, q1, m; double conf; };  // grid units [q0, q1)

const int PC_FIFTHS[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};

Key detectKey(const std::vector<QNote>& notes, double* corrOut) {
  static const double MAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
  static const double MIN[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
  double h[12] = {};
  for (auto& n : notes) h[n.m % 12] += n.q1 - n.q0;
  auto corr = [&](const double* prof, int tonic) {
    double mx = 0, my = 0;
    for (int i = 0; i < 12; i++) { mx += h[i] / 12; my += prof[i] / 12; }
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < 12; i++) {
      double dx = h[(i + tonic) % 12] - mx, dy = prof[i] - my;
      sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
    }
    return sxx > 0 ? sxy / std::sqrt(sxx * syy) : 0;
  };
  Key best{0, "maj"};
  double bc = -2;
  for (int t = 0; t < 12; t++) {
    if (double c = corr(MAJ, t); c > bc) { bc = c; best = {PC_FIFTHS[t], "maj"}; }
    if (double c = corr(MIN, t); c > bc) { bc = c; best = {PC_FIFTHS[(t + 3) % 12], "min"}; }
  }
  *corrOut = bc;
  return best;
}

// Split a span of one bar into note values a learner can read (whole .. sixteenth, dotted allowed), tied in the draft,
// so the beats stay visible (#3): a value starting off the beat stops at the next beat; one on a beat may span whole
// beats (or a beat and a half in simple time: the dotted quarter), but not across the middle of a 4-beat bar unless
// it starts the bar. pos = sixteenths into the bar, beat and bar in sixteenths.
std::vector<int> pieces(int pos, int len, int beat, int bar, bool compound) {
  static const int V[] = {16, 12, 8, 6, 4, 3, 2, 1};
  const int mid = bar / beat >= 4 && bar / beat % 2 == 0 ? bar / 2 : 0;
  std::vector<int> out;
  for (const int end = pos + len; pos < end;) {
    const bool onBeat = pos % beat == 0;
    const int room = onBeat ? end - pos : std::min(end - pos, beat - pos % beat);
    for (int v : V) {
      if (v > room) continue;
      if (onBeat && v > beat && ((v % beat && (compound || 2 * v != 3 * beat)) || (pos && pos < mid && pos + v > mid)))
        continue;
      out.push_back(v);
      pos += v;
      break;
    }
  }
  return out;
}
}  // namespace

Transcription transcribe(const std::vector<float>& x, int sr, const TranscribeOptions& o) {
  int N = 1;
  while (N < sr * 0.09) N <<= 1;  // ~93 ms window: resolves semitones down to C3
  const int HOP = N / 8;
  const double hopSec = (double)HOP / sr;
  if ((long long)x.size() < (long long)N * 4) throw std::runtime_error("audio is too short to transcribe");
  const int T = 1 + (int)((x.size() - N) / HOP);
  if (!o.meter.empty() && parseMeter(o.meter) != o.meter)  // callers pass the normalized "N/D"
    throw std::runtime_error("unsupported meter '" + o.meter + "'");
  // pitch search range (semitone states): the recording's solo instrument sounding range, two semitones margin
  int sLo = 0, sHi = M - 1;
  std::optional<Instrument> rec;
  if (!o.recordingInstrument.empty()) {
    rec = findInstrument(o.recordingInstrument, "D");
    if (!rec) throw std::runtime_error("unknown recording instrument '" + o.recordingInstrument + "'");
    sLo = std::clamp(rec->low + 12 * rec->octave - 2 - LO, 0, M - 1);
    sHi = std::clamp(rec->high + 12 * rec->octave + 2 - LO, 0, M - 1);
  }

  // ---- spectrum: onset envelope + fine harmonic salience ----
  std::vector<float> win(N), flux(T, 0.f), prevLog(N / 2 + 1, 0.f);
  for (int n = 0; n < N; n++) win[n] = 0.5f - 0.5f * (float)std::cos(2 * M_PI * n / N);
  std::vector<float> fine((size_t)T * NB, 0.f);
  std::vector<std::complex<float>> buf(N);
  std::vector<float> mag(N / 2 + 1);
  const int kmax = std::min(N / 2 - 1, (int)(5000.0 * N / sr));
  for (int t = 0; t < T; t++) {
    for (int n = 0; n < N; n++) buf[n] = x[(size_t)t * HOP + n] * win[n];
    fft(buf);
    float top = 0;
    for (int k = 0; k <= N / 2; k++) {
      mag[k] = std::abs(buf[k]);
      top = std::max(top, mag[k]);
      float l = std::log1p(100.f * mag[k]);
      if (t > 0) flux[t] += std::max(0.f, l - prevLog[k]);
      prevLog[k] = l;
    }
    if (top <= 1e-6f) continue;
    // spectral peaks within 40 dB of the loudest, strongest 40
    std::vector<std::pair<float, float>> peaks;  // (magnitude, Hz)
    for (int k = 2; k < kmax; k++) {
      if (mag[k] <= mag[k - 1] || mag[k] < mag[k + 1] || mag[k] < top * 0.01f) continue;
      double a = std::log(mag[k - 1] + 1e-9), b = std::log(mag[k] + 1e-9), c = std::log(mag[k + 1] + 1e-9);
      double d = a - 2 * b + c != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0;
      peaks.push_back({mag[k], (float)((k + d) * sr / N)});
    }
    if (peaks.size() > 40) {
      std::partial_sort(peaks.begin(), peaks.begin() + 40, peaks.end(), std::greater<>());
      peaks.resize(40);
    }
    float* f = &fine[(size_t)t * NB];
    for (auto [a, hz] : peaks) {
      double w = 1;
      for (int h = 1; h <= HARMONICS; h++, w *= 0.8) {
        double cents = 1200 * std::log2(hz / h / 440.0) + 6900 - LO * 100;
        double bin = cents / CENTS;
        if (bin < -10) break;
        if (bin > NB + 10) continue;
        for (int b = (int)std::ceil(bin - 10); b <= (int)std::floor(bin + 10); b++) {
          if (b < 0 || b >= NB) continue;
          double cw = std::cos(M_PI / 2 * (b - bin) / 10);
          f[b] += (float)(a * w * cw * cw);
        }
      }
    }
  }

  // ---- tuning offset (recordings aren't always A440) ----
  double sx = 0, sy = 0;
  for (int t = 0; t < T; t++) {
    const float* f = &fine[(size_t)t * NB];
    int b = (int)(std::max_element(f, f + NB) - f);
    double ang = 2 * M_PI * ((b * CENTS) % 100) / 100.0;
    sx += f[b] * std::cos(ang);
    sy += f[b] * std::sin(ang);
  }
  double tune = std::atan2(sy, sx) / (2 * M_PI) * 100;  // -50..50 cents

  // ---- semitone salience ----
  std::vector<float> S((size_t)T * M, 0.f), frameMax(T, 0.f);
  for (int t = 0; t < T; t++)
    for (int m = 0; m < M; m++) {
      int c = (int)std::lround((m * 100 + tune) / CENTS);
      float v = 0;
      for (int b = c - 4; b <= c + 4; b++) if (b >= 0 && b < NB) v = std::max(v, fine[(size_t)t * NB + b]);
      S[(size_t)t * M + m] = v;
      frameMax[t] = std::max(frameMax[t], v);
    }
  fine.clear();
  fine.shrink_to_fit();
  // A note's lower octave collects ~0.8 of its salience through the even harmonics; subtract part of the octave
  // above so accompaniment can't tip the melody down an octave. Only up to C6, as when the search stopped at C7:
  // suppressing C#6..E6 by the D7..E7 states (their own 2nd harmonics) put whistle notes an octave off (#40).
  for (int t = 0; t < T; t++) {
    float* row = &S[(size_t)t * M];
    frameMax[t] = 0;
    for (int m = 0; m < M; m++) {
      if (m + 12 <= 96 - LO) row[m] = std::max(0.f, row[m] - 0.5f * row[m + 12]);
      if (m < sLo || m > sHi) row[m] = 0;  // outside the recording instrument's range
      frameMax[t] = std::max(frameMax[t], row[m]);
    }
  }
  std::vector<float> sorted = frameMax;
  std::nth_element(sorted.begin(), sorted.begin() + T * 9 / 10, sorted.end());
  const float norm = sorted[T * 9 / 10];
  if (norm <= 0) throw std::runtime_error("no pitched sound found");

  // ---- melody line: Viterbi over semitones + an unvoiced state ----
  // ponytail: fixed weights tuned by ear on a few songs; a learned voicing model would do better on dense mixes
  const float VOICE = 0.25f, CHANGE = 0.15f, STEP = 0.01f, ONOFF = 0.3f;
  std::vector<float> score(M + 1, 0.f), next(M + 1);
  std::vector<unsigned char> back((size_t)T * (M + 1));
  auto emit = [&](int t, int s) { return s == M ? VOICE : std::min(2.f, S[(size_t)t * M + s] / norm); };
  for (int s = 0; s <= M; s++) score[s] = emit(0, s);
  for (int t = 1; t < T; t++) {
    for (int j = 0; j <= M; j++) {
      float best = -1e30f;
      int arg = 0;
      for (int i = 0; i <= M; i++) {
        float c = score[i];
        if (i != j) c -= (i == M || j == M) ? ONOFF : CHANGE + STEP * std::abs(i - j);
        if (c > best) { best = c; arg = i; }
      }
      next[j] = best + emit(t, j);
      back[(size_t)t * (M + 1) + j] = (unsigned char)arg;
    }
    float mx = *std::max_element(next.begin(), next.end());
    for (int j = 0; j <= M; j++) score[j] = next[j] - mx;  // keep numbers small
  }
  std::vector<int> path(T);
  path[T - 1] = (int)(std::max_element(score.begin(), score.end()) - score.begin());
  for (int t = T - 1; t > 0; t--) path[t - 1] = back[(size_t)t * (M + 1) + path[t]];

  if (const char* dbg = getenv("JOHN_TRANSCRIBE_DEBUG")) {  // per-frame pitch track for tuning the tracker
    if (FILE* f = fopen(dbg, "w")) {
      for (int t = 0; t < T; t++) {
        std::vector<std::pair<float, int>> top;
        for (int m = 0; m < M; m++) top.push_back({S[(size_t)t * M + m] / norm, m + LO});
        std::partial_sort(top.begin(), top.begin() + 3, top.end(), std::greater<>());
        fprintf(f, "%.3f %d %.2f %d:%.2f %d:%.2f %d:%.2f %.2f\n", (t * HOP + N / 2.0) / sr, path[t] == M ? 0 : path[t] + LO,
                path[t] == M ? 0 : S[(size_t)t * M + path[t]] / norm, top[0].second, top[0].first, top[1].second, top[1].first,
                top[2].second, top[2].first, flux[t]);
      }
      fclose(f);
    }
  }

  // ---- notes: runs of one pitch, drop blips, split re-articulated repeats at salience dips ----
  const int minF = std::max(2, (int)std::ceil(0.06 / hopSec));
  std::vector<RawNote> raw;
  for (int t = 0; t < T;) {
    int u = t;
    while (u < T && path[u] == path[t]) u++;
    if (path[t] != M) raw.push_back({t, u, path[t], 0});
    t = u;
  }
  std::vector<RawNote> notes;
  for (auto& n : raw) {
    if (n.b - n.a >= minF) {
      if (!notes.empty() && notes.back().b == n.a && notes.back().m == n.m) notes.back().b = n.b;  // blip removed between
      else notes.push_back(n);
    } else if (!notes.empty() && notes.back().b == n.a) notes.back().b = n.b;  // short glide: part of the previous note
  }
  std::vector<RawNote> split;
  for (auto& n : notes) {
    auto v = [&](int t) { return S[(size_t)t * M + n.m]; };
    int start = n.a;
    float before = 0;
    for (int t = n.a; t < n.b; t++) {
      before = std::max(before, v(t));
      if (t - start < minF || n.b - t < minF || v(t) > v(t - 1) || v(t) > v(t + 1)) continue;
      float after = 0;
      for (int u = t + 1; u < std::min(n.b, t + 3 * minF); u++) after = std::max(after, v(u));
      if (v(t) < 0.5f * before && v(t) < 0.5f * after) {
        split.push_back({start, t, n.m, 0});
        start = t;
        before = 0;
      }
    }
    split.push_back({start, n.b, n.m, 0});
  }
  // Melody continuity: compare each note with the duration-weighted median pitch of the notes around it (±2 s).
  // An octave away with energy in the other octave = octave error, move it. Short and well below = accompaniment
  // poking through (a chord attack, a bass note in a melody rest), drop it and give its time to the next note.
  // Unless the next note is back in the melody's range and its pitch is still silent during this one: the melody is
  // monophonic, so that is the melody itself leaping up from a low note (#7). Accompaniment pokes through while the
  // melody sounds over it.
  {
    const int win = (int)(2.0 / hopSec), shortF = (int)(0.3 / hopSec);
    std::vector<RawNote> kept;
    std::vector<int> pitch;
    for (auto& n : split) pitch.push_back(n.m);
    for (size_t i = 0; i < split.size(); i++) {
      RawNote n = split[i];
      std::vector<std::pair<int, int>> around;  // (pitch, frames)
      for (size_t j = 0; j < split.size(); j++)
        if (split[j].b > n.a - win && split[j].a < n.b + win) around.push_back({pitch[j], split[j].b - split[j].a});
      std::sort(around.begin(), around.end());
      int total = 0, acc = 0, ref = n.m;
      for (auto& [m, f] : around) total += f;
      for (auto& [m, f] : around) if ((acc += f) * 2 >= total) { ref = m; break; }
      auto energy = [&](int m) {
        double e = 0;
        if (m < 0 || m >= M) return 0.0;
        for (int t = n.a; t < n.b; t++) e += S[(size_t)t * M + m];
        return e;
      };
      int d = n.m - ref;
      // after the octave-ghost suppression: a real note keeps ~0.85x its salience an octave up (its 2nd harmonic),
      // and ~0.3x an octave down; an octave error reverses those ratios
      if (d > 9 && energy(n.m - 12) >= 0.7 * energy(n.m)) n.m -= 12;
      else if (d < -7) {
        if (energy(n.m + 12) >= 1.5 * energy(n.m)) n.m += 12;
        else if (n.b - n.a < shortF &&
                 !(i + 1 < split.size() && pitch[i + 1] - ref >= -7 && energy(pitch[i + 1]) < 0.25 * energy(n.m))) {
          if (i + 1 < split.size() && split[i + 1].a == n.b) split[i + 1].a = n.a;
          continue;
        }
      }
      kept.push_back(n);
    }
    split = std::move(kept);
  }
  if (split.empty()) throw std::runtime_error("no melody found (try a recording where one instrument or voice leads)");
  for (auto& n : split) {
    double c = 0;
    for (int t = n.a; t < n.b; t++) c += S[(size_t)t * M + n.m] / norm;
    n.conf = c / (n.b - n.a);
  }

  // ---- beats: onset envelope, tempo by autocorrelation, beat times by dynamic programming ----
  std::vector<double> env(T, 0);
  {
    int w = std::max(1, (int)(0.2 / hopSec));
    std::vector<double> pre(T + 1, 0);
    for (int t = 0; t < T; t++) pre[t + 1] = pre[t] + flux[t];
    double sum = 0, sq = 0;
    for (int t = 0; t < T; t++) {
      int a = std::max(0, t - w), b = std::min(T, t + w + 1);
      env[t] = std::max(0.0, flux[t] - (pre[b] - pre[a]) / (b - a));
      sum += env[t];
      sq += env[t] * env[t];
    }
    double sd = std::sqrt(std::max(1e-12, sq / T - (sum / T) * (sum / T)));
    for (auto& e : env) e /= sd;
  }
  // Beat period (frames) by autocorrelation, centred on 110 beats a minute, or 80 for the dotted-quarter beat of 6/8
  struct Tempo { double period = 0, conf = 0, rival = 0, rivalPeriod = 0; };  // peak strength; runner-up peak / best
  auto tempoOf = [&](bool compound) {
    Tempo tp;
    int lo = (int)(60.0 / 220 / hopSec), hi = std::min(T / 2, (int)(60.0 / 40 / hopSec));
    std::vector<double> r(hi + 2, 0), sc(hi + 2, 0);
    for (int L = lo; L <= hi + 1; L++)
      for (int t = 0; t + L < T; t++) r[L] += env[t] * env[t + L];
    double best = -1, r0 = 0;
    int arg = lo;
    for (int t = 0; t < T; t++) r0 += env[t] * env[t];
    double center = compound ? 80 : 110;  // typical beats per minute
    for (int L = lo + 1; L <= hi; L++) {
      double bpm = 60.0 / (L * hopSec);
      sc[L] = r[L] * std::exp(-0.5 * std::pow(std::log2(bpm / center), 2));
      if (sc[L] > best) { best = sc[L]; arg = L; }
    }
    double a = r[arg - 1], b = r[arg], c = r[arg + 1];
    tp.period = arg + (a - 2 * b + c != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0);
    tp.conf = r0 > 0 ? b / r0 : 0;
    // runner-up: the strongest other peak more than ~15% away (often half or double the tempo)
    for (int L = lo + 2; L < hi; L++)
      if (sc[L] >= sc[L - 1] && sc[L] >= sc[L + 1] && std::abs(std::log2((double)L / arg)) > 0.2 && sc[L] > tp.rival * best) {
        tp.rival = best > 0 ? sc[L] / best : 0;
        tp.rivalPeriod = L;
      }
    return tp;
  };
  // Beats, grid and bars for one meter on a beat period. The meter sets the beat level: a quarter in 4/4 and 3/4,
  // a dotted quarter in 6/8 and 9/8 (6 sixteenths a beat).
  struct Fit {
    std::string meter;
    int bpm = 0, rivalBpm = 0, bar = 0, beat = 0, phase = 0;  // bar, beat: grid cells; rivalBpm: runner-up tempo
                                                              // (quarter notes per minute), 0 = none
    double tempoConf = 0, rival = 0, score = 0;
    std::vector<QNote> clean;
  };
  auto fitMeter = [&](const std::string& meter, const Tempo& tp) {
    Fit F;
    F.meter = meter;
    const bool compound = compoundMeter(meter);
    const int den = std::atoi(meter.c_str() + meter.find('/') + 1);
    const int beatTicks = compound ? 3 * WHOLE / den : WHOLE / den;
    const int g = F.beat = beatTicks / 12;  // grid cells (sixteenths) per beat
    const double quarterPerBeat = beatTicks / 48.0;
    const double period = o.bpm > 0 ? 60.0 / (o.bpm / quarterPerBeat) / hopSec : tp.period;  // frames per beat
    F.tempoConf = tp.conf;
    F.rival = tp.rival;
    if (tp.rivalPeriod > 0) F.rivalBpm = (int)std::lround(60.0 / (tp.rivalPeriod * hopSec) * quarterPerBeat);
    std::vector<double> beats;
    {
      std::vector<double> sc(T);
      std::vector<int> prev(T, -1);
      const double TIGHT = 100;
      for (int t = 0; t < T; t++) {
        sc[t] = env[t];
        double best = -1e30;
        for (int tau = t - (int)std::round(2 * period); tau <= t - (int)std::round(period / 2); tau++) {
          if (tau < 0) continue;
          double l = std::log((t - tau) / period);
          double c = sc[tau] - TIGHT * l * l;
          if (c > best) { best = c; prev[t] = tau; }
        }
        if (prev[t] >= 0) sc[t] += best;
      }
      int t = T - 1;
      for (int u = std::max(0, T - (int)period); u < T; u++) if (sc[u] > sc[t]) t = u;
      for (; t >= 0; t = prev[t]) beats.push_back(t);
      std::reverse(beats.begin(), beats.end());
    }
    if (beats.size() < 2) beats = {0, period};
    auto beatPos = [&](double f) {
      size_t k = std::upper_bound(beats.begin(), beats.end(), f) - beats.begin();
      if (k == 0) return (f - beats[0]) / (beats[1] - beats[0]);
      if (k >= beats.size()) { size_t n = beats.size(); return n - 1 + (f - beats[n - 1]) / (beats[n - 1] - beats[n - 2]); }
      return k - 1 + (f - beats[k - 1]) / (beats[k] - beats[k - 1]);
    };
    double beatFrames = (beats.back() - beats.front()) / (beats.size() - 1);
    F.bpm = (int)std::lround(60.0 / (beatFrames * hopSec) * quarterPerBeat);

    // ---- quantize to the grid ----
    std::vector<QNote> q;
    for (auto& n : split) {
      int q0 = (int)std::lround(beatPos(n.a) * g), q1 = (int)std::lround(beatPos(n.b) * g);
      q.push_back({q0, std::max(q0 + 1, q1), n.m + LO, n.conf});
    }
    std::vector<QNote>& clean = F.clean;
    for (auto& n : q) {
      if (!clean.empty()) {
        QNote& p = clean.back();
        if (n.q0 <= p.q0) {  // two notes on one grid point: keep the longer
          if (n.q1 - n.q0 > p.q1 - p.q0) p = n;
          continue;
        }
        if (p.q1 > n.q0 || n.q0 - p.q1 <= 1) p.q1 = n.q0;  // overlaps cut, sixteenth gaps closed (legato reads better)
      }
      clean.push_back(n);
    }

    // ---- bars: downbeat phase where the most note time starts ----
    const int bar = F.bar = meterTicks(meter) / 12;  // grid cells per bar
    int& phase = F.phase;
    std::vector<double> sc(bar, 0);
    for (auto& n : clean) sc[((n.q0 % bar) + bar) % bar] += n.q1 - n.q0;
    for (int p = 0; p < bar; p += g) if (sc[p] > sc[phase]) phase = p;

    // ---- how well the meter fits, each part 0..1 ----
    // subdivision: of the note starts between beats, the share on halves/quarters (simple) or on thirds (compound).
    // Pitch onsets lag the beats (onset flux) by a steady amount: measure it on the notes near a beat first.
    std::vector<double> lag;
    for (auto& n : split)
      if (double d = beatPos(n.a) - std::round(beatPos(n.a)); std::abs(d) < 0.15) lag.push_back(d);
    std::nth_element(lag.begin(), lag.begin() + lag.size() / 2, lag.end());
    const double shift = lag.empty() ? 0 : lag[lag.size() / 2];
    double duple = 0, triple = 0;
    for (auto& n : split) {
      double x = beatPos(n.a) - shift, f = x - std::floor(x);
      if (f < 1.0 / 12 || f > 11.0 / 12) continue;  // on the beat: says nothing about the subdivision
      double d3 = std::min({std::abs(f - 1.0 / 3), std::abs(f - 2.0 / 3)});
      double d4 = std::min({std::abs(f - 0.25), std::abs(f - 0.5), std::abs(f - 0.75)});
      (d3 < d4 ? triple : duple)++;
    }
    double subdiv = duple + triple > 0 ? (compound ? triple : duple) / (duple + triple) : compound ? 0 : 1;
    // long notes on the beats, and on beat 1 of the bar more than on the other beats
    double len = 0, onBeat = 0;
    for (int p = 0; p < bar; p++) len += sc[p], onBeat += p % g ? 0 : sc[p];
    const int B = bar / g;  // beats per bar
    double accent = onBeat > 0 && B > 1 ? (sc[phase] / onBeat - 1.0 / B) / (1 - 1.0 / B) * onBeat / len : 0;
    // repeated motifs: the same pitch one, two or four bars later
    std::vector<int> cell(clean.back().q1 - clean.front().q0, 0);
    for (auto& n : clean) std::fill(cell.begin() + n.q0 - clean.front().q0, cell.begin() + n.q1 - clean.front().q0, n.m);
    double motif = 0;
    for (int k : {1, 2, 4}) {
      int L = k * bar, same = 0, both = 0;
      for (size_t t = 0; t + L < cell.size(); t++)
        if (cell[t] && cell[t + L]) both++, same += cell[t] == cell[t + L];
      if (both) motif = std::max(motif, (double)same / both);
    }
    F.score = (subdiv + onBeat / len + accent + motif) / 4;
    return F;
  };
  // Without a meter, try each one on both tempo searches' beats: the quarter-centred search often locks onto a jig's
  // dotted quarter, the dotted-quarter one onto a waltz's quarter. Keep each meter's better fit.
  std::vector<Fit> fits;
  const Tempo simpleTp = o.bpm > 0 ? Tempo{} : tempoOf(false), compoundTp = o.bpm > 0 ? Tempo{} : tempoOf(true);
  if (!o.meter.empty()) fits.push_back(fitMeter(o.meter, compoundMeter(o.meter) ? compoundTp : simpleTp));
  else
    for (const char* m : {"4/4", "3/4", "6/8", "9/8"}) {
      bool c = compoundMeter(m);
      fits.push_back(fitMeter(m, c ? compoundTp : simpleTp));
      if (o.bpm > 0) continue;
      Fit other = fitMeter(m, c ? simpleTp : compoundTp);
      if (other.score > fits.back().score) fits.back() = std::move(other);
    }
  std::stable_sort(fits.begin(), fits.end(), [](const Fit& a, const Fit& b) { return a.score > b.score; });
  const Fit& F = fits[0];
  std::vector<QNote> clean = F.clean;
  const int bar = F.bar, phase = F.phase, bpm = F.bpm, rivalBpm = F.rivalBpm;
  const double tempoConf = F.tempoConf, rival = F.rival;
  // ---- key, octave fit, events ----
  double keyCorr = 0;
  Key key = detectKey(clean, &keyCorr);
  auto inst = findInstrument(o.instrument, "D");
  if (!inst) throw std::runtime_error("unknown instrument '" + o.instrument + "'");
  const int lo = inst->low, hi = inst->high, wr = -12 * inst->octave;  // written = sounding + wr + 12 * shift
  int shift = 0, bestIn = -1;
  // pick the octave that fits the range best. On a tie the whistle takes the lowest (easier to blow); other
  // instruments take the one nearest the recording (the piano's huge range fits almost any octave).
  const bool lowest = inst->id == "whistle";
  for (int k = -4; k <= 4; k++) {
    int in = 0;
    for (auto& n : clean) in += inst->plays(n.m + wr + 12 * k) * (n.q1 - n.q0);
    if (in > bestIn || (in == bestIn && !lowest && std::abs(k) < std::abs(shift))) { bestIn = in; shift = k; }
  }
  int folded = 0;
  for (auto& n : clean) {  // octave errors and low/high notes the instrument can't play: fold into its range
    int w = n.m + wr + 12 * shift, f = w;
    while (f < lo) f += 12;
    while (f > hi) f -= 12;
    folded += f != w;
    n.m = f - wr - 12 * shift;
  }
  Song s;
  s.title = o.title;
  s.instrument = inst->id;
  s.meter = F.meter;
  s.key = key;
  s.tempo = bpm;
  auto isBarStart = [&](int c) { return (((c - phase) % bar) + bar) % bar == 0; };
  auto emitSpan = [&](int from, int to, int midi, bool note) {
    for (int c = from; c < to;) {
      if (c != clean.front().q0 && isBarStart(c)) { Event b; b.kind = Kind::Bar; b.bar = "|"; s.events.push_back(b); }
      int end = c + 1;
      while (end < to && !isBarStart(end)) end++;
      for (int len : pieces((((c - phase) % bar) + bar) % bar, end - c, F.beat, bar, compoundMeter(F.meter))) {
        Event e;
        e.kind = note ? Kind::Note : Kind::Rest;
        e.midi = note ? midi : 0;
        e.len = len * 12;
        e.tie = note;
        s.events.push_back(e);
      }
      c = end;
    }
    if (note) s.events.back().tie = false;
  };
  int cursor = clean.front().q0;
  for (auto& n : clean) {
    if (n.q0 > cursor) emitSpan(cursor, n.q0, 0, false);
    emitSpan(n.q0, n.q1, n.m + wr + 12 * shift, true);
    cursor = n.q1;
  }
  int endBar = cursor;
  while (!isBarStart(endBar)) endBar++;
  if (endBar > cursor) emitSpan(cursor, endBar, 0, false);
  Event fin;
  fin.kind = Kind::Bar;
  fin.bar = "|]";
  s.events.push_back(fin);

  // ---- report ----
  int voiced = 0;
  for (int p : path) voiced += p != M;
  int firstBar = (int)std::floor((double)(clean.front().q0 - phase) / bar);
  bool pickup = !isBarStart(clean.front().q0);
  std::map<int, std::pair<double, double>> perBar;  // bar -> (confidence * length, length)
  for (auto& n : clean) {
    int b = (int)std::floor((double)(n.q0 - phase) / bar) - firstBar + (pickup ? 0 : 1);
    perBar[b].first += n.conf * (n.q1 - n.q0);
    perBar[b].second += n.q1 - n.q0;
  }
  std::string weak;
  for (auto& [b, v] : perBar) if (v.first / v.second < 0.35) weak += (weak.empty() ? "" : ", ") + std::to_string(b);
  char line[256];
  std::string r;
  snprintf(line, sizeof line, "tempo: %d bpm (quarter notes)", bpm);
  r += line;
  if (o.bpm > 0) r += ", as given";
  else {
    // a steady beat gives one clear autocorrelation peak; a near-equal rival (half, double, 3:2) or a weak peak
    // (irregular onsets, rubato) means the detected tempo may be wrong
    bool low = rival >= 0.85 || tempoConf < 0.3, high = rival < 0.6 && !low;
    r += high ? ", confidence high" : low ? ", confidence low" : ", confidence medium";
    if (!high && rivalBpm) {
      snprintf(line, sizeof line, "; also plausible: ~%d bpm%s", rivalBpm, low ? ", re-run with bpm if the note lengths look wrong" : "");
      r += line;
    }
  }
  r += "\nmeter: " + F.meter;
  if (!o.meter.empty()) r += ", as given";
  else {
    r += " (detected;";
    for (size_t i = 0; i < fits.size(); i++) {
      snprintf(line, sizeof line, "%s %s %.2f", i ? "," : "", fits[i].meter.c_str(), fits[i].score);
      r += line;
    }
    r += ")";
  }
  r += "\n";
  snprintf(line, sizeof line, "key: %s (fit %.2f)\ntuning: %+.0f cents from A440\n", keyName(key).c_str(), keyCorr, tune);
  r += line;
  snprintf(line, sizeof line, "melody heard in %d%% of the audio; %zu notes\n", voiced * 100 / T, clean.size());
  r += line;
  if (shift == 0) r += "octave: the " + inst->name + " plays at the recording's pitch\n";
  else {
    snprintf(line, sizeof line, "octave: the %s plays %d octave(s) %s the recording, to fit its range\n", inst->name.c_str(),
             std::abs(shift), shift > 0 ? "above" : "below");
    r += line;
  }
  if (folded) {
    snprintf(line, sizeof line, "%d note(s) moved an octave to fit the %s (or octave errors)\n", folded, inst->name.c_str());
    r += line;
  }
  if (rec) {
    Key c{0, "maj"};
    r += "pitch search: limited to the " + rec->name + "'s sounding range, " + noteName(spell(c, sLo + LO)) + ".." +
         noteName(spell(c, sHi + LO)) + "\n";
  }
  if (!weak.empty()) r += "unclear bars (check by ear): " + weak + "\n";
  return {s, r};
}

std::string toAbc(const Song& s) {
  std::string out = "X:1\nT:" + s.title + "\nM:" + s.meter + "\nL:1/16\n";
  if (s.tempo > 0) out += "Q:1/4=" + std::to_string(s.tempo) + "\n";
  out += "K:" + keyName(s.key) + "\n";
  auto acc = accidentals(s);
  int bars = 0;
  for (size_t i = 0; i < s.events.size(); i++) {
    const Event& e = s.events[i];
    if (e.kind == Kind::Bar) {
      out += " " + e.bar;
      if (!e.ending.empty()) out += "[" + e.ending;
      out += ++bars % 4 == 0 && i + 1 < s.events.size() ? "\n" : " ";
      continue;
    }
    for (auto& t : e.techniques) out += "!" + t + "!";
    if (e.kind == Kind::Rest) out += "z";
    else {
      if (acc[i].shown) out += acc[i].alter < 0 ? "_" : acc[i].alter > 0 ? "^" : "=";
      Spelled sp = spell(s.key, e.midi);
      char letter = "CDEFGAB"[sp.letter];
      if (sp.octave >= 5) out += (char)tolower(letter), out += std::string(sp.octave - 5, '\'');
      else out += letter, out += std::string(std::max(0, 4 - sp.octave), ',');
    }
    int num = e.len, dn = 12;  // L:1/16 = 12 ticks
    int gcd = std::gcd(num, dn);
    num /= gcd, dn /= gcd;
    if (num != 1) out += std::to_string(num);
    if (dn != 1) out += "/" + std::to_string(dn);
    if (e.tie) out += "-";
    out += " ";
  }
  return out + "\n";
}

}  // namespace john
