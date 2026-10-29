# Penumbra manual

Penumbra 1.0 for the M-VAVE FM-1, by Lunar Eclipse Forge.

Penumbra is a space-drone instrument. Three layers make its sound:

- **The drone** is the ground everything stands on. It is five voices held on a pedal, slowly beating, and it
  barely moves.
- **The colour** is what you play over it: VOX, a soft synth voice far back in the shimmer, and BLOOM, a slow
  swell in the key.
- **The space** is a long shimmering reverb, a worn tape delay, and a wind that is always under everything.

Nothing plays by itself except the drone you latch. Leave room: the silence between notes is part of the sound.

Contents:

1. [Starting](#1-starting)
2. [HOME and the screen](#2-home-and-the-screen)
3. [The keys](#3-the-keys)
4. [The holds](#4-the-holds)
5. [The looper](#5-the-looper)
6. [FREEZE and MORPH](#6-freeze-and-morph)
7. [Scenes and journeys](#7-scenes-and-journeys)
8. [SHAPE: the sound](#8-shape-the-sound)
9. [FX, SYSTEM and saving](#9-fx-system-and-saving)
10. [Cheat sheet](#10-cheat-sheet)

---

## 1. Starting

Switch the FM-1 on. The clock starts and the first scene's drone voices swell in. To start silent instead,
turn **PLAY AT POWER-ON** off in SYSTEM.

- **SEQ** starts and stops the clock. Starting it swells the scene's drones in. Stopping it lets everything
  fade away: the drone, the loop and the tails.
- **ALGO** sets the tempo, which defaults to 90 BPM. The tempo sets the delay's time and the looper's beat.
- **HOME** always brings you back to the main screen. Any other page also returns to HOME after 30 seconds
  untouched, except BENCH.
- **Hold any button** for a moment and it says what it does instead of doing it. FX takes a little longer,
  because holding FX is also how you throw.

## 2. HOME and the screen

HOME shows the corona: the eclipsed moon with a bead on its rim for each drone voice, lit as loud as that
voice is. Each note you play puts out a star.

Under the corona:

- **The scene line** shows the scene's number, key and scale, for example `1/4 A AEOLIAN`. While a move to
  another scene is under way, it adds `> 2`. During a MORPH it shows `~ 2` and a bar for how far you have
  come.
- **On the right** it shows PLAYING or STOPPED, or SLEEP with the minutes left.
- **The looper line** shows what the looper is doing, its layers and its speed, with a bar for where it is in
  the loop.
- **FROZEN** shows when notes are frozen, and **THROW** when a throw is out.

The header carries the journey's name and the tempo. A small dot in the header means something has not been
saved yet.

**HOME's four knobs** are macros, each 0 as made:

| Knob | Macro | What it does |
| --- | --- | --- |
| K1 | **DRONE** | Opens the drone's filter and body together |
| K2 | **VOX** | VOX's brightness |
| K3 | **SPACE** | The reverb and the delay together |
| K4 | **TONE** | Everything brighter or darker |

## 3. The keys

### White keys: VOX

The white keys play VOX in the scene's key and scale, so you cannot play a wrong note. VOX has four voices.
**OCT-** and **OCT+** move the keys' octave, two octaves either way.

VOX also plays notes from MIDI, on SYSTEM's VOX MIDI channel (1 by default). It takes pitch bend too.

### Black keys 1-5: the drone

Each of the first five black keys is a drone voice. Each voice is one of two kinds:

- **HELD** (as made): the key latches the voice on, and pressing it again lets it fall away. A held voice
  latches when you let go of the key, so holding the key to shape the voice does not toggle it (see the
  holds, below).
- **SWELL**: the voice comes and goes. Each press swells it in, holds it a moment, and lets it out again.
  While the clock runs, the journey also swells it in by itself every few bars.

To switch a voice between HELD and SWELL, hold its key and turn **ALGO**: left for HELD, right for SWELL.

The journey sets the drone's notes from the scene: its root (the *pedal*) and its voicing. The drone holds
those notes the whole scene. When you move to another scene, only the notes that must change move: one voice
at a time, the top first and the bass last, three seconds apart, each sliding slowly into place.

### Black key 6: the kick

A deep kick, tuned to the key. Use it rarely: a low thud under the drone, or a slow pulse in a loop.

### Black key 7: BLOOM

A deep, slow swell in the drone's key. Two sine tones a few cents apart beat against each other, with a soft
fifth above them. It swells in over about two seconds, then fades into the reverb. Press it again while it
sounds and it grows from where it is.

### Black keys 8-11: VOX's four sounds

Each key picks one of VOX's sounds. You can switch even mid-phrase.

| Key | Sound | Character |
| --- | --- | --- |
| 8 | **GLASS** | Two soft saws, a little apart: VOX as made |
| 9 | **REED** | A narrow pulse through a band-pass, closer and woody |
| 10 | **HALO** | Wide, an octave above, long and far back |
| 11 | **LOW** | A square an octave below, gliding from note to note |

The key of the sound playing is lit. Each journey keeps its own four sounds.

## 4. The holds

While you hold one of these, the four knobs play something else, on any page.

| Hold | K1 | K2 | K3 | K4 | Extra |
| --- | --- | --- | --- | --- | --- |
| A drone key (black 1-5) | LEVEL | TONE | DETUNE | OCTAVE | ALGO: HELD or SWELL |
| A VOX sound key (black 8-11) | BRIGHT | WAVE | FILTER | RELEASE | |
| BLOOM's key (black 7) | LEVEL | LENGTH | HEIGHT | FIFTH | |
| **FX** | SHIMMER | FEEDBACK | FREEZE | DIP | Throws |
| A white key | BRIGHT | VIBRATO | SHIMMER | RELEASE | Bends; ALGO: PITCH |

The knob strip shows what the knobs are playing while you hold.

- **A drone key** shapes that voice alone: its level and tone, how detuned it is, and its octave (one down, as
  it is, or one up). The change stays.
- **A VOX sound key** shapes that sound. The change stays.
- **BLOOM's key** sets its level, its length, its height (on the root, an octave above, or two) and how much
  fifth it has. The change stays.
- **FX** throws the effects for a moment:
  - **SHIMMER** swells the shimmer.
  - **FEEDBACK** pushes the delay toward feedback, but never into a runaway.
  - **FREEZE** makes the reverb hold what it has while new sound stops going in.
  - **DIP** closes a low-pass filter over the whole mix.

  Let go and the throws drift back over a couple of seconds.
- **A white key** bends VOX for the note you are holding: its brightness, vibrato, shimmer and release.
  Turning **ALGO** bends the pitch of every VOX note sounding, including frozen and looped ones. Let go and
  the bends ease back.

**SAVE while holding keeps what the knobs did.**

- With FX, the throws stay out until you next hold FX and turn a knob.
- With a white key, the bends go into VOX's sound for good, and the pitch stays bent until the next note you
  play.

## 5. The looper

The looper records what you play: VOX's notes, the kick and BLOOM. It plays them back on the clock, in
layers.

| You press | What happens |
| --- | --- |
| **REC** (no loop) | Starts recording. The clock starts too, if it is stopped |
| **REC** again | The loop closes, its length rounded to the nearest beat, and it plays |
| **REC** while it plays | Overdubs a new layer over it |
| **REC** again | Keeps the layer |
| **PLAY** | Stops the loop, or plays it from its start |
| **PLAY** held and let go | Undoes the last layer. With only one layer, the loop goes |
| **PLAY** held, turn **ALGO** | Half speed, as played, or double speed |
| **REC and PLAY together** | Clears the loop |

A loop can be up to 16 bars long, with up to 8 layers.

**The loop follows the scene.** It keeps its notes as degrees of the scene's key, not as fixed pitches. Move
to another scene, or MORPH, and the same tune plays on in the new key.

**The loop is kept.** It is saved with the journey playing, so it comes back after a power cut, and it plays
again when the clock starts. Stopping the clock silences it until the next start. User journey slots do not
hold a loop.

## 6. FREEZE and MORPH

**FREEZE (ARP).** Hold some notes and press ARP: they keep sounding after you let go. Press ARP again and they
ring out; anything you are holding at that moment freezes in their place. Pressing a frozen key takes that
note back into your hand. ARP is lit while notes are frozen.

**MORPH (SELECT, on HOME).** Turn SELECT to walk toward the next scene (right) or the one before (left), by
hand:

- Halfway there, the key changes.
- Turn all the way and you arrive; the next turn sets out again from there.
- Turn back and you go back.

The drone, the wind and VOX's brightness move only as far as you turn. MORPH waits while a PRESET move is
gliding.

## 7. Scenes and journeys

A **scene** is a place to play in:

- a key and scale;
- the drone's root (its pedal) and voicing;
- VOX's brightness and register;
- the space, and the wind's level and colour.

A **journey** is a bank of up to eight scenes, with a sound of its own.

**PRESET** moves to the next scene, or the one before. Everything glides there over 12 seconds, and the key
changes at the middle. Each preset journey keeps its drone's root from scene to scene, so the drone changes
only its colour.

**SCENE (EDIT)** edits the scenes:

- SELECT picks the field.
- K1 picks the scene.
- K2 to K4 change the field.
- **SEL** stores the moment, as it sounds now, as a new scene. When the journey already has eight scenes, it
  goes over the scene shown.

The fields:

| Field | What it sets |
| --- | --- |
| KEY | The scene's key |
| SCALE | Ionian, dorian, phrygian, lydian, mixolydian, aeolian, harmonic minor |
| DRONE ROOT | The drone's pedal: the key's root, or another note in the key |
| VOICING | How the five drone voices stack: open, warm, high, cluster |
| VOX BRIGHTNESS | How bright VOX is in this scene |
| VOX REGISTER | VOX's octave |
| SPACE | How much reverb and delay |
| WIND | The wind's level |
| WIND COLOUR | The wind's colour |

**JOURNEY (LFO)** lists the six presets and eight user slots:

- **The presets**: ECLIPSE, PENUMBRA, SELENE, TRANQUILITY, PERIGEE and FAR SIDE.
- SELECT or PRESET moves through the list, and SEL opens the journey shown.
- SAVE on a user slot stores the journey playing there.
- Opening a preset keeps the sound you have made. Opening a user journey brings back the sound saved with it.

## 8. SHAPE: the sound

**ENV** opens SHAPE: four knobs for each part of the sound, with **ALGO** setting that part's level. Press
ENV again (or turn SELECT) for the next page. The VOX pages shape the VOX sound playing at the time.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| DRONE | FILTER | BODY (resonance) | SWARM (the crowd, and the voices' detune) | WAVE (soft to reedy) |
| VOX | BRIGHT | ATTACK | RELEASE | SHIMMER (how far into the reverb) |
| VOX 2 | WAVE (saw, square, narrow pulse) | FILTER (low-pass to band-pass) | RESO | GLIDE |
| VOX 3 | DETUNE | B OCT (the second oscillator: -1, 0, +1) | B MIX | ENV (how far the envelope opens the filter) |
| PULSE | DROP (the kick's fall in pitch) | DECAY | CLICK | SPACE |
| TEXTURE | COLOUR | SWEEP | RATE | WIDTH |

BLOOM's settings are under its own hold (black key 7, above). PULSE's level sets the kick's and BLOOM's
together.

**SEL twice** on a SHAPE page puts that page back as made:

- DRONE also resets the drone voices' own settings.
- The VOX pages reset the VOX sound playing.
- PULSE also resets BLOOM.

The first press only asks, so you cannot reset by accident.

## 9. FX, SYSTEM and saving

**FX** sets the reverb and the delay. SELECT picks the row; the knobs change it.

| Row | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| REVERB | DECAY | SIZE | SHIMMER | TONE |
| DELAY | TIME (a note length, on the tempo) | FEEDBACK | TONE | WEAR (the tape's age) |

**SYSTEM (GLO)**: SELECT moves through the list, and a knob or SEL changes the row shown.

| Setting | What it does |
| --- | --- |
| MIDI CLOCK OUT | Sends MIDI clock and start/stop while the clock runs |
| FOLLOW CLOCK IN | Follows MIDI clock and start/stop from outside |
| VOX MIDI CH | VOX's MIDI channel, 1-16, or off |
| 2ND CORE | Uses the FM-1's second processor core; applies at the next power-on |
| DRONE CORE | Which core runs the drone voices; SPLIT balances itself |
| OUTPUT LEVEL | NORMAL, or LOUD (6 dB hotter) |
| USB AUDIO | MASTER (follows the volume knob), or FULL level |
| PLAY AT POWER-ON | Starts the clock and the drone when you switch on |
| BEND RANGE | The pitch bend's range: 2, 5, 7 or 12 semitones each way |
| SLEEP | Off, or 20, 40, 60 or 90 minutes (below) |
| RESET SOUND | SEL twice puts the whole sound back as made (below) |
| ABOUT | The version |

**SLEEP** lets everything go, gently:

- Three minutes before the end, the loop stops and FREEZE lets go.
- Then a drone voice falls away every 30 seconds, the top voice first.
- At the end the clock stops.

SLEEP lasts only this session; it is not saved.

**RESET SOUND** puts back as made:

- every SHAPE page, the drone voices and VOX's four sounds;
- the FX page and HOME's macros;
- any throw or bend that is out.

Your scenes, the loop, the tempo and the other settings stay.

**BENCH**: press GLO again from SYSTEM. It shows what each part of the sound costs the processor. SEL there
runs a load test that sets every voice at its heaviest.

**Saving** happens by itself once the sound has been silent for a second and nothing has changed for two, so
after you STOP. SAVE saves at once. Everything is kept: the journey playing, its sound, its loop and the
settings.

**Leaving Penumbra.** Install another firmware the usual way; Penumbra carries the update loader. If the FM-1
ever won't take an install, hold **OCT-** and **OCT+** together for five seconds to put it in update mode.

## 10. Cheat sheet

| Control | Does |
| --- | --- |
| White keys | VOX, in the key |
| Black 1-5 | Drone voices (held: shape; + ALGO: HELD or SWELL) |
| Black 6 / 7 | Kick / BLOOM (held: shape) |
| Black 8-11 | VOX sounds: GLASS, REED, HALO, LOW (held: shape) |
| K1-K4 (HOME) | DRONE, VOX, SPACE, TONE |
| SELECT (HOME) | MORPH |
| PRESET | Next or previous scene |
| ALGO | Tempo (SHAPE: level; holds: see above) |
| SEQ | Clock start / stop |
| REC | Loop: record, play, overdub, keep |
| PLAY | Loop: stop / play; held: undo; held + ALGO: speed |
| REC + PLAY | Clear the loop |
| ARP | FREEZE |
| FX | FX page; held: throws |
| ENV | SHAPE (again: next page; SEL twice: reset) |
| EDIT | SCENE (SEL: store the moment) |
| LFO | JOURNEY (SEL: open; SAVE: into a user slot) |
| GLO | SYSTEM, then BENCH |
| SAVE | Save now; in a hold: keep it |
| OCT- / OCT+ | White keys' octave; both for 5 s: update mode |
| Any button, held | Says what it does |
