# SomeChops

<img src=https://raw.githubusercontent.com/echoe/somechops/refs/heads/main/picture.png width="600" height="480" />

A JUCE plugin: load a sample, auto-slice it at transients into a playable
drumkit, sequence it with a 1 to 16-step per-pad sequencer (ratchet/pitch/
probability/nudge per step, 32 pattern slots), and save/load presets that embed
the sample, slices, and full sequencer state.

Adjust slice timings manually, by hand as well as with numbers. Play through slices on one octave of keys, then start and stop the sequencer and switch between patterns with the keyboard.

Randomize the patterns. Play some chops.

This entire plugin is basically based off of my memory of having the Teenage Engineering OP-1, which I hated other than its sample handler. You had to give it 12-second .wav files that it would automatically slice. Works well with breakbeats but you can also just sort of put whatever in if you really want, as long as the sample is long enough!

## Walkthrough

- On the top bar, load a sample, auto-slice, save and load presets, change the BPM the sequence plays at (in standalone mode), change the play mode from polyphonic to mono (choking all other hits), play and stop the sequencer, and open the settings page [which lets you change MIDI note settings and theme].

- The top screen is where you do the slicing. The plugin shows your sample there, and auto-slices using a transient detector: you can adjust the slice sensitivity and reslice if you think it got it wrong, or you can manually adjust each slice yourself per sample.

- The bottom screen is where the sequencing of the slices happens, in a twelve-lane up to 16-step sequencer. Press 'Randomize All' to randomize everything about the notes that you want, and mess around. Click a step to adjust all of its parameters using the bottom sliders and boxes, including pitch (which stacks with the per-slice pitch), nudging slices left/right, probability, and ratchet amount (1x to 8x). 

- Toggle whether a pattern switch happens instantly, or waits to land on the beat. Have up to 32 patterns and switch between them to make a song ... or just have fun, IDK. It's pretty fun!

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

## Status

- I think this works well actually, and it's even themed now. Maybe will add choke groups, but that's about it.
