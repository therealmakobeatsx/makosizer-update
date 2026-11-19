#include "MakosizerXDSP.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace
{
bool allSamplesFinite (const std::vector<float>& samples)
{
    return std::all_of (samples.begin(), samples.end(),
                        [] (float sample) { return std::isfinite (sample); });
}

float peakMagnitude (const std::vector<float>& samples)
{
    const auto peak = std::max_element (samples.begin(), samples.end(),
                                        [] (float a, float b)
                                        {
                                            return std::abs (a) < std::abs (b);
                                        });
    return peak == samples.end() ? 0.0f : std::abs (*peak);
}

std::size_t peakPosition (const std::vector<float>& samples)
{
    const auto peak = std::max_element (samples.begin(), samples.end(),
                                        [] (float a, float b)
                                        {
                                            return std::abs (a) < std::abs (b);
                                        });
    return static_cast<std::size_t> (std::distance (samples.begin(), peak));
}

bool limiterRespectsCeiling()
{
    MakosizerX::MakosizerXDSP dsp;
    dsp.prepareToPlay (44100.0, 257);

    MakosizerX::DSPParameters parameters;
    parameters.limiter = 1.0f;
    std::vector<float> samples (8192);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = 1.5f * std::sin (static_cast<float> (i) * 0.071f);

    float* channels[] { samples.data() };
    dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);

    const auto ceiling = std::pow (10.0f, (-12.0f) / 20.0f);
    const auto peak = peakMagnitude (samples);
    return peak <= ceiling + 1.0e-5f && peak > 0.20f;
}

bool limiterOffPreservesOverrangeSignal()
{
    MakosizerX::MakosizerXDSP dsp;
    dsp.prepareToPlay (44100.0, 128);

    MakosizerX::DSPParameters parameters;
    parameters.mix = 0.0f;
    std::vector<float> samples (1024, 1.25f);
    float* channels[] { samples.data() };
    dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);

    return peakMagnitude (samples) > 1.0f;
}

bool allOversamplingModesAreStable()
{
    constexpr int sampleRate = 44100;
    constexpr std::array<int, 4> factors {{ 1, 2, 4, 8 }};

    for (std::size_t mode = 0; mode < factors.size(); ++mode)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (sampleRate, 257);

        MakosizerX::DSPParameters parameters;
        parameters.drive = 0.72f;
        parameters.destroy = 0.36f;
        parameters.vinyl = 0.61f;
        parameters.crush = 0.68f;
        parameters.comp = 0.24f;
        parameters.limiter = 0.4f;
        parameters.oversampling = static_cast<float> (mode);

        std::vector<float> samples (4096);
        for (std::size_t i = 0; i < samples.size(); ++i)
            samples[i] = 0.7f * std::sin (static_cast<float> (i) * 0.19f);

        float* channels[] { samples.data() };
        dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);
        if (! allSamplesFinite (samples) || peakMagnitude (samples) < 0.01f
            || peakMagnitude (samples) > 2.0f)
            return false;

        const auto internalRate = sampleRate * factors[mode];
        std::cout << sampleRate / 1000.0 << " kHz @ " << factors[mode] << "x = "
                  << internalRate / 1000.0 << " kHz, latency "
                  << dsp.getLatencySamples() << " samples\n";
    }

    return true;
}

bool switchingModesRemainsStable()
{
    MakosizerX::MakosizerXDSP dsp;
    dsp.prepareToPlay (48000.0, 127);

    constexpr std::array<int, 4> modes {{ 0, 1, 2, 3 }};
    std::vector<float> samples (127);
    for (int pass = 0; pass < 4; ++pass)
    {
        for (const auto mode : modes)
        {
            MakosizerX::DSPParameters parameters;
            parameters.drive = 0.63f;
            parameters.destroy = 0.41f;
            parameters.vinyl = 0.5f;
            parameters.crush = 0.7f;
            parameters.oversampling = static_cast<float> (mode);

            for (std::size_t i = 0; i < samples.size(); ++i)
                samples[i] = 0.8f * std::sin (
                    static_cast<float> (i + static_cast<std::size_t> (pass) * 127u)
                    * 0.13f);

            float* channels[] { samples.data() };
            dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);
            if (! allSamplesFinite (samples) || peakMagnitude (samples) > 3.0f)
                return false;
        }
    }

    return true;
}

