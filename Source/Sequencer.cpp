#include "Sequencer.h"

Sequencer::Sequencer()
{
    for (auto& p : patterns)
        p.name = "Pattern";

    trackStep.fill (-1); // so the first boundary crossing advances each track to step 0
}

void Sequencer::prepare (double sr)
{
    const juce::ScopedLock sl (lock);
    sampleRate = sr;
    updateTiming();
}

void Sequencer::setHostInfo (double newBpm, bool isPlaying)
{
    const juce::ScopedLock sl (lock);
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

void Sequencer::setStepsPerBeat (int steps)
{
    const juce::ScopedLock sl (lock);
    stepsPerBeat = juce::jmax (1, steps);
    updateTiming();
}

void Sequencer::setCurrentPatternIndex (int index)
{
    const juce::ScopedLock sl (lock);
    currentPatternIndex = juce::jlimit (0, kNumPatterns - 1, index);
    pendingPatternIndex = -1;
}

int Sequencer::getCurrentPatternIndex() const
{
    const juce::ScopedLock sl (lock);
    return currentPatternIndex;
}

void Sequencer::requestPatternChange (int newIndex, bool immediate)
{
    const juce::ScopedLock sl (lock);
    newIndex = juce::jlimit (0, kNumPatterns - 1, newIndex);

    if (immediate)
    {
        currentPatternIndex = newIndex;
        pendingPatternIndex = -1;

        // Reset every lane back to its own step 1: since the next processBlock only
        // advances trackStep on a step-boundary crossing (not right here), setting
        // these to -1 means the very next boundary brings every lane to step 0
        // together, same as at playback start.
        if (resetOnPatternChange)
            trackStep.fill (-1);
    }
    else
    {
        pendingPatternIndex = newIndex;
    }
}

int Sequencer::getPendingPatternIndex() const
{
    const juce::ScopedLock sl (lock);
    return pendingPatternIndex;
}

void Sequencer::setResetPatternOnChange (bool shouldReset)
{
    const juce::ScopedLock sl (lock);
    resetOnPatternChange = shouldReset;
}

bool Sequencer::getResetPatternOnChange() const
{
    const juce::ScopedLock sl (lock);
    return resetOnPatternChange;
}

void Sequencer::resetPosition()
{
    const juce::ScopedLock sl (lock);
    currentStep = -1; // so the next processBlock wraps to step 0
    trackStep.fill (-1);
    stepPhase = 0.0;
    pendingHits.clear();
}

void Sequencer::setTrackNumSteps (int trackIndex, int newNumSteps)
{
    const juce::ScopedLock sl (lock);

    if (trackIndex < 0 || trackIndex >= kNumPads)
        return;

    newNumSteps = juce::jlimit (1, kNumSteps, newNumSteps);
    patterns[(size_t) currentPatternIndex].tracks[(size_t) trackIndex].numSteps = newNumSteps;

    // If the lane just got shorter than where its playhead currently sits, wrap it
    // back into range immediately rather than waiting for the next natural wrap.
    auto& ts = trackStep[(size_t) trackIndex];
    if (ts >= newNumSteps)
        ts = ts % newNumSteps;
}

int Sequencer::getTrackNumSteps (int trackIndex) const
{
    const juce::ScopedLock sl (lock);
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return kNumSteps;
    return patterns[(size_t) currentPatternIndex].tracks[(size_t) trackIndex].numSteps;
}

int Sequencer::getCurrentTrackStep (int trackIndex) const
{
    const juce::ScopedLock sl (lock);
    if (trackIndex < 0 || trackIndex >= kNumPads)
        return -1;
    return trackStep[(size_t) trackIndex];
}

int Sequencer::getCurrentStep() const
{
    const juce::ScopedLock sl (lock);
    return currentStep;
}

void Sequencer::clearPattern (int patternIndex)
{
    const juce::ScopedLock sl (lock);

    if (patternIndex < 0 || patternIndex >= kNumPatterns)
        return;

    for (auto& track : patterns[(size_t) patternIndex].tracks)
        for (auto& step : track.steps)
            step = StepData {};
}

void Sequencer::randomizeTrack (int trackIndex, float density, float pitchRangeSemitones, int maxRatchet,
                                 float nudgeRangePercent, bool randomizeLength)
{
    const juce::ScopedLock sl (lock);

    if (trackIndex < 0 || trackIndex >= kNumPads)
        return;

    std::uniform_real_distribution<float> unit (0.0f, 1.0f);
    std::uniform_real_distribution<float> pitchDist (-pitchRangeSemitones, pitchRangeSemitones);
    std::uniform_real_distribution<float> nudgeDist (-nudgeRangePercent, nudgeRangePercent);
    std::uniform_int_distribution<int> ratchetDist (1, juce::jmax (1, maxRatchet));

    auto& track = patterns[(size_t) currentPatternIndex].tracks[(size_t) trackIndex];
    for (auto& step : track.steps)
    {
        step.enabled = unit (rng) < density;
        step.pitchSemitones = step.enabled ? pitchDist (rng) : 0.0f;
        step.ratchet = step.enabled ? ratchetDist (rng) : 1;
        step.probability = step.enabled ? juce::jmap (unit (rng), 0.0f, 1.0f, 60.0f, 100.0f) : 100.0f;
        step.nudge = step.enabled ? nudgeDist (rng) : 0.0f;
    }

    if (randomizeLength)
    {
        // 4..kNumSteps rather than 1..kNumSteps — avoids absurdly short 1-2 step loops
        // that would rarely be musically useful, while still giving real polymeter variety.
        std::uniform_int_distribution<int> lengthDist (4, kNumSteps);
        const int newLen = lengthDist (rng);
        track.numSteps = newLen;

        auto& ts = trackStep[(size_t) trackIndex];
        if (ts >= newLen)
            ts = ts % newLen;
    }
}

void Sequencer::randomizeAllTracks (float density, float pitchRangeSemitones, int maxRatchet,
                                     float nudgeRangePercent, bool randomizeLength)
{
    // Locking once for the whole batch (rather than once per track inside randomizeTrack)
    // isn't required for correctness — CriticalSection is re-entrant — but avoids 12
    // separate lock/unlock round trips for what's conceptually one user action.
    const juce::ScopedLock sl (lock);
    for (int t = 0; t < kNumPads; ++t)
        randomizeTrack (t, density, pitchRangeSemitones, maxRatchet, nudgeRangePercent, randomizeLength);
}

StepData Sequencer::getStep (int pad, int step) const
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return {};
    return patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step];
}

