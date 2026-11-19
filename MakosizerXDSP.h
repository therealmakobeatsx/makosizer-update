#pragma once

#include <array>
#include <cstdint>
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
};

class MakosizerXDSP final
{
public:
    MakosizerXDSP();

    void prepareToPlay (double sampleRate);
    void reset() noexcept;
    void process (float* const* channels, int numChannels, int numSamples,
                  const DSPParameters& parameters) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    struct ChannelState
    {
        float previousHighPassInput = 0.0f;
        float highPassOutput = 0.0f;
        float lowPassOutput = 0.0f;
        float vinylLowPassOutput = 0.0f;
        float compressorEnvelope = 0.0f;
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

    std::array<ChannelState, 2> channelStates;
    std::array<std::vector<float>, 4> delayLines;
    double currentSampleRate = 44100.0;
    std::size_t writePosition = 0;
    int latencySamples = 220;
};
}
