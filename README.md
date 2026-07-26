# SomeChops

<img src=https://raw.githubusercontent.com/echoe/somechops/refs/heads/main/picture.png width="600" height="480" />

A JUCE plugin: load a sample, auto-slice it at transients into a playable
drumkit, sequence it with a 16-step per-pad sequencer (ratchet/pitch/
probability per step, 32 pattern slots), and save/load presets that embed
the sample, slices, and full sequencer state.

(Renamed from the original working title "DrumChop" — some of the history
further down still refers to that name where it's describing what changed
at the time.)

## Status

This is a first complete pass at the whole signal chain and UI, written and
reasoned through carefully, but **not yet compiled** — I don't have a JUCE
checkout or an audio host in this sandbox to build and click-test against.
Treat this the way you'd treat a big diff from a pairing session: read it,
build it locally, and let's fix whatever the compiler/DAW turns up. I've
flagged the parts I'm least sure about below rather than pretending
otherwise.

## Building

```
./build.sh
./run.sh
```

`build.sh` wipes `build/` and does a fresh Release CMake configure + build.
`run.sh` launches the built Standalone app directly. `CMakeLists.txt` uses
your local JUCE checkout at `$HOME/JUCE` if it exists, otherwise fetches JUCE
8.0.13 via `FetchContent` automatically — no manual clone step either way.
It already carries over the `JUCE_WEB_BROWSER=0` / `JUCE_USE_CURL=0` flags
from your Linux/GTK build fix on other projects.

## Architecture

- **TransientDetector** (`TransientDetector.h/cpp`) — spectral-flux onset
  detection: STFT via `juce::dsp::FFT`, adaptive local mean+stddev threshold,
  minimum inter-onset gap. Returns raw sample positions; no ML, just DSP.
- **DrumSampler** (`DrumSampler.h/cpp`) — owns the loaded source buffer and up
  to 32 `Slice`s (start/end/trimmedEnd). `trimmedEnd` is the adjustable
  "sample length" per slice — shortenable from the waveform view without
  losing the original transient-detected end (so you can lengthen it back).
  Polyphonic (24 voices) with linear interpolation for pitch-shifted ratchet
  hits, and a short fade-out at the trimmed end to avoid clicks.
