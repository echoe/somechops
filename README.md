# SomeChops

<img src=https://raw.githubusercontent.com/echoe/somechops/refs/heads/main/picture.png width="600" height="480" />

A JUCE plugin: load a sample, auto-slice it at transients into a playable
drumkit, sequence it with a 1 to 16-step per-pad sequencer (ratchet/pitch/
probability/nudge per step, 32 pattern slots), and save/load presets that embed
the sample, slices, and full sequencer state.

Adjust slice timings manually, by hand as well as with numbers. Play through slices on one octave of keys, then start and stop the sequencer and switch between patterns with the keyboard.

Randomize the patterns. Play some chops.

This entire plugin is basically based off of my memory of having the Teenage Engineering OP-1, which I hated other than its sample handler. You had to give it 12-second .wav files that it would automatically slice, from what I remember, and then there was a slice per sample. So the 'slice a single sample' thing is sort of intentional. Works well with breakbeats but you can also just sort of put whatever in if you really want, as long as the sample is long enough! But if you're editing the samples you'll want something else, this is more a 'see what you get from this file' type random slicer.

## Status

Mostly this works! There is some oddness moving slice points around that I'll continue to work on, and the plugin's a bit meh style-wise, but otherwise ... yeah. I think it's really fun right now, anyways

## Building
Once prerequisites are installed (see below), run build.sh on linux and mac, or build.bat on windows.

### Windows 
- I installed CMake and Git from the respective websites (using the windows executables) and then installed Visual Studio Community Edition: "Desktop development with C++" workload, and then the build went without issues.
### Linux (Fedora)
- Just install development tools, CMAKE dependencies, and then a whole bundle of tools for JUCE ...
- sudo dnf group install development-tools
- sudo dnf install cmake gcc-c++ git alsa-lib-devel freetype-devel fontconfig-devel libX11-devel libXinerama-devel libXext-devel libXrandr-devel
- sudo dnf install libXcursor-devel libXcomposite-devel gtk3-devel webkit2gtk4.1-devel freetype-devel curl-devel
### MacOS
- xcode-select --install (everything else is already there by default)

## Walkthrough

- The top is the actual slicing. Load a sample (.aif and .wav work at least, others should hopefully work as well). The plugin auto-slices using a transient detector: you can adjust the slice sensitivity and reslice if you think it got it wrong, or manually adjust each slice yourself. Save and load presets also if you want. There aren't choke groups [I don't care about them. If you do, complain and maybe I'll work on it??], but you can set the plugin to be mono and choke all other notes off if you want. Also adjust the BPM when this is playing standalone.

- The bottom is where the sequencing of the slices happens. Press 'Randomize All' to randomize everything about the notes that you want, and mess around. Click a step to then adjust all of its parameters on the bottom, including pitch (which stacks with the per-slice pitch). Toggle whether a pattern switch happens instantly, or waits to land on the beat. Have up to 32 patterns and switch between them to make a song ... or just have fun, IDK. It's pretty fun!

## Plugin Architecture (per the LLM)

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
