#include "PresetManager.h"

PresetManager::PresetManager()
{
    formatManager.registerBasicFormats();
}

juce::String PresetManager::encodeSampleAsBase64Wav (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryBlock mb;
    {
        // AudioFormatWriter takes ownership of the stream passed to createWriterFor() and
        // deletes it in its own destructor — so this must be heap-allocated, not a stack
        // object. (A previous version passed the address of a stack-allocated
        // MemoryOutputStream here, which crashed — reliably, on preset save/getStateInformation,
        // e.g. whenever the standalone window closes with a sample loaded — because the
        // writer's destructor called `delete` on a stack address.)
        auto* mos = new juce::MemoryOutputStream (mb, false);
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (mos, sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));

        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            writer->flush();
            // Writer must be destroyed before the MemoryOutputStream's data is finalized/read
            // safely. Destroying it also deletes `mos`, since the writer owns it.
            writer.reset();
        }
        else
        {
            // createWriterFor() only takes ownership on success; if it failed, `mos` was
            // never adopted by anything, so we're still responsible for it.
            delete mos;
        }
    }

    return juce::Base64::toBase64 (mb.getData(), mb.getSize());
}

bool PresetManager::decodeBase64WavToBuffer (const juce::String& base64, juce::AudioBuffer<float>& bufferOut, double& sampleRateOut)
{
    juce::MemoryOutputStream mos;
    if (! juce::Base64::convertFromBase64 (mos, base64))
        return false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (mos.getData(), mos.getDataSize(), false)));

    if (reader == nullptr)
        return false;

    bufferOut.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&bufferOut, 0, (int) reader->lengthInSamples, 0, true, true);
    sampleRateOut = reader->sampleRate;
    return true;
}

std::unique_ptr<juce::XmlElement> PresetManager::buildXml (DrumSampler& sampler, Sequencer& sequencer,
                                                             int currentPatternIndex, double bpm,
                                                             const MidiMappingSettings& midi, int uiTheme)
{
    auto root = std::make_unique<juce::XmlElement> ("SomeChopsPreset");
    root->setAttribute ("version", 1);
    root->setAttribute ("bpm", bpm);
    root->setAttribute ("currentPattern", currentPatternIndex);
    root->setAttribute ("chokeMode", sampler.getChokeMode());
    root->setAttribute ("uiTheme", uiTheme);

    // --- MIDI note mapping ---
    auto* midiEl = root->createNewChildElement ("MidiMapping");
    midiEl->setAttribute ("padBaseNote", midi.padBaseNote);
    midiEl->setAttribute ("patternBaseNote", midi.patternBaseNote);
    midiEl->setAttribute ("patternNoteCount", midi.patternNoteCount);
    midiEl->setAttribute ("startNote", midi.startNote);
    midiEl->setAttribute ("stopNote", midi.stopNote);
    midiEl->setAttribute ("quantizePatternChanges", midi.quantizePatternChanges);

    // --- Sample ---
    auto* sampleEl = root->createNewChildElement ("Sample");
    const auto& src = sampler.getSourceBuffer();
    if (src.getNumSamples() > 0)
    {
        sampleEl->setAttribute ("sampleRate", sampler.getSourceSampleRate());
        sampleEl->setAttribute ("data", encodeSampleAsBase64Wav (src, sampler.getSourceSampleRate()));
    }

    // --- Slices ---
    auto* slicesEl = root->createNewChildElement ("Slices");
    for (int i = 0; i < sampler.getNumSlices(); ++i)
    {
        const auto& sl = sampler.getSlice (i);
        auto* sliceEl = slicesEl->createNewChildElement ("Slice");
        sliceEl->setAttribute ("start", sl.startSample);
        sliceEl->setAttribute ("end", sl.endSample);
        sliceEl->setAttribute ("trimmedEnd", sl.trimmedEnd);
        sliceEl->setAttribute ("basePitch", sl.basePitch);
        sliceEl->setAttribute ("name", sl.name);
    }

    // --- Sequencer patterns ---
    auto* patternsEl = root->createNewChildElement ("Patterns");
    auto allPatterns = sequencer.getAllPatternsSnapshot(); // locked copy, not a live reference
    for (int p = 0; p < kNumPatterns; ++p)
    {
        auto* patternEl = patternsEl->createNewChildElement ("Pattern");
        patternEl->setAttribute ("index", p);
        patternEl->setAttribute ("name", allPatterns[(size_t) p].name);

        for (int t = 0; t < kNumPads; ++t)
        {
            auto* trackEl = patternEl->createNewChildElement ("Track");
            trackEl->setAttribute ("pad", t);
            trackEl->setAttribute ("numSteps", allPatterns[(size_t) p].tracks[(size_t) t].numSteps);

            juce::String stepsCsv;
            for (int s = 0; s < kNumSteps; ++s)
            {
                const auto& step = allPatterns[(size_t) p].tracks[(size_t) t].steps[(size_t) s];
                // enabled:ratchet:pitch:probability:nudge packed per step, steps separated by '|'
                stepsCsv << (step.enabled ? 1 : 0) << ":" << step.ratchet << ":"
                         << step.pitchSemitones << ":" << step.probability << ":" << step.nudge;
                if (s != kNumSteps - 1)
                    stepsCsv << "|";
            }
            trackEl->setAttribute ("steps", stepsCsv);
        }
    }

    return root;
}

