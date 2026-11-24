# Songs and Import song

How songs get into John, where they are kept, and the lessons that come with it.

## Import song

**Import song** is a button in the sidebar, on the web and on the desktop. No agent needed.

- Paste ABC notation (from thesession.org, abcnotation.com, ...) and press *Import*.
- Or open a file:
  - an `.abc` file;
  - a MusicXML score: `.musicxml`, `.xml` or zipped `.mxl`, as MuseScore, Finale, Sibelius and flat.io export it.
- On the web, use *Open a file...*. On the desktop, type the file's path or drop it on the window.

Pick the instrument the sheet is for at the top of the window, with the whistle or harmonica key. It starts on the Settings instrument and is remembered until you close John.

What you get:

- Every ABC tune (`X:`) becomes a song in *Unsorted* for that instrument.
- If many notes fall outside the instrument's range, the window suggests *Play in key* (see [Instruments](instruments.md#play-a-song-in-another-key)).
- A MusicXML score becomes one song, from its first part and voice. Chords are reduced to the top note.
- Lyrics are shown under the notes: ABC `w:` and MusicXML `<lyric>`, first verse.
- Warnings are listed in the window, and the first song opens.

The ABC reader understands thesession.org tune bodies, grace notes as cuts and taps, `!decorations!`, tuplets, voltas and `w:` lyrics.

## Images and PDFs (web only for now)

On the web, *Open a file...* also takes a photo, scan or PDF of sheet music: PNG, JPG, WebP, and HEIC where the browser opens it.

- A music reader that runs in the browser reads every page in order into one song: [copisteria](https://github.com/kobimusic/copisteria) with the copista-2m model.
- The image never leaves the device.
- The first time, it downloads about 22 MB, with a progress bar and *Cancel*. The browser keeps it afterwards.
- The first page is shown next to the sheet (*Show the original image*), so you can compare.
- Clean printed scores read best. Photos and handwriting read worse.
- Small scans are enlarged 2x before reading.
- Sometimes only the first line comes out without the key signature of the rest. Then John gives it that key and says so.

How the reader is built and licensed: see [Web version](web.md#the-image-reader).

## Folders

Songs are grouped into folders: Lessons, Beginner, Intermediate, Advanced, or any name. The agent files new songs by difficulty. You can move a song with the Folder picker in the viewer.

## Where songs are kept

On the desktop, songs live in `~/Documents/John/songs/*.json`. On Windows that is `%USERPROFILE%\Documents\John`. Set `JOHN_HOME` to use another folder.

Other files in the same folder:

- Songs made with Import audio keep their pitch track in `tracks/<id>.json`, next to `songs/` (see [Transcription](transcription.md)).
- Custom techniques: `~/Documents/John/techniques.json`.
- Settings: `~/Documents/John/settings.json`.
- Your own instruments: `~/Documents/John/instruments/` (see [Instruments](instruments.md#custom-instruments)).

On the web, songs are kept in the browser (see [Web version](web.md)).

## Lessons

`lessons/` holds a beginner course for the D whistle. John installs it into your song list the first time it runs. Delete a lesson and it stays deleted. Each lesson has comments on the notes explaining what to practise:

1. **Breath and tongue**: long tones, tonguing, slurring (low octave)
2. **First high notes**: octave jumps on G, A and B, the easiest second-octave notes
3. **High F#, E and D**: the harder bottom-hand high notes
4. **Across the break**: the two-octave scale and the C# to high D move
5. **Ode to Joy**: Beethoven, almost entirely in the high octave
6. **Cuts**: the core Irish ornament, in both octaves
7. **Amazing Grace**: slides, vibrato and breathing in a slow air
8. **John's Air**: an original piece that combines all of the above

Lessons 5 and 7 are public-domain tunes. The rest are original exercises.

The lessons and the starter songs also come in Portuguese: see [Settings and languages](settings-and-languages.md#translated-songs).
