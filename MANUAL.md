# Penumbra manual

Penumbra 1.2 for the M-VAVE FM-1, by Lunar Eclipse Forge.

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

HOME shows the corona: the eclipsed moon with a bead out in its glow for each drone voice, lit and swelling
as loud as that voice is. It shows what you hear:

- **The drone's beads move with it.** They slide round the moon toward the side DRONE is panned to, swaying
  with it, and the tide lifts them out and lets them back (further, the more TIDES moves the DRONE).
- **Each note you play puts out a star**, on VOX's side of the stereo field, and the stars drift as the
  tide sways VOX. If VOX goes to the delay, each echo lights the star again, fainter, in time; with PING-PONG
  the echoes light it on alternate sides.
- **While a scene travels to the next** (or you MORPH there), the moon slides off the sun and back. The
  corona fades, a diamond ring flashes as the sun comes out, a crescent grows, and at the middle, where the
  key changes, the sun shows most. Going to the next scene the moon goes left; to the one before, right.
- **The looper is a satellite** circling the corona once a loop. It trails red while it records, and a
  bead for each layer while it plays.

Under the corona:

- **The scene line** shows the scene's number, key and scale, for example `1/4 A AEOLIAN`. While a move to
  another scene is under way, it adds `> 2`. During a MORPH it shows `~ 2` and a bar for how far you have
  come.
- **On the right** it shows PLAYING or STOPPED, or SLEEP with the minutes left.
- **The looper line** shows what the looper is doing, its layers and its speed (where it is in the loop:
  its satellite).
- **FROZEN** shows when notes are frozen, and **THROW** when a throw is out.

The header carries the journey's name and the tempo. A small dot in the header means something has not been
saved yet.

**HOME's four knobs** are macros, each 0% as made:

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

VOX also plays notes from MIDI, on SYSTEM's MIDI channel (1 by default). It takes pitch bend too. For
playing with a DAW, see section 10.

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
| **FX** | SHIMMER | FEEDBACK | FREEZE | DIP | Throws; ALGO: WARP |
| A white key | BRIGHT | VIBRATO | SHIMMER | RELEASE | Bends; ALGO: PITCH |
| **ENV** | | | | | A/B: OCT- keeps A, OCT+ keeps B, SELECT blends |

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
  - **DIP** filters the whole mix, both ways: turn left and a low-pass closes, turn right and a
    high-pass opens.
  - **WARP** (turn **ALGO**) bends the delay's time: left and the echoes slow and dive, right and they
    rise, like a tape slowing down and speeding up. DELAY 2's WARP sets how far.

  Let go and the throws drift back over a couple of seconds.
- **A white key** bends VOX for the note you are holding: its brightness, vibrato, shimmer and release.
  Turning **ALGO** bends the pitch of every VOX note sounding, including frozen and looped ones. Let go and
  the bends ease back.

- **ENV** holds two versions of the sound, **A** and **B**, and walks between them:
  - Shape a sound, then hold ENV and press **OCT-**: it is kept as A.
  - Shape it another way, hold ENV and press **OCT+**: that is B.
  - Hold ENV and turn **SELECT**: the sound walks from A to B and back, gliding as it follows, the way
    MORPH walks between scenes. Every value moves; an octave or HELD / SWELL changes at the middle.

  The blend is the sound playing: what you hear is what is saved, and you can go on shaping it (keep it
  again to make it A or B). A and B are saved with the journey playing. MORPH the scene and blend the
  sound together for one long change of place and colour.

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
- the space: how much reverb and delay, and its own room (the reverb's size and decay) and halo (its
  shimmer), so a journey travels from one space to the next;
- the wind's level and colour.

A **journey** is a bank of up to eight scenes, with a sound of its own.

**PRESET** moves to the next scene, or the one before. Everything glides there over 12 seconds, the room
too, and the key changes at the middle. Each preset journey keeps its drone's root from scene to scene, so the drone changes
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
| ROOM | The reverb's size and decay, over the FX page's (0%: as the FX page has them) |
| HALO | The shimmer, over the FX page's (0%: as set) |
| WIND | The wind's level |
| WIND COLOUR | The wind's colour |

