#include "DrumSampler.h"
#include "TransientDetector.h"

DrumSampler::DrumSampler() {}

void DrumSampler::prepare (double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
}

void DrumSampler::loadSample (juce::AudioBuffer<float> newSample, double sourceSR)
{
    // newSample has already been fully read from disk by the caller; this just installs
    // it. The move-assignment and clear() below are cheap, so holding the lock for them
    // won't meaningfully block the audio thread.
    const juce::ScopedLock sl (lock);
    sourceBuffer = std::move (newSample);
    sourceSampleRate = sourceSR;
    slices.clear();
    hasChromaticBackup = false; // any pending chromatic undo no longer applies to the new sample
    stopAllVoices(); // safe to call while already holding `lock` — CriticalSection is re-entrant
}

void DrumSampler::autoSlice (float sensitivity)
{
    if (sourceBuffer.getNumSamples() <= 0)
        return;

    TransientDetector::Settings s;
    s.sensitivity = sensitivity;
    TransientDetector detector (s);

    // Reading sourceBuffer here without holding `lock` is safe: the only thing that
    // ever replaces it is loadSample(), which — like this method — only ever runs on
    // the message thread, so the two can't overlap with each other. The audio thread
    // never writes to sourceBuffer, only reads it, so two concurrent reads are fine.
    auto onsets = detector.findOnsets (sourceBuffer, sourceSampleRate);
    if (onsets.empty())
        return;

    if ((int) onsets.size() > kMaxSlices)
        onsets.resize ((size_t) kMaxSlices);

    std::vector<Slice> newSlices;
    for (size_t i = 0; i < onsets.size(); ++i)
    {
        Slice sl;
        sl.startSample = onsets[i];
        sl.endSample = (i + 1 < onsets.size()) ? onsets[i + 1] : sourceBuffer.getNumSamples();
        sl.trimmedEnd = sl.endSample;
        sl.name = "Slice " + juce::String ((int) i + 1);
        newSlices.push_back (sl);
    }

    const juce::ScopedLock sl (lock);
    slices = std::move (newSlices);
    hasChromaticBackup = false; // any pending chromatic undo no longer applies to the new slicing
}

void DrumSampler::setSliceBounds (int sliceIndex, int startSample, int endSample)
{
    const juce::ScopedLock sl (lock);

    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    auto& slc = slices[(size_t) sliceIndex];
    slc.startSample = juce::jlimit (0, sourceBuffer.getNumSamples() - 1, startSample);
    slc.endSample = juce::jlimit (slc.startSample + 1, sourceBuffer.getNumSamples(), endSample);
    slc.trimmedEnd = juce::jmin (slc.trimmedEnd, slc.endSample);
    if (slc.trimmedEnd <= slc.startSample)
        slc.trimmedEnd = slc.endSample;
}

void DrumSampler::setSliceTrimmedLength (int sliceIndex, int newTrimmedEnd)
{
    const juce::ScopedLock sl (lock);

    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    auto& slc = slices[(size_t) sliceIndex];

    // Trimming is allowed out to the full length of the loaded sample, not capped at
    // this slice's endSample — which, for every slice but the last, is simply wherever
    // the *next* auto-detected onset happened to land. Slices are explicitly allowed to
    // overlap (nothing here clamps a slice against its neighbors), so stretching one out
    // past a neighbor's start is legitimate and shouldn't be silently capped. endSample
    // is pulled forward to match whenever trimmedEnd grows past it, since setSliceBounds
    // relies on endSample always being >= trimmedEnd.
    const int upperBound = juce::jmax (slc.startSample + 1, sourceBuffer.getNumSamples());
    slc.trimmedEnd = juce::jlimit (slc.startSample + 1, upperBound, newTrimmedEnd);
    slc.endSample = juce::jmax (slc.endSample, slc.trimmedEnd);
}

void DrumSampler::setSlicePitch (int sliceIndex, float semitones)
{
    const juce::ScopedLock sl (lock);

    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    slices[(size_t) sliceIndex].basePitch = juce::jlimit (-24.0f, 24.0f, semitones);
}

