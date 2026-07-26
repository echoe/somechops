#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumSampler.h"
#include "Sequencer.h"

// Serializes/deserializes everything needed to fully restore plugin state:
// the source sample (embedded as base64 WAV), slice points + trims, and
// the entire 32-pattern sequencer bank.
class PresetManager
{
public:
    PresetManager();

    static constexpr const char* fileExtension = ".dchp";

    bool savePreset (const juce::File& file, DrumSampler& sampler, Sequencer& sequencer,
                      int currentPatternIndex, double bpm, const MidiMappingSettings& midi);

    // Returns true on success. On success, sampler/sequencer are updated in place.
    bool loadPreset (const juce::File& file, DrumSampler& sampler, Sequencer& sequencer,
                      int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut);

    // Same as above but operating on an in-memory XML string (useful for the
    // host's getStateInformation/setStateInformation, which also need to save this).
    juce::String presetToXmlString (DrumSampler& sampler, Sequencer& sequencer,
                                     int currentPatternIndex, double bpm, const MidiMappingSettings& midi);
    bool loadFromXmlString (const juce::String& xmlString, DrumSampler& sampler, Sequencer& sequencer,
                             int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut);

private:
    juce::AudioFormatManager formatManager;

    std::unique_ptr<juce::XmlElement> buildXml (DrumSampler& sampler, Sequencer& sequencer,
                                                 int currentPatternIndex, double bpm, const MidiMappingSettings& midi);
    bool applyXml (const juce::XmlElement& root, DrumSampler& sampler, Sequencer& sequencer,
                   int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut);

    static juce::String encodeSampleAsBase64Wav (const juce::AudioBuffer<float>& buffer, double sampleRate);
    static bool decodeBase64WavToBuffer (const juce::String& base64, juce::AudioBuffer<float>& bufferOut, double& sampleRateOut);
};
