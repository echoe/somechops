#include "Sequencer.h"

Sequencer::Sequencer()
{
    for (auto& p : patterns)
        p.name = "Pattern";

    trackStep.fill (-1); // so the first boundary crossing advances each track to step 0
}

void Sequencer::prepare (double sr)
{
    sampleRate = sr;
    updateTiming();
}

void Sequencer::setHostInfo (double newBpm, bool isPlaying)
{
    if (newBpm > 0.0)
        bpm = newBpm;
    playing = isPlaying;
    updateTiming();
}

void Sequencer::updateTiming()
{
    // samples per 16th-note-equivalent step, generalised by stepsPerBeat
    const double beatsPerStep = 1.0 / (double) stepsPerBeat;
    const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    samplesPerStep = secondsPerBeat * beatsPerStep * sampleRate;
    if (samplesPerStep < 1.0)
        samplesPerStep = 1.0;
}

void Sequencer::setCurrentPatternIndex (int index)
{
    currentPatternIndex = juce::jlimit (0, kNumPatterns - 1, index);
    pendingPatternIndex = -1;
}

void Sequencer::requestPatternChange (int newIndex, bool immediate)
{
    newIndex = juce::jlimit (0, kNumPatterns - 1, newIndex);

    if (immediate)
    {
        currentPatternIndex = newIndex;
        pendingPatternIndex = -1;
    }
    else
    {
        pendingPatternIndex = newIndex;
    }
}

void Sequencer::resetPosition()
{
    currentStep = -1; // so the next processBlock wraps to step 0
    trackStep.fill (-1);
    stepPhase = 0.0;
    pendingHits.clear();
}

void Sequencer::setTrackNumSteps (int trackIndex, int newNumSteps)
{
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return;

    newNumSteps = juce::jlimit (1, kNumSteps, newNumSteps);
    getCurrentPattern().tracks[(size_t) trackIndex].numSteps = newNumSteps;

    // If the lane just got shorter than where its playhead currently sits, wrap it
    // back into range immediately rather than waiting for the next natural wrap.
    auto& ts = trackStep[(size_t) trackIndex];
    if (ts >= newNumSteps)
        ts = ts % newNumSteps;
}

int Sequencer::getTrackNumSteps (int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return kNumSteps;
    return getCurrentPattern().tracks[(size_t) trackIndex].numSteps;
}

int Sequencer::getCurrentTrackStep (int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return -1;
    return trackStep[(size_t) trackIndex];
}

void Sequencer::clearPattern (int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= kNumPatterns)
        return;

    for (auto& track : patterns[(size_t) patternIndex].tracks)
        for (auto& step : track.steps)
            step = StepData {};
}

void Sequencer::randomizeTrack (int trackIndex, float density, float pitchRangeSemitones, int maxRatchet)
{
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return;

    std::uniform_real_distribution<float> unit (0.0f, 1.0f);
    std::uniform_real_distribution<float> pitchDist (-pitchRangeSemitones, pitchRangeSemitones);
    std::uniform_int_distribution<int> ratchetDist (1, juce::jmax (1, maxRatchet));

    auto& track = getCurrentPattern().tracks[(size_t) trackIndex];
    for (auto& step : track.steps)
    {
        step.enabled = unit (rng) < density;
        step.pitchSemitones = step.enabled ? pitchDist (rng) : 0.0f;
        step.ratchet = step.enabled ? ratchetDist (rng) : 1;
        step.probability = step.enabled ? juce::jmap (unit (rng), 0.0f, 1.0f, 60.0f, 100.0f) : 100.0f;
    }
}

void Sequencer::randomizeAllTracks (float density, float pitchRangeSemitones, int maxRatchet)
{
    for (int t = 0; t < kNumPads; ++t)
        randomizeTrack (t, density, pitchRangeSemitones, maxRatchet);
}

std::vector<SequencerHit> Sequencer::processBlock (int numSamples)
{
    std::vector<SequencerHit> output;
    if (! playing || samplesPerStep <= 0.0)
        return output;

    int samplesProcessed = 0;

    // Entries in pendingHits are already relative to the start of *this* block
    // (they were shifted at the end of the previous call). Emit any that land here.
    for (auto it = pendingHits.begin(); it != pendingHits.end(); )
    {
        if (it->first >= 0.0 && it->first < (double) numSamples)
        {
            SequencerHit hit = it->second;
            hit.sampleOffsetInBlock = (int) juce::jlimit (0, numSamples - 1, (int) std::round (it->first));
            output.push_back (hit);
            it = pendingHits.erase (it);
        }
        else
        {
            ++it;
        }
    }

    while (samplesProcessed < numSamples)
    {
        const double remainingInStep = samplesPerStep - stepPhase;
        const int samplesToBoundary = (int) std::ceil (juce::jmax (0.0, remainingInStep));
        const int chunk = juce::jmin (numSamples - samplesProcessed, juce::jmax (1, samplesToBoundary));

        stepPhase += chunk;
        samplesProcessed += chunk;

        if (stepPhase >= samplesPerStep - 0.0001)
        {
            stepPhase = 0.0;
            // currentStep is the global bar position (0..15) — kept for pattern-switch
            // quantization and as a shared "beat" reference; it does NOT index each
            // track's steps directly anymore, since tracks can loop at different lengths.
            currentStep = (currentStep + 1) % kNumSteps;

            if (currentStep == 0 && pendingPatternIndex >= 0)
            {
                currentPatternIndex = pendingPatternIndex;
                pendingPatternIndex = -1;
            }

            std::uniform_real_distribution<float> unit (0.0f, 1.0f);
            auto& pattern = getCurrentPattern();

            for (int pad = 0; pad < kNumPads; ++pad)
            {
                auto& track = pattern.tracks[(size_t) pad];
                const int trackLen = juce::jlimit (1, kNumSteps, track.numSteps);
                trackStep[(size_t) pad] = (trackStep[(size_t) pad] + 1) % trackLen;

                const auto& step = track.steps[(size_t) trackStep[(size_t) pad]];
                if (! step.enabled)
                    continue;

                if (unit (rng) * 100.0f > step.probability)
                    continue;

                const int ratchet = juce::jlimit (1, 8, step.ratchet);
                const double subDiv = samplesPerStep / (double) ratchet;

                // Nudge shifts this step's whole hit group (including any ratchets) earlier
                // or later, as a percentage of one step's duration. Clamped to not schedule
                // before the current block's start — a large negative nudge on a step that
                // lands right at the top of a block can't reach back into already-rendered
                // audio from a previous block.
                const float nudgePercent = juce::jlimit (-50.0f, 50.0f, step.nudge);
                const double nudgeSamples = (nudgePercent / 100.0) * samplesPerStep;

                for (int k = 0; k < ratchet; ++k)
                {
                    const double hitTimeInBlock = juce::jmax (0.0, (double) samplesProcessed + nudgeSamples + k * subDiv);

                    if (hitTimeInBlock < (double) numSamples)
                    {
                        SequencerHit hit { pad, step.pitchSemitones, (int) juce::jlimit (0, numSamples - 1, (int) std::round (hitTimeInBlock)) };
                        output.push_back (hit);
                    }
                    else
                    {
                        // Falls beyond this block; store relative to *this* block's start.
                        // It will be shifted to be relative to the next block below.
                        pendingHits.emplace_back (hitTimeInBlock, SequencerHit { pad, step.pitchSemitones, 0 });
                    }
                }
            }
        }
    }

    // Shift any still-pending (future) times back by numSamples so they're relative to next call.
    for (auto& p : pendingHits)
        p.first -= (double) numSamples;

    return output;
}
