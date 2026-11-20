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

float rootMeanSquare (const std::vector<float>& samples)
{
    double sumSquares = 0.0;
    for (const auto sample : samples)
        sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
    return samples.empty()
               ? 0.0f
               : static_cast<float> (std::sqrt (sumSquares / samples.size()));
}

float differenceRootMeanSquare (const std::vector<float>& first,
                                const std::vector<float>& second)
{
    if (first.size() != second.size() || first.empty())
        return 0.0f;

    double sumSquares = 0.0;
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        const auto difference = static_cast<double> (first[i] - second[i]);
        sumSquares += difference * difference;
    }
    return static_cast<float> (std::sqrt (sumSquares / first.size()));
}

float toneAmplitude (const std::vector<float>& samples, float frequency,
                     float sampleRate)
{
    double inPhase = 0.0;
    double quadrature = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const auto phase = 2.0 * 3.14159265358979323846 * frequency
                           * static_cast<double> (i) / sampleRate;
        inPhase += samples[i] * std::cos (phase);
        quadrature += samples[i] * std::sin (phase);
    }

    return samples.empty()
               ? 0.0f
               : static_cast<float> (2.0 * std::hypot (inPhase, quadrature)
                                     / samples.size());
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

bool limiterIncreasesWithControl()
{
    std::array<float, 5> peaks {};
    std::array<float, 5> gainReductions {};
    constexpr std::array<float, 5> controls {{ 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }};

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (44100.0, 257);
        MakosizerX::DSPParameters parameters;
        parameters.limiter = controls[i];
        parameters.mix = 1.0f;
        std::vector<float> samples (8192);
        for (std::size_t sample = 0; sample < samples.size(); ++sample)
            samples[sample] = 1.5f * std::sin (static_cast<float> (sample) * 0.071f);
        float* channels[] { samples.data() };
        dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);
        peaks[i] = peakMagnitude (samples);
        gainReductions[i] = dsp.getLimiterGainReductionDb();
    }

    for (std::size_t i = 1; i < controls.size(); ++i)
        if (peaks[i - 1] < peaks[i]
            || gainReductions[i - 1] > gainReductions[i])
            return false;

    return peaks.back() <= 0.26f
           && gainReductions.back() > gainReductions.front() + 3.0f
           && peaks.front() - peaks.back() > 0.25f;
}

bool vinylSimulationDiffersFromSimpleNoise()
{
    std::array<std::vector<float>, 2> renders {
        std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f)
    };
    for (std::size_t mode = 0; mode < renders.size(); ++mode)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (44100.0, 257);
        MakosizerX::DSPParameters parameters;
        parameters.vinyl = mode == 0 ? 1.0f : 0.0f;
        parameters.noise = mode == 0 ? 0.0f : 1.0f;
        float* channels[] { renders[mode].data() };
        dsp.process (channels, 1, static_cast<int> (renders[mode].size()), parameters);
    }

    const auto vinylLevel = rootMeanSquare (renders[0]);
    const auto noiseLevel = rootMeanSquare (renders[1]);
    const auto vinylRumble = toneAmplitude (renders[0], 31.0f, 44100.0f);
    const auto noiseAtRumble = toneAmplitude (renders[1], 31.0f, 44100.0f);
    return allSamplesFinite (renders[0]) && allSamplesFinite (renders[1])
           && vinylLevel > 0.001f && noiseLevel > 0.001f
           && differenceRootMeanSquare (renders[0], renders[1]) > 0.002f
           && vinylRumble > noiseAtRumble + 0.0005f;
}

bool compressorControlIsClearlyAudible()
{
    std::array<float, 4> peaks {};
    std::array<float, 4> rms {};
    std::array<float, 4> controls {{ 0.0f, 0.1f, 0.5f, 1.0f }};

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (44100.0, 257);
        MakosizerX::DSPParameters parameters;
        parameters.comp = controls[i];
        std::vector<float> samples (8192);
        for (std::size_t sample = 0; sample < samples.size(); ++sample)
            samples[sample] = 0.9f * std::sin (static_cast<float> (sample) * 0.071f);
        float* channels[] { samples.data() };
        dsp.process (channels, 1, static_cast<int> (samples.size()), parameters);
        peaks[i] = peakMagnitude (samples);
        double sumSquares = 0.0;
        for (const auto sample : samples)
            sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
        rms[i] = static_cast<float> (std::sqrt (sumSquares / samples.size()));
    }

    const auto fullRangeDifference = std::abs (rms[0] - rms[3]);
    const auto smallRangeDifference = std::abs (rms[1] - rms[0]);
    const auto metricsFinite = std::all_of (peaks.begin(), peaks.end(),
                                            [] (float value) { return std::isfinite (value); })
        && std::all_of (rms.begin(), rms.end(),
                        [] (float value) { return std::isfinite (value); });
    return metricsFinite
           && fullRangeDifference > rms[0] * 0.08f
           && smallRangeDifference > rms[0] * 0.01f
           && std::abs (peaks[0] - peaks[3]) > 0.05f;
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

