# Practice: playback and the mic

John can play a tune for you, and listen while you play it.

## Play

Press **Play** (or Space). A synthesized voice of your instrument (Settings → My instrument) plays the tune:

- in the octave your instrument would sound in;
- in performance order, through repeats and endings;
- with its cuts, rolls, slides, vibrato and breaths.

More controls:

- The *sound* menu switches to another instrument's voice. The notes and the pitch stay the same.
- A one-bar count-in comes first. You can turn it off in Settings.
- You can add a metronome.
- The tempo slider changes the speed, even while playing.
- Playback starts from the selected note.

Singing voices have no synth of their own: see [Instruments](instruments.md#singing-voices).

## Mic

Turn on **Mic** for a live tuner. It shows the note you play, how far off it is in cents, and its fingering.

- **Wait for me**: the cursor waits on each note until you play it in tune.
- **Play along**: John grades each note while the backing plays. Green is right, red is wrong, orange is the wrong octave. Use headphones, or untick *Melody* and keep the metronome, so the mic doesn't hear the backing.
- Right-click *Reset marks* to set the mic noise gate and latency. They are also in Settings.

The mic expects the octave your instrument sounds in, and accepts any octave for a singing voice (see [Instruments](instruments.md)).

On macOS, the first time you turn on the mic, the system asks to allow microphone access for the app you launched `john` from (Terminal, iTerm, ...). On the web, the mic needs `localhost` or HTTPS (see [Web version](web.md)).
