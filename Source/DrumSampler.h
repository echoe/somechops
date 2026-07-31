#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

static constexpr int kMaxSlices = 32;   // slices detected from the sample
static constexpr int kNumPads   = 12;   // pads playable in the drumkit / sequencer — one per note in an octave
static constexpr int kMaxVoices = 24;   // polyphony for ratchets/overlaps

// One detected/adjustable slice of the source sample.
struct Slice
{
    int startSample = 0;
    int endSample   = 0;     // exclusive, into the original sample buffer; always >= trimmedEnd.
                             // Set by auto-slice to the next detected onset (or sample end, for
                             // the last slice), but not a hard ceiling — setSliceTrimmedLength()
                             // pulls this forward if the user trims past it.
    int trimmedEnd  = 0;     // <= endSample; this is the *adjustable length* the user can shorten
                             // or lengthen (out to the full sample), independent of neighbors.
    float basePitch = 0.0f;  // -24..+24 semitones; permanently retunes this sample's playback speed
    juce::String name;

    // 0 = no choke group (this slice never chokes anything, and is never choked).
    // Any nonzero value groups slices together: triggering one slice immediately
    // fades out any other currently-sounding slice sharing the same group (classic
    // hi-hat open/closed behaviour, but generalized to any group of slices/pads).
    int chokeGroup = 0;

    int getPlaybackLength() const { return juce::jmax (1, trimmedEnd - startSample); }
};

// A single playing voice (one hit of one slice, possibly a ratchet repeat).
struct DrumVoice
{
    bool active = false;
    int sliceIndex = -1;
    double position = 0.0;       // read position in samples, within the slice
    double pitchRatio = 1.0;
    float gain = 1.0f;
    float envelope = 1.0f;       // simple linear fade-out envelope near slice end to avoid clicks
    int padIndex = -1;
    int delaySamples = 0;        // samples to wait, within the current block, before this voice starts

    // >= 0 while this voice is being choked out by a newly-triggered slice in the same
    // choke group; counts down from kFadeSamples to 0. -1 means "not choking" (either
    // playing normally, or already silent). Fades the voice out from wherever it
    // currently is, rather than jumping its read position to the tail of the sample —
    // jumping there would play whatever audio happens to live at the end of the slice
    // for a few samples, which is audible as a random, unrelated blip.
    int chokeFadeSamplesRemaining = -1;
};

// Owns the loaded source sample, its slices, and renders the drum voices.
class DrumSampler
{
public:
    DrumSampler();

    void prepare (double sampleRate, int maxBlockSize);

    // Replaces the current source sample (mono or stereo) and its sample rate.
    void loadSample (juce::AudioBuffer<float> newSample, double sourceSampleRate);

    // Runs transient detection on the currently loaded sample and rebuilds slices/pad mapping.
    void autoSlice (float sensitivity = 1.5f);

    // Manual slice editing (used by the waveform UI).
    void setSliceBounds (int sliceIndex, int startSample, int endSample);
    void setSliceTrimmedLength (int sliceIndex, int newTrimmedEnd);
    void setSlicePitch (int sliceIndex, float semitones);
    void setSliceChokeGroup (int sliceIndex, int chokeGroup);

    // "Chromatic" tool: copies the given slice's sample region (start/end/trim), choke
    // group, and name onto every pad, then re-tunes each pad's basePitch so the whole
    // set of pads plays chromatically across one octave — pad i ends up at the source
    // slice's basePitch + (i - sourceSliceIndex) semitones, so the pad you picked keeps
    // its original pitch and the rest fan out chromatically around it. Always leaves
    // exactly kNumPads slices afterwards, even if there were more or fewer before (e.g.
    // from auto-slice finding a different number of transients than there are pads).
    // Returns false (and does nothing) if sourceSliceIndex doesn't currently have a
    // valid slice to copy from.
    bool makeChromaticFrom (int sourceSliceIndex);

    // Undoes the most recent makeChromaticFrom() call, restoring every slice exactly as
    // it was immediately beforehand. Only one level of undo is kept — a second
    // makeChromaticFrom() call overwrites the previous snapshot, and loading a new
    // sample/preset or re-running auto-slice invalidates it entirely (there's nothing
    // sensible to revert to any more). Returns false (and does nothing) if there's no
    // chromatic change currently available to undo.
    bool undoChromatic();
    bool canUndoChromatic() const { return hasChromaticBackup; }

    int getNumSlices() const { return (int) slices.size(); }
    const Slice& getSlice (int index) const { return slices[(size_t) index]; }
    std::vector<Slice>& getSlices() { return slices; }

    // Triggers pad playback (from sequencer, MIDI, or UI). The slice's own basePitch is
    // always applied; pitchSemitones is an optional additional offset on top of that.
    // delaySamples lets a caller (the sequencer) schedule a hit to start partway through
    // the current block, e.g. for ratchets or per-step timing nudge, instead of always
    // starting at the top of the block.
    void triggerPad (int padIndex, float pitchSemitones = 0.0f, float gain = 1.0f, int delaySamples = 0);
    void stopAllVoices();

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples);

    const juce::AudioBuffer<float>& getSourceBuffer() const { return sourceBuffer; }
    double getSourceSampleRate() const { return sourceSampleRate; }

    // For preset save/load: replace slices wholesale (e.g. loaded from XML).
    void setSlices (std::vector<Slice> newSlices);

private:
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSampleRate = 44100.0;
    double currentSampleRate = 44100.0;

    std::vector<Slice> slices;
    std::array<DrumVoice, kMaxVoices> voices;

    // One-level undo snapshot for makeChromaticFrom(). Only meaningful while
    // hasChromaticBackup is true; invalidated (along with the flag) by anything that
    // replaces `slices` wholesale (loadSample, autoSlice, a fresh setSlices from a
    // preset load) since there'd be nothing coherent left to revert to.
    std::vector<Slice> slicesBeforeChromatic;
    bool hasChromaticBackup = false;

    // Guards sourceBuffer/slices/voices against the message thread (sample loading,
    // slicing, slider edits) and the audio thread (triggerPad/renderNextBlock)
    // touching them at the same time. A prior version of this class had no such
    // guard, which could crash — e.g. loadSample() clearing `slices` on the message
    // thread while the audio thread was mid-render and still indexing into it.
    juce::CriticalSection lock;

    static constexpr int kFadeSamples = 64; // click-free fade out at slice end

    int findFreeVoice();

    // Starts a fade-out (over kFadeSamples, handled in renderNextBlock) on every
    // currently-active voice whose slice belongs to the given choke group. Only ever
    // called from within triggerPad(), which already holds `lock` (CriticalSection is
    // re-entrant on the same thread, so this doesn't need its own lock). `group` is
    // assumed nonzero — group 0 means "no choke group" and never chokes anything.
    void chokeGroupVoices (int group);
};
