# Instruments

John started with the tin whistle, but a song can be for other instruments too.

## The song's instrument and yours

Each song is for an instrument: tin whistle (in any key), violin, piano, guitar, electric guitar, harmonica, or a singing voice (soprano, alto, tenor, bass). The tin whistle is the default, unless another instrument is picked in Settings.

The song's instrument says what the sheet was written for. Pick it with the *Written for* picker in the viewer, or tell the agent.

Everything about playing follows **your** instrument (Settings → My instrument):

- the fingering charts. For a whistle, they are in your whistle's key. Untick *Instrument hints* in the sidebar to hide them. Fingering charts exist only for the whistle.
- the red marks on notes out of range;
- the playback voice and octave;
- the mic, which expects the octave your instrument sounds in. A whistle playing a fiddle tune sounds an octave above the written notes, and that counts as right.

## Which songs fit

Songs are melodies, so most can be played on more than one instrument. The song list shows the songs that fit *your* instrument (Settings → My instrument, with its whistle or harmonica key), in any key.

- A song that only fits in another key gets a small badge, like "in G" or "octave up".
- Opening it shows a one-click "Play in G major" button.
- "Fits" means no note out of range and fewer than 10 % hard notes. Hard notes are whistle half-holes and harmonica bends. A cross-fingered C counts as a fifth of a hard note, an overblow as three.
- Pick "All songs" in the menu above the list to see everything.

The agent's `list_songs` and `analyze_song` tools report the same verdict, for your instrument or any other (see [MCP tools](mcp-tools.md)).

## Play a song in another key

Use the *Play in key* picker. It transposes the song by the smallest step, picks the octave that fits the instrument best, and saves it. The original key is marked, and picking it brings back the original sheet.

## Harmonica

The harmonica is a 10-hole diatonic (Richter) harp in any key, G to F#. Pick yours under Settings, or per song with the *Harmonica key* picker.

- Its range follows the harp, so an A harp starts on A3.
- Blow and draw notes are easy. Notes you can only get by bending are hard. Overblows and overdraws are harder: they count three.
- So a tune in the bottom octave of the harp often fits only an octave up.
- For harmonica songs, the details panel shows the usual keys for that harp: 1st position is the harp's key, 2nd position is a fifth above.

## Singing voices

Singers pick a voice as their instrument:

| Voice | Range |
|---|---|
| Soprano | C4 to A5 |
| Alto | G3 to E5 |
| Tenor | C3 to A4 |
| Bass | E2 to E4 |

Tenor and bass music is written an octave above how it sounds, on the treble staff, as in choir books.

**Find my range.** With a voice picked, Settings → My instrument shows *Find my range*.

1. Sing your lowest comfortable note for about three seconds.
2. Then sing your highest.
3. John shows each note on a small staff. You can retry either one.
4. *Save* stores the range as your own instrument, *My voice*, and picks it. The song list then shows the songs that fit your voice.

Run it again any time.

Also for voices:

- With the mic on, a note sung an octave off counts as right, because singers pick their own octave.
- During playback and play-along, the syllable being sung is highlighted.
- Voices list breaths, slides and vibrato as their techniques.
- John has no singing synth, so the tunes are played on the violin sound, with breaths.

## Instrument files

Each instrument is one JSON file. It holds:

- its name;
- its lowest and highest written note;
- the octave it sounds in;
- its key;
- which notes are easy, hard (half-hole, cross-fingering, bend, overblow) or impossible;
- the techniques it can play;
- the voice that plays it;
- `"voice": true` for a singing voice, so the mic accepts any octave.

John's own instruments are in `instruments/`.

## Custom instruments

Your own instruments go in `~/Documents/John/instruments/<id>.json`. Two ways to make one:

- In Settings → Custom instruments: name, lowest and highest note, key, then *Chromatic* or *Diatonic* to fill the notes. Click a note to make it easy, hard or impossible. Then pick the techniques and the sound.
- Ask the agent, for example "I play a C flute, low D to high A". It uses `define_instrument`, `list_instruments` and `delete_instrument`.

Custom instruments show up in My instrument, Written for and Import song.

- A file with the id of one of John's instruments replaces it.
- A broken file is skipped, with a warning in Settings.
- Deleting an instrument asks first, and says how many songs are written for it. Those songs keep their notes and show "Written for: <name> (deleted)".

## Technique marks follow your instrument

The sheet shows the technique marks that **your** instrument (Settings) can play.

- A mark your instrument can't play is shown and played as its equivalent, when the profile has one (`instead`). The piano plays cuts and taps as grace notes and rolls as turns. The harmonica plays rolls as shakes.
- With no equivalent, the mark is hidden. The legend says how many are hidden, and *Show all marks* (in the legend or Settings) brings them back.
- Breath marks always show. On instruments that don't breathe, the legend calls them a phrase end (lift).
- The song file keeps every mark, so switching back to the whistle shows the original ones.

Custom instruments set `instead` in the editor's *Played instead* section, or with `define_instrument`.