- **Sequencer** (`Sequencer.h/cpp`) — 12 pads × 16 steps (one pad per note in
  an octave), each step has
  `enabled`, `ratchet` (1–8 subdivisions), `pitchSemitones`, `probability`.
  32 `Pattern`s live in a `PatternBank`-style array; switching patterns is
  instant (no realloc). `randomizeTrack`/`randomizeAllTracks` take
  density/pitch-range/max-ratchet knobs. Clock derives step length from host
  BPM (falls back to 120 if the host doesn't report one) and schedules
  ratchet hits via a small pending-hit queue so hits that fall in a later
  audio block still land on time.
- **PresetManager** (`PresetManager.h/cpp`) — one `.dchp` XML file with the
  sample embedded as base64 WAV, all slices, and all 32 patterns (steps
  packed as compact `enabled:ratchet:pitch:probability` tokens). Also used
  directly for the host's `getStateInformation`/`setStateInformation`, so a
  saved DAW project restores everything too.
- **PluginProcessor/Editor** — MIDI note-on (notes 36–43 → pads 1–8) and
  mouse clicks both trigger pads; the editor has a waveform view with
  draggable slice-start (white) and trim/length (orange) markers, an 8-pad
  grid, the 16×8 step grid (click toggles a step and selects it for the
  ratchet/pitch/probability sliders below), pattern selector, randomizer,
  and load/save for both raw samples and `.dchp` presets.

## Recent additions

- **Manual start/end sliders per pad**: a row of two-thumb sliders sits directly
  above the pad grid — drag the left thumb to move a slice's start sample,
  the right thumb to set its adjustable end/length, as a numeric-feeling
  complement to auto-slice and dragging the waveform markers directly.
- **Play/Stop button**: the sequencer now runs if *either* the host transport
  is playing *or* this button is active — so it works in a DAW and standalone
  alike. Pressing Play resets the step counter to a clean start.
- **MIDI keyboard input**: note-on messages for MIDI notes 36–47 (C2 up to
  B2, a full octave, under the standard octave-numbering convention where
  note 60 = C4) trigger pads 1–12 directly, velocity-sensitive.
- **Pattern-change quantization toggle**: "Wait for pattern end" — off,
  selecting a new pattern switches immediately; on, the switch is queued and
  only takes effect once the currently playing pattern finishes its 16 steps.
- Along the way, fixed a pre-existing off-by-one in the sequencer clock:
  step index 0 of a pattern was never actually played (playback effectively
  started from step 1) because the step counter only advances *after*
  crossing a step boundary. Now starts cleanly at step 0.

- **(Superseded)** A per-track "tune" slider/column was added at one point,
  then removed in favor of the per-sample + per-step pitch model described
  further down — see the "Pitch, clarified" section below for how it
  actually works now.
- Confirmed all sequencer controls (sensitivity, density, pitch range, max
  ratchet, and the per-step ratchet/pitch/probability sliders) are already
  `LinearHorizontal` sliders, not rotary knobs — nothing to change there.

- **Choke/Poly toggle**: "Choke (mono)" in the top bar. Poly (default, off)
  lets overlapping hits ring out together, same as before. Choke, when on,
  makes every new trigger quickly fade out (over ~1.5ms) whatever's currently
  sounding first, so only one voice plays at a time across the whole kit —
  handy for things like a closed-hat-style choke or just a tighter, more
  "single-voice" feel. Saved in presets.

- **BPM control**: top-right of the top bar. Acts as the tempo whenever the
  host doesn't report one (standalone, or hosts that omit it); host tempo
  still takes priority when available. Saved in presets and synced with the
  host's own save/restore.
- **Longer ratchet/probability sliders**: the per-step editor's Ratchet and
  Probability sliders are now much wider with visible numeric text boxes, so
  exact values are easy to read at a glance.
- **Pitch, clarified**: there are now two independent, additive pitch layers,
  and no per-track "lane" pitch (that concept is gone for good):
  - **Per-sample pitch** (the slider above each pad's start/end slider):
    permanently retunes that slice's own playback speed, no matter how it's
    triggered — sequencer, MIDI, or pad click.
  - **Per-step pitch** (back in the selected-step editor at the bottom,
    alongside Ratchet and Probability): an additional offset for that one
    step only, on top of whatever the sample's own pitch is set to. The
    randomizer's "Pitch Range" control randomizes this per-step value again.
  A step's actual pitch when triggered is simply sample pitch + step pitch.

- **Layout cleanup**: randomization (Randomize All, Clear Pattern, and the
  Density/Pitch Range/Max Ratchet sliders) now has its own row, so those
  sliders could be made much longer. The Ratchet/Pitch/Probability sliders in
  the step editor are now all the same width. Window enlarged to 1200×830 to
  fit it all comfortably.
- **MIDI pattern switching + start/stop, for live performance**: with default
  note assignments —
  - **Pads**: C2 upward (unchanged), one note per pad.
  - **Pattern switch**: C3 upward, one note per pattern — by default only
    C3 through A#3 (11 notes/patterns) are mapped, leaving B3 and C4 free.
    Honors the same "Wait for pattern end" toggle as the on-screen selector.
  - **Start/Stop**: B3 = Start (also resets to step 0), C4 = Stop.
  I picked these defaults specifically so nothing collides out of the box —
  32 patterns would need notes all the way up to G5 if every pattern got a
  key, which would run right over B3/C4. If you want more patterns
  MIDI-reachable, the settings page lets you raise "Pattern Note Count" and
  move Start/Stop notes up out of the way.
- **Settings page**: new "Settings" button, top bar, right next to BPM. Opens
  a full-screen overlay to remap all five MIDI note assignments (Pad Base
  Note, Pattern Base Note, Pattern Note Count, Start Note, Stop Note) with
  live note-name readouts (e.g. "B3 (59)"). Saved in presets and synced with
  the host's own save/restore, just like BPM and choke mode.

- **12 pads instead of 8**: one per note in an octave. `kNumPads` in
  `DrumSampler.h` is still the single place controlling this.
- **Per-lane step count (polymeter)**: a small slider column to the left of
  the step grid sets each lane's own length, 1–16 steps. All lanes stay
  locked to the same tempo clock, but a lane shorter than 16 loops back to
  its own step 0 sooner than the others — so an 11-step lane against a
  16-step lane phases differently every time around, rather than always
  resetting together. Steps beyond a lane's current length are dimmed in the
  grid (not hidden) — the data's still there if you lengthen the lane again.
  The "current step" highlight is now per-row for the same reason: each
  lane can be sitting at a different position in its own loop at any given
  moment. Saved per-pattern in presets; old presets without this default to
  the full 16 steps.

- **Per-step timing nudge**: a fourth slider ("Nudge", -50 to +50) next to
  Ratchet/Pitch/Probability in the step editor — shifts that step's whole hit
  group (including any ratchets) earlier or later, as a percentage of one
  step's duration, for a more human/groove feel. This only actually does
  anything now because hit timing was made sample-accurate at the same time
  (see the resolved rough edge above) — nudging a step that's already
  triggering at the top of every block regardless of its real position
  wouldn't have been audible. One limitation worth knowing: a large negative
  nudge on a step landing right at the very start of an audio block would
  need to reach back into already-rendered audio from the previous block,
  which isn't possible — those get clamped to the start of the current block
  instead of shifting earlier. In practice this only bites at small block
  sizes combined with close-to-maximum negative nudge values.

## Known rough edges / things I'd tackle next

1. **(Resolved)** Ratchets and hit timing used to be block-quantized rather
   than sample-accurate. Fixed as part of adding per-step nudge (see below) —
   `DrumVoice` now has a `delaySamples` counter, and `renderNextBlock` holds
   off writing/advancing a freshly-triggered voice until that many samples
   have elapsed within the block. Every sequencer hit (ratchets included) now
   starts at its exact computed sample offset.
2. **(Resolved)** Was "8 pads, not more" — now 12, one per note in an
   octave. `kNumPads` in `DrumSampler.h` is still the single place
   controlling pad/track count if you want to change it again.
3. **UI is functional, not pretty.** Plain JUCE components, no custom LookAndFeel
   yet — deliberately, so we can get the engine right first the way you've
   done on OAO/SimpleVocals, then pass over styling once the logic's solid.
4. **`getChildWithTagNameIterator`** in `PresetManager.cpp` requires a
   reasonably recent JUCE (7.x+). Shout if you're pinned to an older version
   and I'll swap it for the `forEachXmlChildElementWithTagName` macro.
5. No stereo-width/pan per pad, no per-slice output routing, no filter/ADSR
   on the voices yet — kept the voice engine minimal so the sequencer/preset
   logic could be the focus of this pass.
6. **Quantized pattern-change UX nuance**: with "Wait for pattern end" on,
   the pattern selector and step grid immediately show the *target* pattern
   you just picked, even though the currently *playing* pattern (the one
   actually queued in the audio thread) hasn't switched yet. So editing
   steps right after selecting a new pattern edits the target pattern, not
   what's currently sounding — which is probably what you want, but worth
   knowing. A "pending" indicator on the selector would clear this up if it
   becomes confusing in practice.

Happy to jump straight into fixing whatever the first compile throws up.