**JOURNEY (LFO)** lists the six presets and eight user slots:

- **The presets**: ECLIPSE, PENUMBRA, SELENE, TRANQUILITY, PERIGEE and FAR SIDE. Each has a sound of its
  own as well as its scenes:

  | Journey | Its sound |
  | --- | --- |
  | ECLIPSE | Penumbra as made: VOX on a gentle tide, about once every 50 seconds |
  | PENUMBRA | A reedier drone; VOX (REED) and the wind mirrored, swaying every 16 seconds |
  | SELENE | An uneasy, wandering tide; the two high drone voices come and go; VOX (HALO) and the drone a quarter tide apart on it |
  | TRANQUILITY | Soft and still: a three-minute even tide; VOX and the wind drifting together every 40 seconds |
  | PERIGEE | Close and quick: a short tide, VOX (LOW) to the left, the kick and the wind swinging round every 4 seconds |
  | FAR SIDE | Vast: a four-minute wandering tide, three drone voices coming and going, every part taking its turn round the field every 24 seconds |

  SEL twice on a SHAPE page puts it back as Penumbra makes it, not as the journey had it: reopen the journey
  (JOURNEY, SEL) to have its own sound back.
- SELECT or PRESET moves through the list, and SEL opens the journey shown.
- SAVE on a user slot stores the journey playing there.
- Opening a preset keeps the sound you have made. Opening a user journey brings back the sound saved with it.

**SOUNDS (LFO again, from JOURNEY)** is a library of eight sounds, apart from the journeys. A sound is
everything the SHAPE pages, the holds, DRONE 2 and TIDES set; not the scenes, and not the FX page.

- **SAVE** on a slot stores the sound playing.
- **SEL** brings a slot's sound into the journey playing, gliding in over two seconds, so it can be done
  while you play.
- So any of your sounds plays over any journey's scenes, and two of them can be A and B.

## 8. SHAPE: the sound

