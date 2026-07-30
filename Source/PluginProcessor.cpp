#include "PluginProcessor.h"
#include "PluginEditor.h"

SomeChopsAudioProcessor::SomeChopsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
}

void SomeChopsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampler.prepare (sampleRate, samplesPerBlock);
    sequencer.prepare (sampleRate);
}

bool SomeChopsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void SomeChopsAudioProcessor::loadSampleFromFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return;

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);

    sampler.loadSample (std::move (buffer), reader->sampleRate);
    sampler.autoSlice();
}

void SomeChopsAudioProcessor::triggerPadFromUI (int padIndex)
{
    const juce::ScopedLock sl (uiTriggerLock);
    uiTriggerQueue.add (padIndex);
}

void SomeChopsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // --- host tempo/playhead ---
    double bpm = manualBpm; // fallback when the host doesn't report a tempo (e.g. standalone)
    bool isPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())
                bpm = *pos->getBpm();
            isPlaying = pos->getIsPlaying();
        }
    }
    currentBpmForSave = bpm;
    const bool effectivePlaying = (isPlaying && midiSettings.followHostTransport) || manualPlayActive;
    sequencer.setHostInfo (bpm, effectivePlaying);

    // --- MIDI: pads, pattern switching (for live performance), and start/stop ---
    // Checked in priority order (start/stop, then pattern switch, then pad) so
    // overlapping note ranges from misconfigured settings resolve predictably.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (! msg.isNoteOn())
            continue;

        const int note = msg.getNoteNumber();

        if (note == midiSettings.startNote)
        {
            manualPlayActive = true;
            sequencer.resetPosition();
        }
        else if (note == midiSettings.stopNote)
        {
            manualPlayActive = false;
        }
        else if (const int patternNoteCount = juce::jlimit (0, kNumPatterns, midiSettings.patternNoteCount);
                 note >= midiSettings.patternBaseNote && note < midiSettings.patternBaseNote + patternNoteCount)
        {
            const int patternIndex = note - midiSettings.patternBaseNote;
            sequencer.requestPatternChange (patternIndex, ! midiSettings.quantizePatternChanges);
        }
        else
        {
            const int pad = note - midiSettings.padBaseNote;
            if (pad >= 0 && pad < kNumPads)
                sampler.triggerPad (pad, 0.0f, msg.getFloatVelocity());
        }
    }

    // --- UI-triggered pads (mouse clicks on the pad grid) ---
    {
        const juce::ScopedLock sl (uiTriggerLock);
        for (int pad : uiTriggerQueue)
            sampler.triggerPad (pad);
        uiTriggerQueue.clear();
    }

    // --- sequencer clock ---
    auto hits = sequencer.processBlock (buffer.getNumSamples());
    for (auto& hit : hits)
        sampler.triggerPad (hit.padIndex, hit.pitchSemitones, 1.0f, hit.sampleOffsetInBlock);
    // Note: current DrumSampler renders the whole block per-voice rather than honouring
    // hit.sampleOffsetInBlock exactly; for tighter-than-block-accurate ratchets, extend
    // DrumVoice with a "start offset" and have renderNextBlock skip samples before it.

    sampler.renderNextBlock (buffer, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SomeChopsAudioProcessor::createEditor()
{
    return new SomeChopsAudioProcessorEditor (*this);
}

void SomeChopsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xmlString = presetManager.presetToXmlString (sampler, sequencer, sequencer.getCurrentPatternIndex(), currentBpmForSave, midiSettings, uiTheme);
    juce::MemoryOutputStream mos (destData, false);
    mos.writeString (xmlString);
}

void SomeChopsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);
    auto xmlString = mis.readString();

    int patternIndex = 0;
    double bpm = 120.0;
    if (presetManager.loadFromXmlString (xmlString, sampler, sequencer, patternIndex, bpm, midiSettings, uiTheme))
    {
        sequencer.setCurrentPatternIndex (patternIndex);
        currentBpmForSave = bpm;
        manualBpm = bpm;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SomeChopsAudioProcessor();
}
