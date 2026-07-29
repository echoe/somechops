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

- On the top bar, load a sample, auto-slice, save and load presets, play and stop the sequencer, open settings (change MIDI note settings and theme), and change the BPM the sequence plays at (in standalone mode).

- The top screen shows the slices. The plugin shows your sample there, and auto-slices using a transient detector. You can adjust the slice sensitivity and reslice if you think it got it wrong, or if you want to slice at specific times you can move the sliders. You can also change the pitch of each slice up or down two octaves! If you want to set exact slicing or an exact pitch amount, you can click on the slice, then edit it with the edit options at the bottom.

- The bottom screen is where the sequencing of the slices happens, in a twelve-lane up to 16-step sequencer with up to 32 patterns. Press 'Randomize All' to randomize a pattern with a randomizer with some slidable options, and mess around. Click a step to adjust all of its parameters using the bottom sliders and boxes, including pitch (which stacks with the per-slice pitch), nudging slices left/right, probability, and ratchet amount (1x to 8x, combine with nudge if you want a ratchet in a specific place). 

- Toggle whether a pattern switch happens instantly, or waits to land on the beat. Also toggle whether a pattern switch starts everything on the 1 again. You can theoretically make a bunch of patterns and switch between them to make a song! Or have fun! Or drop a loop and just see what happens.

### Settings 
- You can change the MIDI note settings in the settings, and change the theme of the plugin. 
- By default the plugin maps each slice to the c2-b2 range, so you can play each slice in one octave. If you are using the sequencer, patterns 1-11 are mapped from c3-a#3, the start note is mapped to b3, and the stop note is mapped to c4. You can also of course click the buttons with the mouse.
- There are four themes: minimal (the default theme), cute, old-school, and futuristic.

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