bool oversamplingModesProduceDifferentAudio()
{
    constexpr std::size_t sampleCount = 8192;
    std::array<std::vector<float>, 4> renders {
        std::vector<float> (sampleCount), std::vector<float> (sampleCount),
        std::vector<float> (sampleCount), std::vector<float> (sampleCount)
    };

    for (std::size_t mode = 0; mode < renders.size(); ++mode)
    {
        MakosizerX::MakosizerXDSP dsp;
        dsp.prepareToPlay (44100.0, 257);
        MakosizerX::DSPParameters parameters;
        parameters.drive = 0.9f;
        parameters.destroy = 0.7f;
        parameters.crush = 0.8f;
        parameters.comp = 0.7f;
        parameters.limiter = 0.65f;
        parameters.oversampling = static_cast<float> (mode);
        for (std::size_t i = 0; i < sampleCount; ++i)
            renders[mode][i] = 0.85f * std::sin (
                static_cast<float> (i) * 2.0f * 3.14159265358979323846f
                * 15000.0f / 44100.0f);

        float* channels[] { renders[mode].data() };
        dsp.process (channels, 1, static_cast<int> (sampleCount), parameters);
        if (! allSamplesFinite (renders[mode]))
            return false;
    }

    return differenceRootMeanSquare (renders[0], renders[1]) > 0.005f
           && differenceRootMeanSquare (renders[1], renders[2]) > 0.001f
           && differenceRootMeanSquare (renders[2], renders[3]) > 0.0005f;
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
        {
            std::cerr << "Dry impulse latency mismatch in " << mode << "x: "
                      << peakPosition (dry) << " vs " << reportedLatency << '\n';
            return false;
        }

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
        {
            std::cerr << "Wet impulse latency mismatch in " << mode << "x: "
                      << wetPeakPosition << " vs " << reportedLatency << '\n';
            return false;
        }
    }

    return true;
}

bool bypassSignalMatchesReportedLatency()
{
    MakosizerX::MakosizerXDSP dsp;
    dsp.prepareToPlay (48000.0, 257);
    const auto reportedLatency = static_cast<std::size_t> (dsp.getLatencySamples());
    if (reportedLatency == 0)
        return false;

    std::vector<float> samples (reportedLatency + 16, 0.0f);
    samples[0] = 1.0f;
    float* channels[] { samples.data() };
    dsp.processBypassed (channels, 1, static_cast<int> (samples.size()));

    return peakPosition (samples) == reportedLatency
           && std::abs (samples[reportedLatency] - 1.0f) < 1.0e-6f
           && allSamplesFinite (samples);
}
}

int main()
{
    if (! limiterRespectsCeiling())
    {
        std::cerr << "Limiter exceeded its -12 dBFS maximum-setting ceiling.\n";
        return 1;
    }

    if (! limiterOffPreservesOverrangeSignal())
    {
        std::cerr << "Limiter-off path unexpectedly changed the dry signal.\n";
        return 1;
    }

    if (! limiterIncreasesWithControl())
    {
        std::cerr << "Limiter control is not progressive across its full range.\n";
        return 1;
    }

    if (! vinylSimulationDiffersFromSimpleNoise())
    {
        std::cerr << "Vinyl simulation is not distinguishable from simple noise.\n";
        return 1;
    }

    if (! compressorControlIsClearlyAudible())
    {
        std::cerr << "Compressor control is not sufficiently effective across its range.\n";
        return 1;
    }

    if (! allOversamplingModesAreStable())
    {
        std::cerr << "An oversampling mode produced unstable or silent output.\n";
        return 1;
    }

    if (! oversamplingModesProduceDifferentAudio())
    {
        std::cerr << "Oversampling modes do not produce sufficiently different audio.\n";
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

    if (! bypassSignalMatchesReportedLatency())
    {
        std::cerr << "Bypassed audio does not preserve the reported plugin latency.\n";
        return 1;
    }

    return 0;
}
