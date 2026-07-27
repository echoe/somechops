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
    int endSample   = 0;     // exclusive, into the original sample buffer
    int trimmedEnd  = 0;     // <= endSample; this is the *adjustable length* the user can shorten
    float basePitch = 0.0f;  // -24..+24 semitones; permanently retunes this sample's playback speed
    juce::String name;

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

    // Poly (default): overlapping hits all ring out together.
    // Choke: triggering any new hit quickly fades out every other currently-sounding
    // voice first, so only one note sounds at a time across the whole kit.
    void setChokeMode (bool shouldChoke) { chokeMode = shouldChoke; }
    bool getChokeMode() const { return chokeMode; }

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
    bool chokeMode = false;

    // Guards sourceBuffer/slices/voices against the message thread (sample loading,
    // slicing, slider edits) and the audio thread (triggerPad/renderNextBlock)
    // touching them at the same time. A prior version of this class had no such
    // guard, which could crash — e.g. loadSample() clearing `slices` on the message
    // thread while the audio thread was mid-render and still indexing into it.
    juce::CriticalSection lock;

    static constexpr int kFadeSamples = 64; // click-free fade out at slice end

    int findFreeVoice();

    // Fades out all currently active voices quickly (over kFadeSamples) rather than
    // hard-cutting them, so choke mode doesn't click. Only ever called from within
    // triggerPad(), which already holds `lock` (CriticalSection is re-entrant on the
    // same thread, so this doesn't need its own lock).
    void chokeAllVoices();
};