bool PresetManager::applyXml (const juce::XmlElement& root, DrumSampler& sampler, Sequencer& sequencer,
                               int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut, int& uiThemeOut)
{
    // Accept the current tag name, plus the legacy one from before the project was
    // renamed from DrumChop to SomeChops, so old presets still load.
    if (root.getTagName() != "SomeChopsPreset" && root.getTagName() != "DrumChopPreset")
        return false;

    bpmOut = root.getDoubleAttribute ("bpm", 120.0);
    currentPatternIndexOut = root.getIntAttribute ("currentPattern", 0);
    sampler.setChokeMode (root.getBoolAttribute ("chokeMode", false));
    uiThemeOut = root.getIntAttribute ("uiTheme", uiThemeOut);

    if (auto* midiEl = root.getChildByName ("MidiMapping"))
    {
        midiOut.padBaseNote = midiEl->getIntAttribute ("padBaseNote", midiOut.padBaseNote);
        midiOut.patternBaseNote = midiEl->getIntAttribute ("patternBaseNote", midiOut.patternBaseNote);
        midiOut.patternNoteCount = midiEl->getIntAttribute ("patternNoteCount", midiOut.patternNoteCount);
        midiOut.startNote = midiEl->getIntAttribute ("startNote", midiOut.startNote);
        midiOut.stopNote = midiEl->getIntAttribute ("stopNote", midiOut.stopNote);
        midiOut.quantizePatternChanges = midiEl->getBoolAttribute ("quantizePatternChanges", midiOut.quantizePatternChanges);
    }

    if (auto* sampleEl = root.getChildByName ("Sample"))
    {
        const juce::String data = sampleEl->getStringAttribute ("data");
        if (data.isNotEmpty())
        {
            juce::AudioBuffer<float> buf;
            double sr = 44100.0;
            if (decodeBase64WavToBuffer (data, buf, sr))
                sampler.loadSample (std::move (buf), sr);
        }
    }

    if (auto* slicesEl = root.getChildByName ("Slices"))
    {
        std::vector<Slice> newSlices;
        for (auto* sliceEl : slicesEl->getChildWithTagNameIterator ("Slice"))
        {
            Slice sl;
            sl.startSample = sliceEl->getIntAttribute ("start");
            sl.endSample = sliceEl->getIntAttribute ("end");
            sl.trimmedEnd = sliceEl->getIntAttribute ("trimmedEnd", sl.endSample);
            sl.basePitch = (float) sliceEl->getDoubleAttribute ("basePitch", 0.0);
            sl.name = sliceEl->getStringAttribute ("name");
            newSlices.push_back (sl);
        }
        sampler.setSlices (std::move (newSlices));
    }

    if (auto* patternsEl = root.getChildByName ("Patterns"))
    {
        // Start from the current live patterns (so any pattern index not present in this
        // XML keeps its existing data), mutate a local copy, then commit it all at once
        // via setAllPatterns() — never holding a direct mutable reference into the
        // sequencer's own data while we're still reading/parsing XML.
        auto allPatterns = sequencer.getAllPatternsSnapshot();

        for (auto* patternEl : patternsEl->getChildWithTagNameIterator ("Pattern"))
        {
            const int p = patternEl->getIntAttribute ("index", -1);
            if (p < 0 || p >= kNumPatterns)
                continue;

            allPatterns[(size_t) p].name = patternEl->getStringAttribute ("name", "Pattern");

            for (auto* trackEl : patternEl->getChildWithTagNameIterator ("Track"))
            {
                const int t = trackEl->getIntAttribute ("pad", -1);
                if (t < 0 || t >= kNumPads)
                    continue;

                allPatterns[(size_t) p].tracks[(size_t) t].numSteps =
                    juce::jlimit (1, kNumSteps, trackEl->getIntAttribute ("numSteps", kNumSteps));

                const juce::String stepsCsv = trackEl->getStringAttribute ("steps");
                auto stepTokens = juce::StringArray::fromTokens (stepsCsv, "|", "");

                for (int s = 0; s < juce::jmin (kNumSteps, stepTokens.size()); ++s)
                {
                    auto fields = juce::StringArray::fromTokens (stepTokens[s], ":", "");
                    if (fields.size() < 3)
                        continue;

                    auto& step = allPatterns[(size_t) p].tracks[(size_t) t].steps[(size_t) s];
                    step.enabled = fields[0].getIntValue() != 0;
                    step.ratchet = fields[1].getIntValue();
                    // Current format is enabled:ratchet:pitch:probability:nudge (5 fields).
                    // Earlier versions used enabled:ratchet:pitch:probability (4, no nudge)
                    // or enabled:ratchet:probability (3, no pitch or nudge). Handle all three
                    // so older presets still load sensibly.
                    if (fields.size() >= 5)
                    {
                        step.pitchSemitones = fields[2].getFloatValue();
                        step.probability = fields[3].getFloatValue();
                        step.nudge = fields[4].getFloatValue();
                    }
                    else if (fields.size() == 4)
                    {
                        step.pitchSemitones = fields[2].getFloatValue();
                        step.probability = fields[3].getFloatValue();
                        step.nudge = 0.0f;
                    }
                    else
                    {
                        step.pitchSemitones = 0.0f;
                        step.probability = fields[2].getFloatValue();
                        step.nudge = 0.0f;
                    }
                }
            }
        }

        sequencer.setAllPatterns (std::move (allPatterns));
    }

    return true;
}

