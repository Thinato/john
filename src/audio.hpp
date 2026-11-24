// Speaker and microphone devices for the GUI (miniaudio). The music logic lives in sound.cpp.
#pragma once
#include "john.hpp"
#include <atomic>
#include <memory>

struct AudioDevices;

class Audio {
 public:
  Audio();
  ~Audio();
  // Backing track. The performance must not change while playing (stop() first).
  bool play(john::Performance p);
  void stop();
  bool playing() const;     // false once the performance has finished
  double playTime() const;  // seconds since start (count-in included)
  const john::Performance& performance() const { return perf_; }
  std::atomic<bool> melody{true}, metronome{true};
  std::atomic<float> volume{0.6f};

  bool startMic();
  void stopMic();
  bool micOn() const;
  int micRate() const;
  int latest(float* out, int n) const;  // copies the most recent n mic samples, returns n (or 0 when off)
  long long micSamples() const;         // mic samples received since startMic(): tells new audio from old

  std::string error;

 private:
  friend struct AudioDevices;
  std::unique_ptr<AudioDevices> d_;
  john::Performance perf_;
};
