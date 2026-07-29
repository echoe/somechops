#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <random>
#include "DrumSampler.h"

static constexpr int kNumSteps = 16;
static constexpr int kNumPatterns = 32;

// Configurable MIDI note assignments: which notes trigger pads, which notes switch
// patterns (for live performance), and which notes start/stop the sequencer. Defaults
// avoid overlap: pads at C2+, pattern switching for one octave from C3 (C3-A#3, 11
// patterns reachable directly), and Start/Stop right above that octave at B3/C4.
struct MidiMappingSettings
{
    int padBaseNote = 36;           // C2: pads occupy padBaseNote .. padBaseNote+kNumPads-1
    int patternBaseNote = 48;       // C3: pattern switching occupies patternBaseNote .. +patternNoteCount-1
    int patternNoteCount = 11;      // how many notes (patterns) are reachable via MIDI, up to kNumPatterns
    int startNote = 59;             // B3
    int stopNote = 60;              // C4
    bool quantizePatternChanges = false; // shared with the "Wait for pattern end" toggle in the UI
};

struct StepData
{
    bool enabled = false;
    int ratchet = 1;              // 1-4 repeats within the step
    float pitchSemitones = 0.0f;  // -24 .. +24, additional offset on top of the slice's own base pitch
    float probability = 100.0f;   // 0-100, chance the step actually fires when enabled
    float nudge = 0.0f;           // -50..+50, percent of a step's duration; shifts this step's hits earlier/later
};

struct Track
{
    std::array<StepData, kNumSteps> steps;
    int numSteps = kNumSteps; // 1..kNumSteps; lets this lane loop shorter than the others (polymeter)
};

struct Pattern
{
    std::array<Track, kNumPads> tracks;
    juce::String name = "Pattern";
};

// A single note event produced by the clock, to be handed to the DrumSampler.
struct SequencerHit
{
    int padIndex;
    float pitchSemitones;
    int sampleOffsetInBlock; // where within the current audio block this hit should fire
};

class Sequencer
{
public:
    Sequencer();

    void prepare (double sampleRate);
    void setHostInfo (double bpm, bool isPlaying);

    // Advances the clock across a block and returns the hits that should fire within it.
    // Called on the audio thread.
    std::vector<SequencerHit> processBlock (int numSamples);

    void setCurrentPatternIndex (int index);
    int getCurrentPatternIndex() const;

    // Requests a switch to a different pattern. If immediate, takes effect on the very
    // next processBlock call. If not, the switch is deferred until the current pattern
    // finishes its 16 steps (i.e. the next time the step counter wraps to 0). Called from
    // both the UI (pattern selector) and the audio thread (MIDI-triggered pattern switch).
    void requestPatternChange (int newIndex, bool immediate);
    int getPendingPatternIndex() const;

    // When enabled, every lane restarts at its own step 1 the moment a pattern change
    // takes effect (immediate or deferred), instead of each lane continuing on from
    // wherever its own step counter happened to be. Handy for patterns/lanes of
    // different lengths, where otherwise a lane mid-loop would stay out of phase
    // with the freshly-switched-to pattern instead of starting clean together.
    void setResetPatternOnChange (bool shouldReset);
    bool getResetPatternOnChange() const;

    // Resets the step/ratchet clock to the start of the pattern. Handy when a manual
    // play button (rather than host transport) starts the sequencer, for a clean count-in.
    void resetPosition();

    void randomizeTrack (int trackIndex, float density, float pitchRangeSemitones, int maxRatchet,
                         float nudgeRangePercent, bool randomizeLength);
    void randomizeAllTracks (float density, float pitchRangeSemitones, int maxRatchet,
                              float nudgeRangePercent, bool randomizeLength);
    void clearPattern (int patternIndex);

    int getCurrentStep() const;

    // Per-lane step length (polymeter): each track can loop over fewer than kNumSteps
    // steps while all tracks stay locked to the same underlying tempo clock, so lanes
    // of different lengths phase against each other rather than all resetting together.
    void setTrackNumSteps (int trackIndex, int newNumSteps);
    int getTrackNumSteps (int trackIndex) const;

    // Which step within its own (possibly shorter) loop a given track is currently on.
    int getCurrentTrackStep (int trackIndex) const;

    void setStepsPerBeat (int steps); // e.g. 4 = 16th notes at 4/4

    // --- Per-step data access, for the current pattern ---
    // These replace direct reference access to Pattern/Track/StepData (which the UI used
    // to mutate directly while the audio thread read the same data with no synchronization
    // at all — the root cause of a real crash in DrumSampler that used the same pattern
    // before it was fixed there). Every call here copies out or applies under the lock.
    StepData getStep (int pad, int step) const;
    void toggleStepEnabled (int pad, int step);
    void setStepRatchet (int pad, int step, int ratchet);
    void setStepPitch (int pad, int step, float pitch);
    void setStepProbability (int pad, int step, float probability);
    void setStepNudge (int pad, int step, float nudge);

    // A full copy of the current pattern, for UI painting (e.g. the step grid) that needs
    // to read many steps at once without locking per-cell.
    Pattern getCurrentPatternSnapshot() const;

    // For preset serialization: copies all 32 patterns out, or replaces them wholesale.
    std::array<Pattern, kNumPatterns> getAllPatternsSnapshot() const;
    void setAllPatterns (std::array<Pattern, kNumPatterns> newPatterns);

private:
    std::array<Pattern, kNumPatterns> patterns;
    int currentPatternIndex = 0;
    int pendingPatternIndex = -1; // -1 = no pattern change queued
    bool resetOnPatternChange = false;

    double sampleRate = 44100.0;
    double bpm = 120.0;
    bool playing = false;
    int stepsPerBeat = 4; // 16th-note steps

    double samplesPerStep = 0.0;
    double stepPhase = 0.0; // accumulated samples into current step
    int currentStep = -1; // -1 so the first boundary crossing advances to step 0, not 1

    // Each track's own position within its (possibly shorter) loop. Advances in lockstep
    // with currentStep every tick, but wraps at that track's own numSteps.
    std::array<int, kNumPads> trackStep;

    std::mt19937 rng { std::random_device {}() };

    // Ratchet hits scheduled beyond the current block are queued here, with times
    // stored relative to the start of the block in which processBlock will next emit them.
    std::vector<std::pair<double, SequencerHit>> pendingHits;

    // Guards `patterns` (and the playback-position state) against the audio thread
    // (processBlock) and the message thread (step edits, randomize, pattern switching,
    // preset load/save) touching them at the same time. `mutable` so const read-only
    // methods can still lock. Re-entrant, so methods here can freely call each other.
    mutable juce::CriticalSection lock;

    void updateTiming(); // assumes `lock` is already held by the caller
};