juce::String PresetManager::presetToXmlString (DrumSampler& sampler, Sequencer& sequencer,
                                                 int currentPatternIndex, double bpm, const MidiMappingSettings& midi, int uiTheme)
{
    auto xml = buildXml (sampler, sequencer, currentPatternIndex, bpm, midi, uiTheme);
    return xml->toString();
}

bool PresetManager::loadFromXmlString (const juce::String& xmlString, DrumSampler& sampler, Sequencer& sequencer,
                                        int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut, int& uiThemeOut)
{
    auto xml = juce::XmlDocument::parse (xmlString);
    if (xml == nullptr)
        return false;
    return applyXml (*xml, sampler, sequencer, currentPatternIndexOut, bpmOut, midiOut, uiThemeOut);
}

bool PresetManager::savePreset (const juce::File& file, DrumSampler& sampler, Sequencer& sequencer,
                                 int currentPatternIndex, double bpm, const MidiMappingSettings& midi, int uiTheme)
{
    auto xml = buildXml (sampler, sequencer, currentPatternIndex, bpm, midi, uiTheme);
    return xml->writeTo (file);
}

bool PresetManager::loadPreset (const juce::File& file, DrumSampler& sampler, Sequencer& sequencer,
                                 int& currentPatternIndexOut, double& bpmOut, MidiMappingSettings& midiOut, int& uiThemeOut)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;
    return applyXml (*xml, sampler, sequencer, currentPatternIndexOut, bpmOut, midiOut, uiThemeOut);
}
