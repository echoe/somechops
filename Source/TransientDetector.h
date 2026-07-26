#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Free-standing (not nested) so its default member initializers can be used
// as a default constructor argument elsewhere without hitting GCC's
// nested-class NSDMI restriction.
struct TransientDetectorSettings
{
    int fftOrder = 10;          // 1024-point FFT
    int hopSize = 256;          // 75% overlap at fftSize 1024
    float sensitivity = 1.5f;   // multiplier on adaptive threshold (lower = more onsets)
    int minGapMs = 30;          // minimum time between onsets
};

// Finds transient (onset) positions in an audio buffer using a spectral-flux
// novelty function with adaptive peak picking. Returns sample positions
// (relative to the start of the buffer) at which slices should begin.
class TransientDetector
{
public:
    using Settings = TransientDetectorSettings;

    explicit TransientDetector (Settings settingsIn = Settings()) : settings (settingsIn) {}

    // buffer must be at the given sampleRate. Works on a mono downmix internally.
    std::vector<int> findOnsets (const juce::AudioBuffer<float>& buffer, double sampleRate) const;

private:
    Settings settings;

    static juce::AudioBuffer<float> makeMonoDownmix (const juce::AudioBuffer<float>& buffer);
};
