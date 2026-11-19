#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace MakosizerX
{
struct DSPParameters
{
    float drive = 0.0f;
    float vinyl = 0.0f;
    float destroy = 0.0f;
    float wow = 0.0f;
    float flutter = 0.0f;
    float noise = 0.0f;
    float dust = 0.0f;
    float crush = 0.0f;
    float comp = 0.0f;
    float hpf = 20.0f;
    float lpf = 20000.0f;
    float limiter = 0.0f;
    float width = 1.0f;
    float mix = 1.0f;
    float oversampling = 0.0f;
};

class MakosizerXDSP final
{
public:
    MakosizerXDSP();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock = 512);
    void reset() noexcept;
    void process (float* const* channels, int numChannels, int numSamples,
                  const DSPParameters& parameters) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }
    float getLimiterGainReductionDb() const noexcept
    {
        return limiterGainReductionDb.load (std::memory_order_relaxed);
    }

private:
    struct ChannelState
    {
        float previousHighPassInput = 0.0f;
        float highPassOutput = 0.0f;
        float lowPassOutput = 0.0f;
        float vinylLowPassOutput = 0.0f;
        float vinylNoiseLowPass = 0.0f;
        float compressorEnvelope = 0.0f;
        float limiterEnvelope = 0.0f;
        float dustDropoutRemaining = 0.0f;
        float dustDropoutGain = 1.0f;
        double wowPhase = 0.0;
        double flutterPhase = 0.0;
        std::uint32_t randomState = 1;
    };

    static float clamp01 (float value) noexcept;
    static float readDelay (const std::vector<float>& line, std::size_t writePosition,
                            float delaySamples) noexcept;
    static std::uint32_t nextRandom (std::uint32_t& state) noexcept;
    static float randomUnit (std::uint32_t& state) noexcept;
    static float randomBipolar (std::uint32_t& state) noexcept;
    static void processNonlinearStages (juce::dsp::AudioBlock<float> block,
                                        float drive, float destroy,
                                        float vinyl, float crush) noexcept;

    std::array<ChannelState, 2> channelStates;
    std::array<std::vector<float>, 4> delayLines;
    std::array<std::vector<float>, 2> limiterDelayLines;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplingProcessors;
    std::array<int, 3> oversamplingLatencySamples {};
    juce::AudioBuffer<float> nonlinearBuffer;
    double currentSampleRate = 44100.0;
    std::size_t maximumBlockSize = 512;
    std::size_t writePosition = 0;
    int maximumOversamplingLatencySamples = 0;
    int lastOversamplingIndex = -1;
    std::size_t limiterWritePosition = 0;
    std::atomic<float> limiterGainReductionDb { 0.0f };
    int latencySamples = 220;
};
}
