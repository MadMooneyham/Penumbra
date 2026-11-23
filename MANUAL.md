# Penumbra manual

Penumbra 1.9 for the M-VAVE FM-1, by Lunar Eclipse Forge.

Penumbra is an ambient space-drone synth that plays itself. What it does best comes from shaping and
moving the sound while it plays: turning HOME's knobs, throwing the effects, holding a voice to reshape it,
gliding from scene to scene. The keys are there too, always in tune, but they are one way in among many.

Three layers make its sound:

- **The drone** is the ground everything stands on: five voices on a pedal, slowly beating, drifting in and
  out or held.
- **The colour** over it: VOX, a soft synth voice far back in the shimmer that can sing by itself, and BLOOM,
  a slow swell in the key.
- **The space** is a long shimmering reverb, a worn tape delay, and a wind that is always under everything.

**The first few minutes.** Penumbra is slow, and much of what it does best unfolds over a few minutes:

- **On its own** it plays as soon as it is on. A new unit plays TOTALITY: the drone's voices drift in and
  out, a voice sings now and then, and every 16 bars it glides to its next scene.
- **HOME's four knobs** shape it as it plays: the drone's filter, body, swarm and wave first. **ALGO** on HOME
  turns to the next page of four: VOX, the drone's motion, the space, the mix, and what plays by itself.
- **FX held** turns the knobs into throws: a burst of shimmer, feedback, a frozen room, the whole mix dipped.
  Let go and they drift back.
- **A drone key held** (black 1-5) puts that voice under the knobs; pressed, it calls the voice in.
- **PRESET** glides to the next scene over 12 seconds, the key and the room changing as it goes. **SELECT**
  on HOME walks there by hand, as slowly as it is turned.
- **The white keys** play VOX, always in the scene's key, and **REC** loops what they play, following each
  key change.

Contents:

