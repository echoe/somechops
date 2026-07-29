#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "DrumSampler.h"
#include "Sequencer.h"
#include "PresetManager.h"

class SomeChopsAudioProcessor : public juce::AudioProcessor
{
public:
    SomeChopsAudioProcessor();
    ~SomeChopsAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Overriding only the float version hides AudioProcessor's double overload from name
    // lookup, which -Woverloaded-virtual warns about; this brings it back into scope.
    // We don't actually support double-precision processing (no AudioProcessor::
    // supportsDoublePrecisionProcessing() override), so it's never called in practice.
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SomeChops"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Access for the editor ---
    DrumSampler& getSampler() { return sampler; }
    Sequencer& getSequencer() { return sequencer; }
    PresetManager& getPresetManager() { return presetManager; }

    void loadSampleFromFile (const juce::File& file);
    void triggerPadFromUI (int padIndex);

    // The sequencer runs whenever the host transport is playing OR the manual
    // play/stop button in the editor has been pressed (needed for standalone /
    // when the host doesn't report a play state, or for auditioning without
    // starting the whole DAW transport). Atomic: written by both the UI thread
    // (Play/Stop button) and the audio thread (MIDI start/stop notes).
    std::atomic<bool> manualPlayActive { false };

    // Atomic: written on the audio thread every block, read on the UI thread
    // (Save Preset button, and indirectly via getStateInformation).
    std::atomic<double> currentBpmForSave { 120.0 };

    // Used when the host doesn't report a tempo (e.g. standalone), or as the tempo the
    // editor's BPM slider controls. Host tempo, when available, still takes priority.
    // Atomic: written on the UI thread (BPM slider, preset load), read every block on
    // the audio thread.
    std::atomic<double> manualBpm { 120.0 };

    // Configurable MIDI note assignments for pads, pattern switching, and start/stop —
    // editable from the settings page.
    MidiMappingSettings midiSettings;

    // Selected UI theme (see UiTheme in SomeChopsLookAndFeel.h), stored as a plain int
    // here so this header doesn't need to pull in GUI headers. Only ever touched on the
    // message thread (settings page, editor's LookAndFeel), so no atomic needed. 0 =
    // UiTheme::Minimal, the default.
    int uiTheme = 0;

private:
    DrumSampler sampler;
    Sequencer sequencer;
    PresetManager presetManager;

    juce::AudioFormatManager formatManager;

    // pads triggered directly from the UI thread this block, drained on the audio thread
    juce::Array<int> uiTriggerQueue;
    juce::CriticalSection uiTriggerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SomeChopsAudioProcessor)
};
