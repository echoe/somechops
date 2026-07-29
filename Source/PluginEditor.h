#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "SomeChopsLookAndFeel.h"

//==============================================================================
// Draws the loaded sample waveform with draggable slice-start markers (white)
// and draggable trim/length markers (orange, per-slice adjustable length).
class WaveformSliceView : public juce::Component
{
public:
    explicit WaveformSliceView (SomeChopsAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void rebuildWaveformCache();

private:
    SomeChopsAudioProcessor& processor;
    std::vector<float> minCache, maxCache; // downsampled peaks for fast painting

    enum class DragTarget { none, sliceStart, sliceTrim };
    DragTarget dragTarget = DragTarget::none;
    int dragSliceIndex = -1;

    int xToSample (int x) const;
    int sampleToX (int sample) const;
    int hitTestHandle (int x, int y, DragTarget& targetOut, int& sliceIndexOut) const;
};

//==============================================================================
class PadGrid : public juce::Component, private juce::Timer
{
public:
    explicit PadGrid (SomeChopsAudioProcessor& p);
    void resized() override;

    // Fired (in addition to triggering playback) whenever a pad button is clicked,
    // so the exact-value slice editor can show that slice's numbers.
    std::function<void (int pad)> onPadSelected;

private:
    SomeChopsAudioProcessor& processor;
    juce::OwnedArray<juce::TextButton> padButtons;
    void timerCallback() override;
};

//==============================================================================
// One two-value (min/max thumb) slider per pad, sitting directly above the pad
// grid, for manually setting each slice's start and end/length sample position
// as an alternative (or complement) to auto-slice and dragging the waveform markers.
// A small label above each slider shows its current start/end position in ms.
class SliceRangeRow : public juce::Component, private juce::Timer
{
public:
    explicit SliceRangeRow (SomeChopsAudioProcessor& p);
    void resized() override;

    // Fired whenever a slice's start/end changes via these sliders, so the
    // waveform view above can repaint its markers in sync.
    std::function<void()> onSliceChanged;

private:
    SomeChopsAudioProcessor& processor;
    juce::OwnedArray<juce::Slider> rangeSliders;
    juce::OwnedArray<juce::Label> rangeLabels;
    std::array<bool, kNumPads> userDragging {};
    void timerCallback() override;
    void updateLabel (int padIndex);
};

//==============================================================================
// One row of single-value sliders, one per pad, sitting above the SliceRangeRow
// (start/end sliders). Sets each sample/slice's own base pitch — permanently
// retuning its playback speed — rather than a per-step or per-track modulation.
class SlicePitchRow : public juce::Component, private juce::Timer
{
public:
    explicit SlicePitchRow (SomeChopsAudioProcessor& p);
    void resized() override;

    // Fired whenever a slice's pitch changes via these sliders, so the exact-value
    // slice editor can stay in sync if it's currently showing that slice.
    std::function<void()> onPitchChanged;

private:
    SomeChopsAudioProcessor& processor;
    juce::OwnedArray<juce::Slider> pitchSliders;
    juce::OwnedArray<juce::Label> pitchLabels;
    std::array<bool, kNumPads> userDragging {};
    void timerCallback() override;
    void updateLabel (int padIndex);
};

//==============================================================================
// One row per pad, 16 step toggle buttons per row. Clicking a step selects it
// for editing via the ratchet/probability sliders in the editor. Steps beyond a
// lane's own configured length (see TrackLengthColumn) are dimmed to show they
// won't play, but stay editable in case the lane is lengthened again later.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (SomeChopsAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int pad, int step)> onStepSelected;

private:
    SomeChopsAudioProcessor& processor;
    void timerCallback() override;
};

//==============================================================================
// One vertical stack of small sliders, one per pad/track, sitting to the left of
// the step grid — sets each lane's own step count (1..16). Lanes shorter than
// 16 loop independently against the others (polymeter) while staying locked to
// the same tempo clock.
class TrackLengthColumn : public juce::Component, private juce::Timer
{
public:
    explicit TrackLengthColumn (SomeChopsAudioProcessor& p);
    void resized() override;

private:
    SomeChopsAudioProcessor& processor;
    juce::OwnedArray<juce::Slider> lengthSliders;
    juce::OwnedArray<juce::Label> lengthLabels;
    std::array<bool, kNumPads> userDragging {};
    void timerCallback() override;
    void updateLabel (int padIndex);
};