1. [Starting](#1-starting)
2. [Shaping on HOME](#2-shaping-on-home)
3. [Gestures: holds, FREEZE and MORPH](#3-gestures-holds-freeze-and-morph)
4. [Scenes and journeys](#4-scenes-and-journeys)
5. [The loop](#5-the-loop)
6. [The keys](#6-the-keys)
7. [SHAPE: the whole sound](#7-shape-the-whole-sound)
8. [FX, SYSTEM and saving](#8-fx-system-and-saving)
9. [MIDI and a DAW](#9-midi-and-a-daw)
10. [Cheat sheet](#10-cheat-sheet)

---

## 1. Starting

When the FM-1 switches on, the clock starts and the journey's drone begins. A new unit plays TOTALITY, which
carries on by itself (section 4): it sings, and travels from scene to scene. In ECLIPSE the lowest voice
swells in at once and the others follow every few bars. **PLAY AT POWER-ON** (in SYSTEM), turned off, starts it
silent instead.

- **SEQ** starts and stops the clock. Starting it swells the scene's drones in. Stopping it lets everything
  fade away: the drone, the loop and the tails.
- **ALGO** on HOME turns HOME's knob pages (section 2). On the other pages it sets the tempo, which defaults
  to 90 BPM. The tempo sets the delay's time and the looper's beat.
- **HOME** always brings you back to the main screen. Any other page also returns to HOME after 30 seconds
  untouched, except BENCH.
- **Hold any button** for a moment and it says what it does instead of doing it. FX takes a little longer,
  because holding FX is also how you throw.

## 2. Shaping on HOME

### The knob pages

**HOME's four knobs** play one of six pages, and **ALGO** on HOME turns to the next, round and round. The knob
strip shows the page's name, a dot for each of the six, and each knob as an arc of the corona filled to its
value (an offset from as made fills from the top, either way). The page stays chosen across a power-off.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| **DRONE** | FILTER | BODY (resonance) | SWARM (the crowd, and the voices' detune) | WAVE (soft to reedy) |
| **VOX** | BRIGHT | FILTER (low-pass to band-pass) | RESO | RELEASE |
| **MOTION** | SWEEP (the drone's slow LFO over its filter) | RATE (that LFO, ~1 s to 100 s) | DRIFT (the drone voices' tuning wandering) | TIDES (how far the tide reaches the drone, VOX and the space) |
| **SPACE** | REVERB (its return) | DECAY | SHIMMER | DELAY (its return) |
| **MIX** | DRONE level | VOX level | PULSE level (the kick and BLOOM) | TEXTURE level |
| **ALIVE** | SINGS (how often the sound playing sings) | BLOOMS (BLOOM by itself) | SWELLS (how many drone voices come and go, the top ones first) | TRAVELS (on to the next scene every 8 to 64 bars) |

- **DRONE, VOX and MOTION** are SHAPE's own settings (section 7), on HOME so they can be played. VOX's are the
  VOX sound playing; a VOX sound key (black 8-11) picks which.
- **SPACE** is the FX page's own settings (section 8).
- **ALIVE** moves a step a click: from nothing by itself to a lot, while the clock runs.
- Everything but SPACE belongs to the journey, and is saved with it.

**The knobs' speed.** Each click is one step. A knob that keeps spinning quickly goes further: after four
quick clicks in a row, two steps a click; after ten, three. A slow click or a turn back is one step again.

### The screen

HOME shows the corona: the eclipsed moon with a bead out in its glow for each drone voice, lit and swelling
as loud as that voice is. It shows what you hear:

- **The drone's beads move with it.** They slide round the moon toward the side DRONE is panned to, swaying
  with it, and the tide lifts them out and lets them back (further, the more TIDES moves the DRONE).
- **Each note puts out a star**, on VOX's side of the stereo field, and the stars drift as the tide sways
  VOX. If VOX goes to the delay, each echo lights the star again, fainter, in time; with PING-PONG the echoes
  light it on alternate sides.
- **While a scene travels to the next** (or MORPH walks there), the moon slides off the sun and back. The
  corona fades, a diamond ring flashes as the sun comes out, a crescent grows, and at the middle, where the
  key changes, the sun shows most. Going to the next scene the moon goes left; to the one before, right.
- **The looper is a satellite** circling the corona once a loop, trailing red while it records. Each layer
  has a bead on the orbit's lower arc at its place in the field, left to right, moving as it drifts, in its
  sound's colour (GLASS pale, REED amber, HALO blue, LOW red), and lit while it sounds.

Under the corona:

- **The scene line** shows the scene's number, key and scale, for example `1/4 A AEOLIAN`. While a move to
  another scene is under way, it adds `> 2`. During a MORPH it shows `~ 2` and a bar for how far it has come.
- **On the right** it shows PLAYING or STOPPED, or SLEEP with the minutes left.
- **The looper line** shows what the looper is doing, its layers and its speed.
- **FROZEN** shows when notes are frozen, and **THROW** when a throw is out.

The header carries the journey's name and the tempo. A small dot in the header means something has not been
saved yet.

**Behind every other page** an eclipsed moon rises from the bottom-right corner under a sky of stars, its
corona in the page's own colour: FX teal, JOURNEY violet, SCENE blue, SOUNDS pink, LOOP green, SHAPE amber.
The corona breathes with the tide and a few stars twinkle, slowly. The row chosen on a page is tinted in the
same colour.

**Installs** show how far they have got: Penumbra's screen while it reads the package and readies the
installer, then the installer's own, with a bar, while it writes. The FM-1 starts again when it is done.

## 3. Gestures: holds, FREEZE and MORPH

### The holds

While one of these is held, the four knobs play something else. The buttons (FX, ENV) do so on any page; the
keys do on every page but SHAPE, where the knobs stay the page's, so keys can be played while a sound is
shaped.

| Hold | K1 | K2 | K3 | K4 | Extra |
| --- | --- | --- | --- | --- | --- |
| **FX** | SHIMMER | FEEDBACK | FREEZE | DIP | Throws; ALGO: WARP |
| A drone key (black 1-5) | LEVEL | TONE | DETUNE | OCTAVE | ALGO: HELD or SWELL |
| The kick's key (black 6) | DROP | DECAY | CLICK | TUNE | ALGO: by itself |
| A VOX sound key (black 8-11) | BRIGHT | WAVE | FILTER | RELEASE | ALGO: it sings |
| BLOOM's key (black 7) | LEVEL | LENGTH | HEIGHT | FIFTH | ALGO: by itself |
| A white key | BRIGHT | VIBRATO | SHIMMER | RELEASE | Bends; ALGO: PITCH |
| **ENV** | | | | | A/B: OCT- keeps A, OCT+ keeps B, SELECT blends |

The knob strip shows what the knobs are playing during a hold.

- **FX** throws the effects for a moment:
  - **SHIMMER** swells the shimmer.
  - **FEEDBACK** pushes the delay toward feedback, but never into a runaway.
  - **FREEZE** makes the reverb hold what it has while new sound stops going in.
  - **DIP** filters the whole mix, both ways: to the left a low-pass closes, to the right a high-pass opens.
  - **WARP** (**ALGO**) bends the delay's time: left and the echoes slow and dive, right and they rise, like
    a tape slowing down and speeding up. DELAY 2's WARP sets how far.

  Let go and the throws drift back over a couple of seconds.
- **A drone key** shapes that voice alone: its level and tone, how detuned it is, and its octave (one down, as
  it is, or one up). The change stays.
- **A VOX sound key** shapes that sound. The change stays.
- **The kick's key** shapes the kick: how far its pitch drops, how long it rings, its click, and its TUNE in
  the scene's key (on the root, the fourth, the fifth, or an octave up). **ALGO** sets it playing by itself
  (section 6). The change stays.
- **BLOOM's key** sets its level, its length, its height (on the root, an octave above, or two) and how much
  fifth it has. The change stays.
- **A white key** bends VOX for the note held: its brightness, vibrato, shimmer and release. **ALGO** bends
  the pitch of every VOX note sounding, frozen and looped ones too. Let go and the bends ease back.
- **ENV** holds two versions of the sound, **A** and **B**, and walks between them:
  - ENV held with **OCT-**: the sound as it is now is kept as A.
  - ENV held with **OCT+**: the sound as it is now is kept as B.
  - ENV held, **SELECT** turned: the sound walks from A to B and back, gliding as it follows, the way MORPH
    walks between scenes. Every value moves; an octave or HELD / SWELL changes at the middle.

  The blend is the sound playing: what is heard is what is saved, and it can go on being shaped (keep it
  again to make it A or B). A and B are saved with the journey playing. MORPH the scene and blend the sound
  together for one long change of place and colour.

**SAVE during a hold keeps what the knobs did.**

- With FX, the throws stay out until FX is next held and a knob turned.
- With a white key, the bends go into VOX's sound for good, and the pitch stays bent until the next note.

### FREEZE and MORPH

**FREEZE (ARP).** Notes held when ARP is pressed keep sounding after they are let go. ARP again and they ring
out; anything held at that moment freezes in their place. A frozen key pressed again comes back into the
hand. ARP is lit while notes are frozen.

**MORPH (SELECT, on HOME).** SELECT walks toward the next scene (right) or the one before (left), by hand:

- Halfway there, the key changes.
- All the way, it arrives; the next turn sets out again from there.
- Turned back, it goes back.

The drone, the wind and VOX's brightness move only as far as it is turned. MORPH waits while a PRESET move is
gliding.

## 4. Scenes and journeys

A **scene** is a place to play in:

- a key and scale;
- the drone's root (its pedal) and voicing;
- VOX's brightness and register;
- the space: how much reverb and delay, and its own room (the reverb's size and decay) and halo (its
  shimmer), so a journey travels from one space to the next;
- the wind's level and colour.

A **journey** is a bank of up to eight scenes, with a sound of its own.

**PRESET** moves to the next scene, or the one before. Everything glides there over 12 seconds, the room
too, and the key changes at the middle. Each preset journey keeps its drone's root from scene to scene, so
the drone changes only its colour.

**A journey can travel by itself.** With TRAVELS set (ALIVE on HOME, or SCENE's last field), the journey moves
to its next scene after that many bars in each, round and round, for as long as the clock runs. PRESET and
MORPH still move it by hand, and the count starts again from wherever that leaves it.

**The sound can travel with the scenes.** With A and B kept (ENV held, section 3), a scene's SOUND A-B names
a place between them. Moving to that scene glides the sound there, as MORPH and PRESET glide everything else,
so each scene has its own colour of the sound. A scene set to STAYS leaves the sound where it is. Neither of
these writes to the FM-1's memory as it moves, so a journey can travel all night.

**SCENE (EDIT)** edits the scenes:

- SELECT picks the field.
- K1 picks the scene.
- K2 to K4 change the field.
- **SEL** stores the moment, as it sounds now, as a new scene. When the journey already has eight scenes, it
  goes over the scene shown.

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
| SOUND A-B | Where the sound sits between A and B in this scene (STAYS: the scene leaves the sound alone) |
| TRAVELS | The whole journey's, not the scene's: BY HAND, or on to the next scene EVERY 8, 16, 32 or 64 BARS |
| LANDS | The whole journey's too: QUIETLY, or A KICK as each new scene's key comes in |

**JOURNEY (LFO)** lists the seven presets and eight user slots:

- **The presets**: ECLIPSE, PENUMBRA, SELENE, TRANQUILITY, PERIGEE, FAR SIDE and TOTALITY. Each has a
  sound of its own as well as its scenes:

  | Journey | Its sound |
  | --- | --- |
  | ECLIPSE | Penumbra as made: all five drone voices coming and going; VOX on a gentle tide, about once every 50 seconds |
  | PENUMBRA | A reedier drone; VOX (REED) and the wind mirrored, swaying every 16 seconds |
  | SELENE | An uneasy, wandering tide; the two high drone voices come and go; VOX (HALO) and the drone a quarter tide apart on it |
  | TRANQUILITY | Soft and still: a three-minute even tide; VOX and the wind drifting together every 40 seconds |
  | PERIGEE | Close and quick: a short tide, VOX (LOW) to the left, the kick and the wind swinging round every 4 seconds |
  | FAR SIDE | Vast: a four-minute wandering tide, three drone voices coming and going, every part taking its turn round the field every 24 seconds |
  | TOTALITY | The one that plays itself, and what a new unit starts on: an eclipse from first contact to afterglow in six scenes on a pedal D. It travels on every 16 bars, HALO sings now and then, every part sways in turn, and its sound glides from A (dark, close) to B (open, a full swarm, a deep tide) and back with the scenes |

  SEL twice on a SHAPE page puts it back as Penumbra makes it, not as the journey had it: reopening the
  journey (JOURNEY, SEL) brings its own sound back.
- SELECT or PRESET moves through the list, and SEL opens the journey shown.
- SAVE on a user slot stores the journey playing there.
- **A journey opened brings its own sound.** It glides in over two seconds from the sound playing, so each
  journey sounds as it was made: its drone, its tide, its movement, the voices that come and go. TOTALITY
  brings its A and B too, its singing and its travelling. A user journey brings back the sound saved with it.
- **SOUND: LOCKED.** Any knob turned right on JOURNEY locks the sound: journeys opened then bring their
  scenes (and each part's place in the field) and the sound playing stays, with its A and B. Turned left, it
  is ITS OWN again. The lock is saved with the settings. A sound worth keeping also fits in SOUNDS or in A.

**SOUNDS (LFO again, from JOURNEY)** is a library of eight sounds, apart from the journeys. A sound is
everything the SHAPE pages, the holds, DRONE 2 and TIDES set; not the scenes, and not the FX page.

- **SAVE** on a slot stores the sound playing.
- **SEL** brings a slot's sound into the journey playing, gliding in over two seconds, so it can be done
  mid-performance.
- So any sound plays over any journey's scenes, and two of them can be A and B.

## 5. The loop

The looper records what is played: VOX's notes, the kick and BLOOM. It plays them back on the clock, in
layers.

| Press | What happens |
| --- | --- |
| **REC** (no loop) | Starts recording. The clock starts too, if it is stopped |
| **REC** again | The loop closes, its length rounded to the nearest beat, and it plays |
| **REC** while it plays | Overdubs a new layer over it |
| **REC** again | Keeps the layer |
| **PLAY** | Stops the loop, or plays it from its start |
| **PLAY** held and let go | Undoes the last layer. With only one layer, the loop goes |
| **PLAY** held, **ALGO** turned | Half speed, as played, or double speed |
| **REC and PLAY together** | Clears the loop |

A loop can be up to 16 bars long, with up to 8 layers.

**The loop follows the scene.** It keeps its notes as degrees of the scene's key, not as fixed pitches. On
another scene, or during a MORPH, the same tune plays on in the new key.

**LOOP: the layers.** SEL pressed on HOME opens LOOP, a row for each layer: the sound it plays with, how many
notes it has, and anything set on it. The row of a layer sounding is lit. SELECT picks a layer, and the knobs
are that layer's, on two pages. **ENV** there turns from one page to the other.

| Knob | First page | Second page |
| --- | --- | --- |
| K1 | LEVEL: 100% as played, down to silent, up to 127% | PAN: the layer's place, over VOX's own (MID: as VOX is) |
| K2 | SOUND: PLAYED (each note the VOX sound it was played with), or GLASS, REED, HALO or LOW for the whole layer | DRIFT: STILL, or how far the layer sways round its place on the tide, each layer a quarter tide behind the one before |
| K3 | OCTAVE: one down, as played, or one up | TONE: the layer's filter, darker to the left, brighter to the right (with a little resonance); 0% as its sound |
| K4 | MUTE: right, the layer is silent and stays in the loop; left, it plays again | SWELL: the layer's envelope, shorter and plucked to the left, slower in and longer out to the right; 0% as its sound |

**ALGO** sets the layer's LENGTH: AS THE LOOP, or its own, from 1 to 64 beats. Shorter, it comes round
before its end and the rest is cut; longer, a rest follows it. Either way it drifts against the other
layers and lines up differently each time round, like tape loops of unequal length.

A row shows a layer's place (L or R), a ~ when it drifts, a * when its TONE or SWELL is changed, and its
length in beats (6B) when it has its own.

**SEL** twice takes the layer shown out of the loop; the layers over it move down, keeping their settings.
Taking out the only layer clears the loop. The layers sound apart, so the same phrase recorded twice, one
layer an octave up in another sound, is two parts. The kick and BLOOM in a layer follow its LEVEL and MUTE.
All of it is saved with the loop.

**Each note keeps its sound.** A note loops with the VOX sound it was played with, whichever sound is in
hand later. A layer of GLASS, then one of REED over it, then HALO played by hand: three instruments. Shaping
a sound still changes it everywhere it plays, the loop included. VOX has eight voices in all, shared by
the hands, the loop and the sounds that sing: the oldest note gives way when a ninth begins. (A loop recorded
before 1.5 plays with the sound in hand, as it did.)

**The loop is kept.** It is saved with the journey playing, so it comes back after a power cut, and it plays
again when the clock starts. Stopping the clock silences it until the next start. User journey slots do not
hold a loop.

## 6. The keys

### White keys: VOX

The white keys play VOX in the scene's key and scale, so there are no wrong notes. VOX has eight voices.
**OCT-** and **OCT+** move the keys' octave, two octaves either way.

VOX also plays notes from MIDI, on SYSTEM's MIDI channel (1 by default). It takes pitch bend too. For
playing with a DAW, see section 9.

### Black keys 1-5: the drone

Each of the first five black keys is a drone voice. Each voice is one of two kinds:

- **SWELL**: the voice comes and goes. Each press swells it in, holds it a moment, and lets it out again.
  While the clock runs, the journey also swells it in by itself every few bars. ECLIPSE starts with all five
  like this. ALIVE's SWELLS on HOME sets how many.
- **HELD**: the key latches the voice on, and pressing it again lets it fall away. A held voice latches when
  the key is let go, so holding the key to shape the voice does not toggle it (section 3). The other
  journeys start with most of their voices held.

A voice's key held with **ALGO** turned switches it: left for HELD, right for SWELL.

The journey sets the drone's notes from the scene: its root (the *pedal*) and its voicing. The drone holds
those notes the whole scene. On another scene only the notes that must change move: one voice at a time,
the top first and the bass last, three seconds apart, each sliding slowly into place.

### Black key 6: the kick

A deep kick, tuned to the key. It's at its best rare: a low thud under the drone, or a slow pulse in a loop.
Held, its key puts the kick under the knobs (section 3). SHAPE's PULSE and PULSE 2 pages have all of it.

- **TUNE** keeps it in the scene's key: on the root, the fourth, the fifth, or an octave up, following each
  key change.
- **DRIVE** (PULSE 2) takes it from a soft round thud to a heavier, grittier hit that cuts through a thick
  drone.
- **DUCK** (PULSE 2): each kick dips the drone, VOX and the wind for a moment, and they swell back over about
  a third of a second. A little gives the drone a slow breath; a lot, a deep throb. The kick and BLOOM stay
  whole.
- **The kick by itself.** With its key held, **ALGO** sets it playing on the clock (also PULSE 2's BY
  ITSELF): A HEARTBEAT (a soft double beat each bar), EVERY BAR, EVERY HALF BAR, or EVERY 2 BARS; back left,
  BY HAND again. It plays only while the clock runs, is not recorded by the looper, and is saved with the
  journey.
- **A kick on each new key.** SCENE's LANDS (the journey's, like TRAVELS) lands a kick as each new scene's
  key comes in, by PRESET, MORPH or travelling by itself.
- **From MIDI** (note 41 on the channel after VOX's), it hits as hard as it is played. BLOOM too (note 42).

### Black key 7: BLOOM

A deep, slow swell in the drone's key. Two sine tones a few cents apart beat against each other, with a soft
fifth above them. It swells in over about two seconds, then fades into the reverb. Pressed again while it
sounds, it grows from where it is.

**BLOOM by itself.** With BLOOM's key held, **ALGO** turned right sets it swelling in by itself, every so
many bars: RARELY, NOW AND THEN or OFTEN; back left, BY HAND again (also ALIVE's BLOOMS on HOME). It plays
only while the clock runs, is not recorded by the looper, and is saved with the journey. TOTALITY blooms
rarely as made.

### Black keys 8-11: VOX's four sounds

Each key picks one of VOX's sounds, even mid-phrase. A note keeps the sound it began with, so notes already
sounding ring on as they were while new ones take the new sound.

| Key | Sound | Character |
| --- | --- | --- |
| 8 | **GLASS** | Two soft saws, a little apart: VOX as made |
| 9 | **REED** | A narrow pulse through a band-pass, closer and woody |
| 10 | **HALO** | Wide, an octave above, long and far back |
| 11 | **LOW** | A square an octave below, gliding from note to note |

The key of the sound playing is lit. Each journey keeps its own four sounds.

**A sound can sing by itself.** With a sound's key held, **ALGO** turned right makes that sound sing: a few
soft, slow notes in the scene's key, a phrase now and then, with long rests between. Further right it sings
more often (RARELY, NOW AND THEN, OFTEN); back left, less, then not at all (also ALIVE's SINGS on HOME, for the
sound playing). Each of the four sounds has its own setting, so several can sing at once, each with its own
phrases, and the white keys can play another over them. SHAPE's **VOX SINGS** page sets the rest for the sound
playing: how often it SINGS, its PLACE in the field, how far it DRIFTs round that place on the tide, and its
OCTAVE (one down, as is, one up). It sings only while the clock runs, its notes are not recorded by the
looper, and it is saved with the journey. As made, nothing sings.

## 7. SHAPE: the whole sound

**ENV** opens SHAPE: every setting of every part, four knobs a page, with **ALGO** setting that part's level.
ENV again (or SELECT) turns to the next page. The VOX pages shape the VOX sound playing. After something is
shaped through a hold (a drone key, a VOX sound key, BLOOM's key, a white key), ENV opens that part's page.
HOME's DRONE, VOX and MOTION pages are a selection of these.

Values are shown as percentages: a share from 0 to 100%, an offset from as made as -/+% (0%: as made), and
a LEVEL as 100% as made (up to 127%). Times, octaves and note lengths keep their own units.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| DRONE | FILTER | BODY (resonance) | SWARM (the crowd, and the voices' detune) | WAVE (soft to reedy) |
| DRONE 2 | RATE (its slow LFO, ~1 s to 100 s) | SWEEP (the LFO over the filter) | WIDTH (the voices swinging apart) | DRIFT (their tuning wandering) |
| VOX | BRIGHT | ATTACK | RELEASE | SHIMMER (how far into the reverb) |
| VOX 2 | WAVE (saw, square, narrow pulse) | FILTER (low-pass to band-pass) | RESO | GLIDE |
| VOX 3 | DETUNE | B OCT (the second oscillator: -1, 0, +1) | B MIX | ENV (how far the envelope opens the filter) |
| VOX SINGS | SINGS (NOT, RARELY, AT TIMES, OFTEN) | PLACE (in the field) | DRIFT (round it, on the tide) | OCTAVE (-1, 0, +1) |
| PULSE | DROP (the kick's fall in pitch) | DECAY | CLICK | SPACE |
| PULSE 2 | TUNE (root, 4th, 5th, octave) | DRIVE | DUCK (how far each kick dips the rest) | BY ITSELF (HAND, HEART, 1 BAR, HALF, 2 BARS) |
| TEXTURE | COLOUR | SWEEP | RATE | WIDTH |
| TIDES | RATE (8 s to 4 min a tide) | DRONE (how far it moves the drone's filter) | SPACE (the sends) | VOX (its brightness) |
| PAN | DRONE | VOX | PULSE (and BLOOM) | TEXTURE |
| PAN 2 | DRONE (how far it sways: STILL to 100%) | VOX | PULSE | TEXTURE |

**TIDES** is one slow swell under everything: the drone opens and closes, the space breathes in and out,
VOX brightens and darkens, all on the same tide. On its page **ALGO** sets its SHAPE: from EVEN (a smooth
sine) to WANDERING (it drifts to a new height each half tide). The line under the knobs shows where the
tide is now. As made, it moves the drone and the space gently, about once every 50 seconds.

**PAN** places each part in the stereo field, from LEFT 100% through MIDDLE to RIGHT 100%. The parts glide
to where they are put, so they can be moved during a performance. The drone's five voices keep their own
spread around the drone's place. On this page **ALGO** sets RATE, how fast the parts sway: at TIDE (all the
way down) they move with the tide, its shape and all; turned up, they get a steady cycle of their own, from
60 seconds down to half a second, so even a part on a slow tide can move audibly. The screen shows each
part's lane, and where the sway has taken it.

**PAN 2** sets how far each part sways, from STILL to 100% (all the way out to each side). When VOX sways,
its echoes follow it half as far. **ALGO** sets OFFSET: at IN SYNC (0%) every swaying part moves together;
turned up, each one follows the one before it (in the order DRONE, VOX, PULSE, TEXTURE) up to half a cycle
behind. At 100% two swaying parts mirror each other, one going left as the other goes right. At 50% each
is a quarter of a cycle behind the last, so four swaying parts take turns round the field. The screen dims
the parts that stay still.

As made, every part is in the middle and VOX sways 25% on the tide, in sync. Journeys from 1.1 and earlier
come in with no sway, so they sound as they did.

BLOOM's settings are under its own hold (black key 7). PULSE's level sets the kick's and BLOOM's together.

**SEL twice** on a SHAPE page puts that page back as made:

- DRONE also resets the drone voices' own settings.
- The VOX pages reset the VOX sound playing.
- PULSE also resets BLOOM; PULSE 2 puts the kick back on the root, its DRIVE as made, no DUCK, by hand.
- DRONE 2, TIDES, PAN and PAN 2 reset only their own page.

The first press only asks, so nothing resets by accident.

## 8. FX, SYSTEM and saving

**FX** sets the reverb and the delay. SELECT picks the row; the knobs change it. HOME's SPACE page plays four
of these.

| Row | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| REVERB | DECAY | SIZE | SHIMMER | TONE |
| REVERB 2 | MOTION (how much its lines move) | LOW CUT (on what goes in) | INTERVAL (the shimmer: an octave, a fifth or a twelfth up) | LEVEL (its return) |
| DELAY | TIME (a note length, on the tempo) | FEEDBACK | TONE | WEAR (the tape's age) |
| DELAY 2 | LEVEL (its return) | LOW CUT | PING-PONG (on: the echoes go left, right) | WARP (how far the WARP throw bends it: up to two octaves) |

A change of SIZE glides: the reverb's lines slide to the new size, so the room breathes and never clicks.
A new delay TIME, or a new tempo, is taken once the knob stops: the echoes cross-fade to it and stay on
pitch. Only the WARP throw bends them. Each scene can move the reverb further with its ROOM and HALO
(section 4).

**SYSTEM (GLO)**: SELECT moves through the list, and a knob or SEL changes the row shown.

| Setting | What it does |
| --- | --- |
| MIDI CLOCK OUT | Sends MIDI clock and start/stop while the clock runs |
| FOLLOW CLOCK IN | Follows MIDI clock and start/stop from outside |
| MIDI CHANNEL | VOX's notes, the CCs and program changes, 1-16, or off; the black keys use the next channel (section 9) |
| MIDI NOTES OUT | Sends the notes played |
| MIDI CC OUT | Sends the CCs and program changes as things change |
| KNOBS | PENUMBRA, or CC ABSOLUTE or CC RELATIVE: on HOME the seven knobs are a MIDI controller (section 9) |
| 2ND CORE | Uses the FM-1's second processor core; applies at the next power-on |
| DRONE CORE | Which core runs the drone voices; SPLIT balances itself |
| OUTPUT LEVEL | NORMAL, or LOUD (6 dB hotter) |
| USB AUDIO | MASTER (follows the volume knob), FULL level, or USB ONLY (below) |
| PLAY AT POWER-ON | Starts the clock and the drone at power-on |
| BEND RANGE | The pitch bend's range: 2, 5, 7 or 12 semitones each way |
| SLEEP | Off, or 20, 40, 60 or 90 minutes (below) |
| RESET SOUND | SEL twice puts the whole sound back as made (below) |
| ABOUT | The version |

**USB ONLY** sends USB audio at full level, as FULL does, and while a computer is taking that audio the
FM-1's own outputs (the speaker, the headphones and the line out) fade to silence. With no computer taking
it, unplugged or on battery, the FM-1 plays as FULL. The speaker is also silent whenever a plug is in the
headphone jack.

**SLEEP** lets everything go, gently:

- Three minutes before the end, the loop stops and FREEZE lets go.
- Then a drone voice falls away every 30 seconds, the top voice first.
- At the end the clock stops.

SLEEP lasts only this session; it is not saved.

**RESET SOUND** puts back as made:

- every SHAPE page, the drone voices (all five coming and going, as in ECLIPSE) and VOX's four sounds;
- the FX page and the macros (CCs 14-17);
- any throw or bend that is out.

The scenes, the loop, the tempo, A and B, the sound library and the other settings stay.

**BENCH**: GLO again from SYSTEM. It shows what each part of the sound costs the processor. SEL there runs
a load test that sets every voice at its heaviest.

**Saving** happens by itself once the sound has been silent for a second and nothing has changed for two, so
after a STOP. SAVE saves at once. Everything is kept: the journey playing, its sound, its loop and the
settings.

**Backups.** The [Backup page](https://madmooneyham.github.io/Penumbra/backup/) (Chrome or Edge, the FM-1
connected by USB) copies everything Penumbra keeps to one file on the computer: the eight user journeys, the
sound library, the journey playing with its A and B, the loop and its layers, and the settings. The same file
puts any of them back, part by part, onto this FM-1 or another; Penumbra then starts again with them. A backup
from an older Penumbra comes over upgraded, as its journeys would; a part from a newer one is not taken, and the
FM-1 keeps what it had. The panel's calibration stays the unit's own. The emulator saves and restores the same
file, so a journey made in the browser can go onto the FM-1, and back.

**Leaving Penumbra.** Another firmware installs the usual way; Penumbra carries the update loader. If the
FM-1 ever won't take an install, **OCT-** and **OCT+** held together for five seconds put it in update mode.

## 9. MIDI and a DAW

Connected by USB, the FM-1 is an audio device and a MIDI device to a computer. The TRS jack takes MIDI in
too. Everything below is on SYSTEM's MIDI CHANNEL (channel *N*, 1 by default), except the black keys, which
use the channel after it (*N*+1).

**The clock.** With FOLLOW CLOCK IN on, the DAW's clock runs the tempo, and its start, stop and continue
run the transport. Penumbra times each tick as it arrives, so it holds the DAW's tempo to about a tenth of
a BPM, and the delay's echoes stay put. SONG POSITION is followed too: when the DAW locates or loops, the
clock and a playing loop carry on from there. With MIDI CLOCK OUT on and nothing to follow, Penumbra sends
its own clock.

**Notes.** VOX plays notes on channel *N*, and sends the notes played on the white keys there (MIDI NOTES
OUT). The black keys go out on channel *N*+1 as notes 36 to 46, one for each key from black 1 to black 11.
Notes 36 to 46 coming in on that channel press those keys, so the DAW can play the drone, the kick, BLOOM
and the VOX sounds back exactly as they were played. The loop's own notes are not sent; the hands' were, as
they were played. A DAW track recording the FM-1's notes and sending them back to it plays every note twice,
so its output belongs elsewhere. ALL NOTES OFF (CC 123) ends VOX's notes.

**CCs.** Each of these can be automated by the DAW. A change on the FM-1 (by a knob, A/B, a load or a throw)
sends its CC (MIDI CC OUT), so the DAW can record the moves. A CC coming in is never sent back.

| CC | Control |
| --- | --- |
| 14-17 | The macros: DRONE (filter and body), VOX (brightness), SPACE (reverb and delay), TONE (all brighter or darker) |
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
| 52-55 | HOME's DRONE page: FILTER, BODY, SWARM, WAVE |
| 56-59 | HOME's VOX page, the sound playing: BRIGHT, FILTER, RESO, RELEASE |
| 60-63 | HOME's ALIVE page: SINGS (the sound playing), BLOOMS, SWELLS, TRAVELS |
| 81-83 | The kick: DROP, DECAY, CLICK |
| 44-46 | The kick: TUNE, DRIVE, DUCK |
| 119 | The kick: BY ITSELF |
| 3 | MORPH (64: at the scene; above, toward the next; below, toward the one before) |
| 9 | SCENE: 0-15 the first, 16-31 the second, and so on (it glides there, as PRESET does) |
| 118 | A/B: where the sound sits between A and B (with both kept) |

A throw set by a CC stays where the DAW puts it; it does not drift back. Settings of a few steps (the FX
page's switches, the kick's TUNE and BY ITSELF, ALIVE's four) spread their steps over 0 to 127. So everything
HOME's pages, the kick, MORPH, PRESET and A/B do can be recorded in the DAW and played back, whatever page the
FM-1 shows. A PRESET move sends both SCENE's CC and its program change. The macros (14-17) are MIDI's own: each 0% as
made, with the journey.

**The knobs as a controller.** With SYSTEM's **KNOBS** set to CC ABSOLUTE or CC RELATIVE, HOME's seven knobs
control other instruments and plugins in the DAW instead of Penumbra, each on its own CC on the MIDI channel,
whether or not MIDI CC OUT is on:

- **CC RELATIVE** sends each turn as how far and which way (binary offset: 65 and up clockwise, 63 and down
  back), so the DAW's control moves from wherever it is, with no end on the FM-1's side. In the DAW's MIDI
  mapping this is called relative binary offset (Ableton: Relative (Bin Offset); REAPER: Relative 2). The
  knob strip shows each knob's last turn.
- **CC ABSOLUTE** keeps a value from 0 to 127 for each knob and sends it. A DAW that sends the CC back (its
  controller feedback) moves the knob's value too, so they agree after the mouse or automation has moved it;
  without that, the FM-1's value is where the knob stops. The knob strip shows each value.

| Knob | CC |
| --- | --- |
| K1, K2, K3, K4 | 74, 75, 76, 77 |
| SELECT | 78 |
| ALGO | 79 |
| PRESETS | 80 |

- Penumbra's sound, scene and page stay as they are while the knobs turn; Penumbra takes these CCs for
  nothing else.
- The holds (FX, a key held) and every other page are Penumbra's as always, so its sound can still be shaped,
  and SYSTEM reached, with the knobs as a controller.
- ABSOLUTE's values are kept with the settings. A click moves a value two steps, RELATIVE one; a knob
  spinning quickly goes further, as everywhere.

**Program changes**, both ways:

| Program | Does |
| --- | --- |
| 0-7 | Travel to scene 1-8 of the journey playing |
| 8-14 | Open preset journey 1-7 |
| 16-23 | Open user journey 1-8 (if saved) |

## 10. Cheat sheet

| Control | Does |
| --- | --- |
| K1-K4 (HOME) | The page's: DRONE, VOX, MOTION, SPACE, MIX or ALIVE |
| ALGO | HOME: the knobs' next page; elsewhere tempo (SHAPE: level; holds: see section 3) |
| SELECT (HOME) | MORPH |
| PRESET | Next or previous scene |
| FX | FX page; held: throws (DIP both ways), + ALGO: WARP |
| ENV | SHAPE, TIDES, PAN, PAN 2 (again: next page; SEL twice: reset); held: A/B |
| Black 1-5 | Drone voices (held: shape; + ALGO: HELD or SWELL) |
| Black 6 / 7 | Kick / BLOOM (held: shape; + ALGO: by itself) |
| Black 8-11 | VOX sounds: GLASS, REED, HALO, LOW (held: shape; + ALGO: sings) |
| White keys | VOX, in the key (held: bends; + ALGO: pitch) |
| ARP | FREEZE |
| SEQ | Clock start / stop |
| REC | Loop: record, play, overdub, keep |
| PLAY | Loop: stop / play; held: undo; held + ALGO: speed |
| REC + PLAY | Clear the loop |
| EDIT | SCENE, ROOM, HALO (SEL: store the moment) |
| LFO | JOURNEY (SEL: open; SAVE: into a user slot); again: SOUNDS |
| GLO | SYSTEM, then BENCH |
| SAVE | Save now; in a hold: keep it |
| OCT- / OCT+ | White keys' octave; both for 5 s: update mode; ENV held: keep A / B |
| Any button, held | Says what it does |
