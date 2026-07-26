#include "DrumSampler.h"
#include "TransientDetector.h"

DrumSampler::DrumSampler() {}

void DrumSampler::prepare (double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
}

void DrumSampler::loadSample (juce::AudioBuffer<float> newSample, double sourceSR)
{
    sourceBuffer = std::move (newSample);
    sourceSampleRate = sourceSR;
    slices.clear();
    stopAllVoices();
}

void DrumSampler::autoSlice (float sensitivity)
{
    if (sourceBuffer.getNumSamples() <= 0)
        return;

    TransientDetector::Settings s;
    s.sensitivity = sensitivity;
    TransientDetector detector (s);

    auto onsets = detector.findOnsets (sourceBuffer, sourceSampleRate);
    if (onsets.empty())
        return;

    if ((int) onsets.size() > kMaxSlices)
        onsets.resize ((size_t) kMaxSlices);

    slices.clear();
    for (size_t i = 0; i < onsets.size(); ++i)
    {
        Slice sl;
        sl.startSample = onsets[i];
        sl.endSample = (i + 1 < onsets.size()) ? onsets[i + 1] : sourceBuffer.getNumSamples();
        sl.trimmedEnd = sl.endSample;
        sl.name = "Slice " + juce::String ((int) i + 1);
        slices.push_back (sl);
    }
}

void DrumSampler::setSliceBounds (int sliceIndex, int startSample, int endSample)
{
    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    auto& sl = slices[(size_t) sliceIndex];
    sl.startSample = juce::jlimit (0, sourceBuffer.getNumSamples() - 1, startSample);
    sl.endSample = juce::jlimit (sl.startSample + 1, sourceBuffer.getNumSamples(), endSample);
    sl.trimmedEnd = juce::jmin (sl.trimmedEnd, sl.endSample);
    if (sl.trimmedEnd <= sl.startSample)
        sl.trimmedEnd = sl.endSample;
}

void DrumSampler::setSliceTrimmedLength (int sliceIndex, int newTrimmedEnd)
{
    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    auto& sl = slices[(size_t) sliceIndex];
    sl.trimmedEnd = juce::jlimit (sl.startSample + 1, sl.endSample, newTrimmedEnd);
}

void DrumSampler::setSlicePitch (int sliceIndex, float semitones)
{
    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    slices[(size_t) sliceIndex].basePitch = juce::jlimit (-24.0f, 24.0f, semitones);
}

int DrumSampler::findFreeVoice()
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (! voices[(size_t) i].active)
            return i;

    // Steal the voice with the least progress remaining (oldest-ish heuristic: furthest along).
    int steal = 0;
    double best = -1.0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[(size_t) i].position > best)
        {
            best = voices[(size_t) i].position;
            steal = i;
        }
    }
    return steal;
}

void DrumSampler::triggerPad (int padIndex, float pitchSemitones, float gain, int delaySamples)
{
    if (padIndex < 0 || padIndex >= (int) slices.size())
        return;

    if (chokeMode)
        chokeAllVoices();

    const int voiceIdx = findFreeVoice();
    auto& v = voices[(size_t) voiceIdx];
    v.active = true;
    v.sliceIndex = padIndex;
    v.position = 0.0;
    const float totalPitch = slices[(size_t) padIndex].basePitch + pitchSemitones;
    v.pitchRatio = std::pow (2.0, totalPitch / 12.0);
    v.gain = gain;
    v.envelope = 1.0f;
    v.padIndex = padIndex;
    v.delaySamples = juce::jmax (0, delaySamples);
}

void DrumSampler::chokeAllVoices()
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        if (v.sliceIndex < 0 || v.sliceIndex >= (int) slices.size())
        {
            v.active = false;
            continue;
        }

        // Jump each active voice to no more than kFadeSamples from the end of its
        // playback length, so the existing end-of-slice fade (in renderNextBlock)
        // takes it out cleanly instead of clicking.
        const int sliceLen = slices[(size_t) v.sliceIndex].getPlaybackLength();
        v.position = juce::jmax (v.position, (double) juce::jmax (0, sliceLen - kFadeSamples));
    }
}

void DrumSampler::stopAllVoices()
{
    for (auto& v : voices)
        v.active = false;
}

void DrumSampler::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const int numOutCh = output.getNumChannels();
    const int srcCh = sourceBuffer.getNumChannels();
    if (srcCh == 0)
        return;

    // Source may be at a different sample rate than the host; combine that ratio with pitch ratio.
    const double srRatio = sourceSampleRate / currentSampleRate;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        if (v.sliceIndex < 0 || v.sliceIndex >= (int) slices.size())
        {
            v.active = false;
            continue;
        }

        const auto& sl = slices[(size_t) v.sliceIndex];
        const int sliceLen = sl.getPlaybackLength();
        const double readRatio = v.pitchRatio * srRatio;

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.delaySamples > 0)
            {
                --v.delaySamples;
                continue; // hasn't started yet within this block
            }

            if (v.position >= (double) sliceLen)
            {
                v.active = false;
                break;
            }

            const double srcPosD = sl.startSample + v.position;
            const int srcPos0 = (int) srcPosD;
            const int srcPos1 = juce::jmin (srcPos0 + 1, sourceBuffer.getNumSamples() - 1);
            const float frac = (float) (srcPosD - (double) srcPos0);

            // click-free fade near the trimmed end
            float env = 1.0f;
            const double samplesLeft = (double) sliceLen - v.position;
            if (samplesLeft < kFadeSamples)
                env = (float) (samplesLeft / (double) kFadeSamples);

            for (int ch = 0; ch < numOutCh; ++ch)
            {
                const int srcChIdx = juce::jmin (ch, srcCh - 1);
                const float s0 = sourceBuffer.getSample (srcChIdx, juce::jmin (srcPos0, sourceBuffer.getNumSamples() - 1));
                const float s1 = sourceBuffer.getSample (srcChIdx, srcPos1);
                const float sample = s0 + frac * (s1 - s0);

                output.addSample (ch, startSample + i, sample * v.gain * env);
            }

            v.position += readRatio;
        }
    }
}