//==============================================================================
// Full-editor overlay for configuring MIDI note assignments: pad triggers,
// pattern-switch notes (for live performance), and start/stop notes.
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (SomeChopsAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pulls current values from processor.midiSettings into the sliders. Call
    // before showing the panel, in case a preset load changed them elsewhere.
    void refresh();

private:
    SomeChopsAudioProcessor& processor;

    juce::Label title { {}, "MIDI Note Settings" };

    juce::Label padBaseLabel { {}, "Pad Base Note" };
    juce::Slider padBaseSlider;
    juce::Label padBaseNoteName;

    juce::Label patternBaseLabel { {}, "Pattern Base Note" };
    juce::Slider patternBaseSlider;
    juce::Label patternBaseNoteName;

    juce::Label patternCountLabel { {}, "Pattern Note Count" };
    juce::Slider patternCountSlider;

    juce::Label startNoteLabel { {}, "Start Note" };
    juce::Slider startNoteSlider;
    juce::Label startNoteName;

    juce::Label stopNoteLabel { {}, "Stop Note" };
    juce::Slider stopNoteSlider;
    juce::Label stopNoteName;

    juce::Label themeLabel { {}, "Theme" };
    juce::ComboBox themeSelector;

    juce::TextButton closeButton { "Close" };

    void layoutRow (juce::Rectangle<int> row, juce::Label& label, juce::Slider& slider, juce::Label* noteName);

public:
    // Fired when the theme selector changes, with the new UiTheme's int value.
    // The editor owns the actual LookAndFeel instance, so this just notifies it.
    std::function<void (int)> onThemeChanged;
};

//==============================================================================
class SomeChopsAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SomeChopsAudioProcessorEditor (SomeChopsAudioProcessor&);
    ~SomeChopsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SomeChopsAudioProcessor& processor;
    SomeChopsLookAndFeel lookAndFeel; // set via setLookAndFeel() in the constructor; must be
                                       // unset in the destructor before this is destroyed

    // Top bar
    juce::TextButton loadButton { "Load Sample" };
    juce::TextButton autoSliceButton { "Auto-Slice" };
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel { {}, "Sensitivity" };
    juce::TextButton savePresetButton { "Save Preset" };
    juce::TextButton loadPresetButton { "Load Preset" };
    juce::TextButton playStopButton { "Play" };
    juce::ToggleButton chokeModeToggle { "Choke (mono)" };
    juce::Slider bpmSlider;
    juce::Label bpmLabel { {}, "BPM" };
    juce::TextButton settingsButton { "Settings" };

    // Waveform + per-slice pitch + slice range sliders + pads
    WaveformSliceView waveformView;
    SlicePitchRow slicePitchRow;
    SliceRangeRow sliceRangeRow;
    PadGrid padGrid;

    // Sequencer area
    juce::ComboBox patternSelector; // 32 patterns
    juce::TextButton randomizeButton { "Randomize All" };
    juce::TextButton clearButton { "Clear Pattern" };
    juce::ToggleButton quantizePatternChangeToggle { "Wait for pattern end" };
    juce::Slider densitySlider, pitchRangeSlider, maxRatchetSlider, nudgeRangeSlider;
    juce::Label densityLabel { {}, "Density" }, pitchRangeLabel { {}, "Pitch Range" }, maxRatchetLabel { {}, "Max Ratchet" },
                nudgeRangeLabel { {}, "Nudge Range" };
    juce::ToggleButton randomizeLengthsToggle { "Randomize Lane Lengths" };

    StepGrid stepGrid;
    TrackLengthColumn trackLengthColumn;

    // Selected-step editor
    juce::Label selectedStepLabel { {}, "No step selected" };
    juce::Slider stepRatchetSlider, stepPitchSlider, stepProbabilitySlider, stepNudgeSlider;
    juce::Label stepRatchetLabel { {}, "Ratchet" }, stepPitchLabel { {}, "Pitch" }, stepProbLabel { {}, "Probability" }, stepNudgeLabel { {}, "Nudge" };
    int selectedPad = -1, selectedStep = -1;

    // Exact-value slice editor: type precise numbers instead of dragging sliders.
    // Selecting a pad (click in PadGrid) populates these. Slices are allowed to
    // overlap — nothing here or in DrumSampler clamps a slice against its neighbors.
    juce::Label selectedSliceLabel { {}, "No slice selected" };
    juce::Label sliceStartFieldLabel { {}, "Start (samples)" }, sliceEndFieldLabel { {}, "End (samples)" },
                slicePitchFieldLabel { {}, "Pitch (semitones)" };
    juce::TextEditor sliceStartEditor, sliceEndEditor, slicePitchEditor;
    int selectedSlice = -1;

    std::unique_ptr<juce::FileChooser> fileChooser;
    SettingsPanel settingsPanel;

    void updateSelectedStepControls();
    void updateSelectedSliceControls();
    void refreshPatternSelector();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SomeChopsAudioProcessorEditor)
};