void Sequencer::toggleStepEnabled (int pad, int step)
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return;
    auto& s = patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step];
    s.enabled = ! s.enabled;
}

void Sequencer::setStepRatchet (int pad, int step, int ratchet)
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return;
    patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step].ratchet = ratchet;
}

void Sequencer::setStepPitch (int pad, int step, float pitch)
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return;
    patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step].pitchSemitones = pitch;
}

void Sequencer::setStepProbability (int pad, int step, float probability)
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return;
    patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step].probability = probability;
}

void Sequencer::setStepNudge (int pad, int step, float nudge)
{
    const juce::ScopedLock sl (lock);
    if (pad < 0 || pad >= kNumPads || step < 0 || step >= kNumSteps)
        return;
    patterns[(size_t) currentPatternIndex].tracks[(size_t) pad].steps[(size_t) step].nudge = nudge;
}

Pattern Sequencer::getCurrentPatternSnapshot() const
{
    const juce::ScopedLock sl (lock);
    return patterns[(size_t) currentPatternIndex];
}

std::array<Pattern, kNumPatterns> Sequencer::getAllPatternsSnapshot() const
{
    const juce::ScopedLock sl (lock);
    return patterns;
}

void Sequencer::setAllPatterns (std::array<Pattern, kNumPatterns> newPatterns)
{
    const juce::ScopedLock sl (lock);
    patterns = std::move (newPatterns);
}

std::vector<SequencerHit> Sequencer::processBlock (int numSamples)
{
    const juce::ScopedLock sl (lock);

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

            // A deferred pattern change should take effect when the *longest* lane in
            // the current (about-to-be-superseded) pattern finishes its own loop, not
            // always after a fixed 16 steps. If every lane in this pattern is shorter
            // than 16 (e.g. an odd-meter idea where nothing uses the full 16 steps), a
            // fixed 16-step wait would add unwanted extra time before switching — so
            // find the longest lane's length and detect when it wraps back to step 0.
            // Ties (multiple lanes sharing the max length) always wrap in the same tick
            // since every lane advances once per tick, so checking any one of them is enough.
            bool longestLaneWrapsThisTick = false;
            {
                const auto& curPatternForWrapCheck = patterns[(size_t) currentPatternIndex];
                int maxLen = 1;
                for (int pad = 0; pad < kNumPads; ++pad)
                    maxLen = juce::jmax (maxLen, juce::jlimit (1, kNumSteps, curPatternForWrapCheck.tracks[(size_t) pad].numSteps));

                for (int pad = 0; pad < kNumPads; ++pad)
                {
                    const int len = juce::jlimit (1, kNumSteps, curPatternForWrapCheck.tracks[(size_t) pad].numSteps);
                    if (len == maxLen && trackStep[(size_t) pad] == maxLen - 1)
                    {
                        longestLaneWrapsThisTick = true;
                        break;
                    }
                }
            }

            // currentStep is the global bar position (0..15) — kept as a shared "beat"
            // reference for the UI. It does NOT index each track's steps directly, and
            // no longer gates pattern switching either, since tracks can loop at
            // different lengths.
            currentStep = (currentStep + 1) % kNumSteps;

            if (longestLaneWrapsThisTick && pendingPatternIndex >= 0)
            {
                currentPatternIndex = pendingPatternIndex;
                pendingPatternIndex = -1;

                // Same reset as the immediate path: since the per-pad loop just below
                // this runs on every tick (including this one), setting trackStep to -1
                // here means it becomes 0 for every lane in that very loop, so all lanes
                // start the freshly-switched-to pattern together at step 1.
                if (resetOnPatternChange)
                    trackStep.fill (-1);
            }

            std::uniform_real_distribution<float> unit (0.0f, 1.0f);
            auto& pattern = patterns[(size_t) currentPatternIndex];

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