**ENV** opens SHAPE: four knobs for each part of the sound, with **ALGO** setting that part's level. Press
ENV again (or turn SELECT) for the next page. The VOX pages shape the VOX sound playing at the time.
After you shape something through a hold (a drone key, a VOX sound key, BLOOM's key, a white key), ENV
opens that part's page, so you can go on shaping it in depth.

Values are shown as percentages: a share from 0 to 100%, an offset from as made as -/+% (0%: as made), and
a LEVEL as 100% as made (up to 127%). Times, octaves and note lengths keep their own units.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| DRONE | FILTER | BODY (resonance) | SWARM (the crowd, and the voices' detune) | WAVE (soft to reedy) |
| DRONE 2 | RATE (its slow LFO, ~1 s to 100 s) | SWEEP (the LFO over the filter) | WIDTH (the voices swinging apart) | DRIFT (their tuning wandering) |
| VOX | BRIGHT | ATTACK | RELEASE | SHIMMER (how far into the reverb) |
| VOX 2 | WAVE (saw, square, narrow pulse) | FILTER (low-pass to band-pass) | RESO | GLIDE |
| VOX 3 | DETUNE | B OCT (the second oscillator: -1, 0, +1) | B MIX | ENV (how far the envelope opens the filter) |
| PULSE | DROP (the kick's fall in pitch) | DECAY | CLICK | SPACE |
| TEXTURE | COLOUR | SWEEP | RATE | WIDTH |
| TIDES | RATE (8 s to 4 min a tide) | DRONE (how far it moves the drone's filter) | SPACE (the sends) | VOX (its brightness) |
| PAN | DRONE | VOX | PULSE (and BLOOM) | TEXTURE |
| PAN 2 | DRONE (how far it sways: STILL to 100%) | VOX | PULSE | TEXTURE |

**TIDES** is one slow swell under everything: the drone opens and closes, the space breathes in and out,
VOX brightens and darkens, all on the same tide. On its page **ALGO** sets its SHAPE: from EVEN (a smooth
sine) to WANDERING (it drifts to a new height each half tide). The line under the knobs shows where the
tide is now. As made, it moves the drone and the space gently, about once every 50 seconds.

**PAN** places each part in the stereo field, from LEFT 100% through MIDDLE to RIGHT 100%. The parts glide
to where you put them, so you can move them while you play. The drone's five voices keep their own spread
around the drone's place. On this page **ALGO** sets RATE, how fast the parts sway: at TIDE (all the way
down) they move with the tide, its shape and all; turned up, they get a steady cycle of their own, from 60
seconds down to half a second, so even a part on a slow tide can move where you can hear it. The screen
shows each part's lane, and where the sway has taken it.

**PAN 2** sets how far each part sways, from STILL to 100% (all the way out to each side). When VOX sways,
its echoes follow it half as far. **ALGO** sets OFFSET: at IN SYNC (0%) every swaying part moves together;
turned up, each one follows the one before it (in the order DRONE, VOX, PULSE, TEXTURE) up to half a cycle
behind. At 100% two swaying parts mirror each other, one going left as the other goes right. At 50% each
is a quarter of a cycle behind the last, so four swaying parts take turns round the field. The screen dims
the parts that stay still.

As made, every part is in the middle and VOX sways 25% on the tide, in sync. Journeys from 1.1 and earlier
come in with no sway, so they sound as they did.

BLOOM's settings are under its own hold (black key 7, above). PULSE's level sets the kick's and BLOOM's
together.

**SEL twice** on a SHAPE page puts that page back as made:

- DRONE also resets the drone voices' own settings.
- The VOX pages reset the VOX sound playing.
- PULSE also resets BLOOM.
- DRONE 2, TIDES, PAN and PAN 2 reset only their own page.

The first press only asks, so you cannot reset by accident.

## 9. FX, SYSTEM and saving

**FX** sets the reverb and the delay. SELECT picks the row; the knobs change it.

| Row | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| REVERB | DECAY | SIZE | SHIMMER | TONE |
| REVERB 2 | MOTION (how much its lines move) | LOW CUT (on what goes in) | INTERVAL (the shimmer: an octave, a fifth or a twelfth up) | LEVEL (its return) |
| DELAY | TIME (a note length, on the tempo) | FEEDBACK | TONE | WEAR (the tape's age) |
| DELAY 2 | LEVEL (its return) | LOW CUT | PING-PONG (on: the echoes go left, right) | WARP (how far the WARP throw bends it: up to two octaves) |

A change of SIZE glides: the reverb's lines slide to the new size, so the room breathes and never clicks.
A new delay TIME, or a new tempo, is taken once you stop turning: the echoes cross-fade to it and stay on
pitch. Only the WARP throw bends them.
Each scene can move the reverb further with its ROOM and HALO (section 7).

**SYSTEM (GLO)**: SELECT moves through the list, and a knob or SEL changes the row shown.

| Setting | What it does |
| --- | --- |
| MIDI CLOCK OUT | Sends MIDI clock and start/stop while the clock runs |
| FOLLOW CLOCK IN | Follows MIDI clock and start/stop from outside |
| MIDI CHANNEL | VOX's notes, the CCs and program changes, 1-16, or off; the black keys use the next channel (section 10) |
| MIDI NOTES OUT | Sends the notes you play |
| MIDI CC OUT | Sends the CCs and program changes as you change things |
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

Your scenes, the loop, the tempo, A and B, the sound library and the other settings stay.

**BENCH**: press GLO again from SYSTEM. It shows what each part of the sound costs the processor. SEL there
runs a load test that sets every voice at its heaviest.

**Saving** happens by itself once the sound has been silent for a second and nothing has changed for two, so
after you STOP. SAVE saves at once. Everything is kept: the journey playing, its sound, its loop and the
settings.

**Leaving Penumbra.** Install another firmware the usual way; Penumbra carries the update loader. If the FM-1
ever won't take an install, hold **OCT-** and **OCT+** together for five seconds to put it in update mode.

## 10. MIDI and a DAW

Connect the FM-1 by USB: a computer sees it as an audio device and a MIDI device. The TRS jack takes MIDI
in too. Everything below is on SYSTEM's MIDI CHANNEL (channel *N*, 1 by default), except the black keys,
which use the channel after it (*N*+1).

**The clock.** With FOLLOW CLOCK IN on, the DAW's clock runs the tempo, and its start, stop and continue
run the transport. Penumbra times each tick as it arrives, so it holds the DAW's tempo to about a tenth of
a BPM, and the delay's echoes stay put. SONG POSITION is followed too: when the DAW locates or loops, the
clock and a playing loop carry on from there. With MIDI CLOCK OUT on and nothing to follow, Penumbra sends
its own clock.

**Notes.** VOX plays notes on channel *N*, and sends the notes you play on the white keys there (MIDI
NOTES OUT). The black keys go out on channel *N*+1 as notes 36 to 46, one for each key from black 1 to
black 11. Notes 36 to 46 coming in on that channel press those keys, so the DAW can play the drone, the
kick, BLOOM and the VOX sounds back exactly as you did. The loop's own notes are not sent; yours were, as
you played them. To record the FM-1's notes, set the DAW track's output somewhere other than the FM-1, or
every note will sound twice. ALL NOTES OFF (CC 123) ends VOX's notes.

**CCs.** Each of these can be automated by the DAW. When you change one on the FM-1 (by a knob, A/B, a
load or a throw), Penumbra sends its CC (MIDI CC OUT), so the DAW can record your moves. A CC coming in is
never sent back.

| CC | Control |
| --- | --- |
| 14-17 | HOME: DRONE, VOX, SPACE, TONE |
| 18-20 | Throws: SHIMMER, FEEDBACK, FREEZE |
| 21 | DIP (64: open; below, the low-pass; above, the high-pass) |
| 22 | WARP (64: none) |
| 23-27 | TIDES: RATE, DRONE, SPACE, VOX, SHAPE |
| 28-31 | DRONE 2: RATE, SWEEP, WIDTH, DRIFT |
| 70-73 | LEVEL: DRONE, VOX, PULSE, TEXTURE |
| 85-88 | PAN: DRONE, VOX, PULSE, TEXTURE (64: the middle) |
| 89 | PAN's RATE (0: the tide) |
| 90 | PAN 2's OFFSET |
| 102-105 | FX: REVERB row (DECAY, SIZE, SHIMMER, TONE) |
| 106-109 | FX: REVERB 2 row (MOTION, LOW CUT, INTERVAL, LEVEL) |
| 110-113 | FX: DELAY row (TIME, FEEDBACK, TONE, WEAR) |
| 114-117 | FX: DELAY 2 row (LEVEL, LOW CUT, PING-PONG, WARP) |

A throw set by a CC stays where the DAW puts it; it does not drift back. The FX page's switches (the delay
TIME, INTERVAL, PING-PONG) spread their steps over 0 to 127.

**Program changes**, both ways:

| Program | Does |
| --- | --- |
| 0-7 | Travel to scene 1-8 of the journey playing |
| 8-13 | Open preset journey 1-6 |
| 16-23 | Open user journey 1-8 (if saved) |

## 11. Cheat sheet

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
| FX | FX page; held: throws (DIP both ways), + ALGO: WARP |
| ENV | SHAPE, TIDES, PAN, PAN 2 (again: next page; SEL twice: reset); held: A/B |
| EDIT | SCENE, ROOM, HALO (SEL: store the moment) |
| LFO | JOURNEY (SEL: open; SAVE: into a user slot); again: SOUNDS |
| GLO | SYSTEM, then BENCH |
| SAVE | Save now; in a hold: keep it |
| OCT- / OCT+ | White keys' octave; both for 5 s: update mode; ENV held: keep A / B |
| Any button, held | Says what it does |