void DrumSampler::setSliceChokeGroup (int sliceIndex, int chokeGroup)
{
    const juce::ScopedLock sl (lock);

    if (sliceIndex < 0 || sliceIndex >= (int) slices.size())
        return;

    slices[(size_t) sliceIndex].chokeGroup = juce::jmax (0, chokeGroup);
}

bool DrumSampler::makeChromaticFrom (int sourceSliceIndex)
{
    const juce::ScopedLock sl (lock);

    if (sourceSliceIndex < 0 || sourceSliceIndex >= (int) slices.size())
        return false;

    slicesBeforeChromatic = slices; // snapshot for undo, taken before anything below changes
    hasChromaticBackup = true;

    const Slice source = slices[(size_t) sourceSliceIndex]; // copy: about to overwrite the whole vector

    // Always ends up with exactly kNumPads slices — one per pad — regardless of how
    // many slices existed before (auto-slice may have found more or fewer transients
    // than there are pads).
    slices.assign ((size_t) kNumPads, source);

    for (int i = 0; i < kNumPads; ++i)
        slices[(size_t) i].basePitch = juce::jlimit (-24.0f, 24.0f, source.basePitch + (float) (i - sourceSliceIndex));

    return true;
}

bool DrumSampler::undoChromatic()
{
    const juce::ScopedLock sl (lock);

    if (! hasChromaticBackup)
        return false;

    slices = std::move (slicesBeforeChromatic);
    hasChromaticBackup = false;
    return true;
}

void DrumSampler::setSlices (std::vector<Slice> newSlices)
{
    const juce::ScopedLock sl (lock);
    slices = std::move (newSlices);
    hasChromaticBackup = false; // any pending chromatic undo no longer applies (e.g. a preset just loaded)
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
    const juce::ScopedLock sl (lock);

    if (padIndex < 0 || padIndex >= (int) slices.size())
        return;

    const int group = slices[(size_t) padIndex].chokeGroup;
    if (group != 0)
        chokeGroupVoices (group);

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
    v.chokeFadeSamplesRemaining = -1; // fresh voice, not being choked
}

void DrumSampler::chokeGroupVoices (int group)
{
    for (auto& v : voices)
    {
        if (! v.active || v.chokeFadeSamplesRemaining >= 0)
            continue; // inactive, or already fading out from a previous choke

        if (v.sliceIndex < 0 || v.sliceIndex >= (int) slices.size())
        {
            v.active = false;
            continue;
        }

        if (slices[(size_t) v.sliceIndex].chokeGroup == group)
            v.chokeFadeSamplesRemaining = kFadeSamples;
    }
}

void DrumSampler::stopAllVoices()
{
    const juce::ScopedLock sl (lock);
    for (auto& v : voices)
        v.active = false;
}

void DrumSampler::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const juce::ScopedLock sl (lock);

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

        const auto& curSlice = slices[(size_t) v.sliceIndex];
        const int sliceLen = curSlice.getPlaybackLength();
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

            const double srcPosD = curSlice.startSample + v.position;
            const int srcPos0 = (int) srcPosD;
            const int srcPos1 = juce::jmin (srcPos0 + 1, sourceBuffer.getNumSamples() - 1);
            const float frac = (float) (srcPosD - (double) srcPos0);

            // click-free fade near the trimmed end
            float env = 1.0f;
            const double samplesLeft = (double) sliceLen - v.position;
            if (samplesLeft < kFadeSamples)
                env = (float) (samplesLeft / (double) kFadeSamples);

            // Being choked out by a newer hit in the same choke group: fade from
            // wherever this voice currently is (not from its slice's tail), then stop.
            if (v.chokeFadeSamplesRemaining >= 0)
            {
                env *= (float) v.chokeFadeSamplesRemaining / (float) kFadeSamples;

                if (v.chokeFadeSamplesRemaining == 0)
                {
                    v.active = false;
                    break;
                }
                --v.chokeFadeSamplesRemaining;
            }

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
