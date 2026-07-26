#pragma once
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
    // starting the whole DAW transport).
    bool manualPlayActive = false;
    double currentBpmForSave = 120.0;

    // Used when the host doesn't report a tempo (e.g. standalone), or as the tempo the
    // editor's BPM slider controls. Host tempo, when available, still takes priority.
    double manualBpm = 120.0;

    // Configurable MIDI note assignments for pads, pattern switching, and start/stop —
    // editable from the settings page.
    MidiMappingSettings midiSettings;

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