bool stereoModesRemainStable()
{
    MakosizerX::MakosizerXDSP dsp;
    dsp.prepareToPlay (48000.0, 193);

    std::vector<float> left (769);
    std::vector<float> right (769);
    constexpr std::array<int, 4> modes {{ 0, 1, 2, 3 }};
    for (const auto mode : modes)
    {
        MakosizerX::DSPParameters parameters;
        parameters.drive = 0.7f;
        parameters.destroy = 0.3f;
        parameters.vinyl = 0.4f;
        parameters.crush = 0.6f;
        parameters.width = 1.4f;
        parameters.oversampling = static_cast<float> (mode);

        for (std::size_t i = 0; i < left.size(); ++i)
        {
            left[i] = 0.8f * std::sin (static_cast<float> (i) * 0.17f);
            right[i] = 0.6f * std::sin (static_cast<float> (i) * 0.11f);
        }

        float* channels[] { left.data(), right.data() };
        dsp.process (channels, 2, static_cast<int> (left.size()), parameters);
        if (! allSamplesFinite (left) || ! allSamplesFinite (right)
            || peakMagnitude (left) > 3.0f || peakMagnitude (right) > 3.0f)
            return false;
    }

    return true;
}

bool latencyAndDryWetAlignmentAreStable()
{
    constexpr int sampleRate = 44100;
    constexpr int blockSize = 257;
    constexpr std::size_t signalLength = 4096;

    for (int mode = 0; mode < 4; ++mode)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (sampleRate, blockSize);
        const auto reportedLatency = dsp.getLatencySamples();
        if (reportedLatency <= 0)
            return false;

        MakosizerX::DSPParameters parameters;
        parameters.oversampling = static_cast<float> (mode);
        parameters.hpf = 20.0f;
        parameters.lpf = 20000.0f;
        parameters.mix = 0.0f;

        std::vector<float> dry (signalLength, 0.0f);
        dry[0] = 1.0f;
        float* dryChannels[] { dry.data() };
        dsp.process (dryChannels, 1, static_cast<int> (dry.size()), parameters);
        if (peakPosition (dry) != static_cast<std::size_t> (reportedLatency))
            return false;

        dsp.reset();
        parameters.mix = 1.0f;
        std::vector<float> wet (signalLength, 0.0f);
        wet[0] = 1.0f;
        float* wetChannels[] { wet.data() };
        dsp.process (wetChannels, 1, static_cast<int> (wet.size()), parameters);
        if (! allSamplesFinite (wet))
            return false;

        const auto wetPeakPosition = static_cast<int> (peakPosition (wet));
        if (std::abs (wetPeakPosition - reportedLatency) > 1)
            return false;
    }

    return true;
}
}

int main()
{
    if (! limiterRespectsCeiling())
    {
        std::cerr << "Limiter exceeded its -0.1 dBFS ceiling or failed to reach it.\n";
        return 1;
    }

    if (! limiterOffPreservesOverrangeSignal())
    {
        std::cerr << "Limiter-off path unexpectedly changed the dry signal.\n";
        return 1;
    }

    if (! allOversamplingModesAreStable())
    {
        std::cerr << "An oversampling mode produced unstable or silent output.\n";
        return 1;
    }

    if (! switchingModesRemainsStable())
    {
        std::cerr << "Switching oversampling modes produced unstable output.\n";
        return 1;
    }

    if (! stereoModesRemainStable())
    {
        std::cerr << "A stereo oversampling mode produced unstable output.\n";
        return 1;
    }

    if (! latencyAndDryWetAlignmentAreStable())
    {
        std::cerr << "Oversampling latency compensation or dry/wet alignment failed.\n";
        return 1;
    }

    return 0;
}
