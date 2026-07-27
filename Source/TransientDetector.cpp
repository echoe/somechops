#include "TransientDetector.h"

juce::AudioBuffer<float> TransientDetector::makeMonoDownmix (const juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> mono (1, buffer.getNumSamples());
    mono.clear();

    const int numCh = buffer.getNumChannels();
    if (numCh == 0)
        return mono;

    for (int ch = 0; ch < numCh; ++ch)
        mono.addFrom (0, 0, buffer, ch, 0, buffer.getNumSamples(), 1.0f / (float) numCh);

    return mono;
}

std::vector<int> TransientDetector::findOnsets (const juce::AudioBuffer<float>& bufferIn, double sampleRate) const
{
    std::vector<int> onsets;
    if (bufferIn.getNumSamples() <= 0)
        return onsets;

    auto mono = makeMonoDownmix (bufferIn);
    const float* data = mono.getReadPointer (0);
    const int numSamples = mono.getNumSamples();

    const int fftSize = 1 << settings.fftOrder;
    const int hop = settings.hopSize;

    juce::dsp::FFT fft (settings.fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

    const int numFrames = std::max (0, (numSamples - fftSize) / hop + 1);
    if (numFrames < 2)
        return onsets;

    std::vector<float> prevMag ((size_t) (fftSize / 2), 0.0f);
    std::vector<float> flux ((size_t) numFrames, 0.0f);

    std::vector<float> fftBuffer ((size_t) (fftSize * 2), 0.0f);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const int start = frame * hop;

        std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftBuffer[(size_t) i] = data[start + i];

        window.multiplyWithWindowingTable (fftBuffer.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

        float frameFlux = 0.0f;
        for (int bin = 0; bin < fftSize / 2; ++bin)
        {
            const float mag = fftBuffer[(size_t) bin];
            const float diff = mag - prevMag[(size_t) bin];
            if (diff > 0.0f)
                frameFlux += diff;
            prevMag[(size_t) bin] = mag;
        }

        flux[(size_t) frame] = frameFlux;
    }

    // Adaptive threshold: local mean + sensitivity * local std, over a sliding window of frames.
    const int localWindow = 8; // frames either side
    std::vector<float> threshold ((size_t) numFrames, 0.0f);

    for (int i = 0; i < numFrames; ++i)
    {
        const int lo = std::max (0, i - localWindow);
        const int hi = std::min (numFrames - 1, i + localWindow);
        const int count = hi - lo + 1;

        float mean = 0.0f;
        for (int k = lo; k <= hi; ++k) mean += flux[(size_t) k];
        mean /= (float) count;

        float variance = 0.0f;
        for (int k = lo; k <= hi; ++k)
        {
            const float d = flux[(size_t) k] - mean;
            variance += d * d;
        }
        variance /= (float) count;
        const float stddev = std::sqrt (variance);

        threshold[(size_t) i] = mean + settings.sensitivity * stddev;
    }

    const int minGapFrames = std::max (1, (int) ((settings.minGapMs / 1000.0) * sampleRate / hop));
    int lastOnsetFrame = -minGapFrames * 2;

    for (int i = 1; i < numFrames - 1; ++i)
    {
        const bool isPeak = flux[(size_t) i] > threshold[(size_t) i]
                          && flux[(size_t) i] >= flux[(size_t) (i - 1)]
                          && flux[(size_t) i] >= flux[(size_t) (i + 1)];

        if (isPeak && (i - lastOnsetFrame) >= minGapFrames)
        {
            onsets.push_back (i * hop);
            lastOnsetFrame = i;
        }
    }

    // Always include sample 0 as the first slice boundary if not already close to one.
    if (onsets.empty() || onsets.front() > hop)
        onsets.insert (onsets.begin(), 0);

    return onsets;
}
