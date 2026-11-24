#include "miniaudio.h"  // implementation lives in core (miniaudio.cpp)
#include "audio.hpp"
#include <algorithm>
#include <iterator>

constexpr int RING = 1 << 15;  // ~0.7 s of mic audio at 48 kHz

struct AudioDevices {
  Audio* owner = nullptr;
  ma_device out{}, in{};
  bool outInit = false, inInit = false;
  john::Synth synth;
  std::atomic<long long> pos{0};
  std::atomic<bool> finished{false};
  float ring[RING] = {};
  std::atomic<unsigned> writeIdx{0};
  std::atomic<long long> heard{0};  // mic samples received since startMic()

  static void playCb(ma_device* dev, void* output, const void*, ma_uint32 frames) {
    auto* d = static_cast<AudioDevices*>(dev->pUserData);
    d->synth.melody = d->owner->melody;
    d->synth.metronome = d->owner->metronome;
    d->synth.volume = d->owner->volume;
    long long p = d->pos.load();
    d->synth.render(d->owner->perf_, p, static_cast<float*>(output), (int)frames);
    d->pos.store(p);
    if (p > (d->owner->perf_.length + 0.5) * d->synth.sampleRate) d->finished = true;
  }
  static void micCb(ma_device* dev, void*, const void* input, ma_uint32 frames) {
    auto* d = static_cast<AudioDevices*>(dev->pUserData);
    const float* x = static_cast<const float*>(input);
    unsigned w = d->writeIdx.load();
    for (ma_uint32 i = 0; i < frames; i++) d->ring[(w + i) % RING] = x[i];
    d->writeIdx.store((w + frames) % RING);
    d->heard += frames;
  }
};

Audio::Audio() : d_(std::make_unique<AudioDevices>()) { d_->owner = this; }
Audio::~Audio() { stop(); stopMic(); }

bool Audio::play(john::Performance p) {
  stop();
  perf_ = std::move(p);
  d_->synth = john::Synth{};
  d_->pos = 0;
  d_->finished = false;
  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = 1;
  cfg.sampleRate = d_->synth.sampleRate;
  cfg.dataCallback = AudioDevices::playCb;
  cfg.pUserData = d_.get();
  if (ma_device_init(nullptr, &cfg, &d_->out) != MA_SUCCESS) { error = "could not open the speaker"; return false; }
  d_->outInit = true;
  if (ma_device_start(&d_->out) != MA_SUCCESS) { error = "could not start the speaker"; stop(); return false; }
  error.clear();
  return true;
}

void Audio::stop() {
  if (d_->outInit) ma_device_uninit(&d_->out);  // waits for the callback to finish
  d_->outInit = false;
}

bool Audio::playing() const { return d_->outInit && !d_->finished; }
double Audio::playTime() const { return double(d_->pos.load()) / d_->synth.sampleRate; }

bool Audio::startMic() {
  if (d_->inInit) return true;
  // a fresh ring: no audio from a previous session is read as new (the web's getUserMedia starts late)
  std::fill(std::begin(d_->ring), std::end(d_->ring), 0.0f);
  d_->writeIdx = 0;
  d_->heard = 0;
  ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
  cfg.capture.format = ma_format_f32;
  cfg.capture.channels = 1;
  cfg.sampleRate = 48000;
  cfg.dataCallback = AudioDevices::micCb;
  cfg.pUserData = d_.get();
  if (ma_device_init(nullptr, &cfg, &d_->in) != MA_SUCCESS) { error = "could not open the microphone"; return false; }
  d_->inInit = true;
  if (ma_device_start(&d_->in) != MA_SUCCESS) { error = "could not start the microphone"; stopMic(); return false; }
  error.clear();
  return true;
}

void Audio::stopMic() {
  if (d_->inInit) ma_device_uninit(&d_->in);
  d_->inInit = false;
}

bool Audio::micOn() const { return d_->inInit; }
int Audio::micRate() const { return 48000; }
long long Audio::micSamples() const { return d_->heard.load(); }

int Audio::latest(float* out, int n) const {
  if (!d_->inInit || n > RING) return 0;
  unsigned w = d_->writeIdx.load();
  // ponytail: lock-free read may catch a sample being written; at worst one slightly glitched analysis frame
  for (int i = 0; i < n; i++) out[i] = d_->ring[(w + RING - n + i) % RING];
  return n;
}
