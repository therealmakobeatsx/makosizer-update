#include "MakosizerXDSP.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace MakosizerX
{
namespace
{
constexpr double twoPi = 6.28318530717958647692;
constexpr float maximumFilterFrequency = 20000.0f;
constexpr std::array<std::uint32_t, 2> randomSeeds {{ 0x13579bdfu, 0x2468ace1u }};

float effectiveMacroValue (float control, float destroy) noexcept
{
    return control + destroy * (1.0f - control);
}

float effectiveDestructiveMacroValue (float control, float destroy) noexcept
{
    return effectiveMacroValue (control, std::sqrt (destroy));
}
}

MakosizerXDSP::MakosizerXDSP()
{
    prepareToPlay (currentSampleRate);
}

void MakosizerXDSP::prepareToPlay (double sampleRate)
{
    currentSampleRate = std::isfinite (sampleRate) ? std::max (1000.0, sampleRate) : 44100.0;
    latencySamples = std::max (1, static_cast<int> (std::round (currentSampleRate * 0.005)));

    const auto maximumModulationSamples = static_cast<std::size_t> (
        std::ceil (currentSampleRate * 0.0040));
    const auto delayLength = static_cast<std::size_t> (latencySamples)
                             + maximumModulationSamples + 4;

    for (auto& line : delayLines)
        line.assign (delayLength, 0.0f);

    reset();
}

void MakosizerXDSP::reset() noexcept
{
    for (auto& line : delayLines)
        std::fill (line.begin(), line.end(), 0.0f);

    writePosition = 0;
    for (std::size_t channel = 0; channel < channelStates.size(); ++channel)
    {
        channelStates[channel] = {};
        channelStates[channel].randomState = randomSeeds[channel];
        channelStates[channel].wowPhase = channel == 0 ? 0.0 : 0.17;
        channelStates[channel].flutterPhase = channel == 0 ? 0.0 : 0.31;
    }
}

float MakosizerXDSP::clamp01 (float value) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0f, 1.0f) : 0.0f;
}

float MakosizerXDSP::readDelay (const std::vector<float>& line,
                                std::size_t position,
                                float delaySamples) noexcept
{
    const auto length = static_cast<float> (line.size());
    auto readPosition = static_cast<float> (position) - delaySamples;
    while (readPosition < 0.0f)
        readPosition += length;

    const auto firstIndex = static_cast<std::size_t> (readPosition);
    const auto secondIndex = (firstIndex + 1) % line.size();
    const auto fraction = readPosition - static_cast<float> (firstIndex);
    return line[firstIndex] + (line[secondIndex] - line[firstIndex]) * fraction;
}

std::uint32_t MakosizerXDSP::nextRandom (std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float MakosizerXDSP::randomUnit (std::uint32_t& state) noexcept
{
    return static_cast<float> (nextRandom (state) >> 8) * (1.0f / 16777216.0f);
}

float MakosizerXDSP::randomBipolar (std::uint32_t& state) noexcept
{
    return randomUnit (state) * 2.0f - 1.0f;
}

void MakosizerXDSP::process (float* const* channels, int numChannels, int numSamples,
                             const DSPParameters& parameters) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0
        || delayLines[0].empty())
        return;

    const auto activeChannels = std::min (numChannels, 2);
    const auto destroy = clamp01 (parameters.destroy);
    const auto drive = effectiveDestructiveMacroValue (clamp01 (parameters.drive), destroy);
    const auto vinyl = effectiveMacroValue (clamp01 (parameters.vinyl), destroy);
    const auto wow = effectiveDestructiveMacroValue (clamp01 (parameters.wow), destroy);
    const auto flutter = effectiveDestructiveMacroValue (clamp01 (parameters.flutter), destroy);
    const auto noise = effectiveMacroValue (clamp01 (parameters.noise), destroy);
    const auto dust = effectiveMacroValue (clamp01 (parameters.dust), destroy);
    const auto crush = effectiveMacroValue (clamp01 (parameters.crush), destroy);
    const auto comp = clamp01 (parameters.comp);
    const auto limiter = clamp01 (parameters.limiter);
    const auto width = std::clamp (std::isfinite (parameters.width) ? parameters.width : 1.0f,
                                   0.0f, 2.0f);
    const auto mix = clamp01 (parameters.mix);
    const auto sampleRate = static_cast<float> (currentSampleRate);

    const auto highPassFrequency = std::clamp (
        std::isfinite (parameters.hpf) ? parameters.hpf : 20.0f,
        20.0f, std::min (maximumFilterFrequency, sampleRate * 0.45f));
    const auto lowPassFrequency = std::clamp (
        std::isfinite (parameters.lpf) ? parameters.lpf : maximumFilterFrequency,
        20.0f, std::min (maximumFilterFrequency, sampleRate * 0.45f));
    const auto highPassCoefficient = std::exp (
        static_cast<float> (-twoPi) * highPassFrequency / sampleRate);
    const auto lowPassCoefficient = 1.0f - std::exp (
        static_cast<float> (-twoPi) * lowPassFrequency / sampleRate);
    const auto vinylCutoff = std::min (maximumFilterFrequency - vinyl * 9000.0f,
                                       sampleRate * 0.45f);
    const auto vinylCoefficient = 1.0f - std::exp (
        static_cast<float> (-twoPi) * vinylCutoff / sampleRate);
    const auto compressorAttack = std::exp (-1.0f / (0.012f * sampleRate));
    const auto compressorRelease = std::exp (-1.0f / (0.120f * sampleRate));
    const auto compressorRatio = 1.0f + comp * 7.0f;
    const auto crushBits = 16.0f - crush * 12.0f;
    const auto crushLevels = std::exp2 (crushBits - 1.0f);
    const auto driveGain = 1.0f + drive * 23.0f;
    const auto driveMakeup = 1.0f / std::sqrt (driveGain);
    const auto baseDelay = static_cast<float> (latencySamples);
    const auto wowDepth = static_cast<float> (currentSampleRate * 0.0028);
    const auto flutterDepth = static_cast<float> (currentSampleRate * 0.0009);
    const auto limiterThreshold = 1.0f - limiter * 0.25f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        std::array<float, 2> dryDelayed {};
        std::array<float, 2> wetDelayed {};

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            const auto channelIndex = static_cast<std::size_t> (channel);
            auto& state = channelStates[channelIndex];
            const auto dryInput = channels[channel][sample];
            auto wet = dryInput;

            state.highPassOutput = highPassCoefficient
                                   * (state.highPassOutput + wet
                                      - state.previousHighPassInput);
            state.previousHighPassInput = wet;
            wet = state.highPassOutput;

            state.lowPassOutput += lowPassCoefficient * (wet - state.lowPassOutput);
            wet = state.lowPassOutput;

            const auto saturated = std::tanh (wet * driveGain) * driveMakeup;
            wet += drive * (saturated - wet);

            state.vinylLowPassOutput += vinylCoefficient
                                        * (wet - state.vinylLowPassOutput);
            wet += vinyl * (state.vinylLowPassOutput - wet);
            const auto vinylSaturation = std::tanh (wet * (1.0f + vinyl * 1.5f));
            wet += vinyl * 0.25f * (vinylSaturation - wet);

            auto& randomState = state.randomState;
            wet += randomBipolar (randomState) * noise * 0.012f;
            if (dust > 0.0f && randomUnit (randomState) < dust * 0.00002f)
            {
                const auto click = 0.04f + randomUnit (randomState) * 0.32f;
                wet += randomBipolar (randomState) * click * dust;
            }

            if (crush > 0.0f)
            {
                const auto bounded = std::clamp (wet, -1.0f, 1.0f);
                const auto quantized = std::round (bounded * crushLevels) / crushLevels;
                wet += crush * (quantized - wet);
            }

            const auto envelopeCoefficient = std::abs (wet) > state.compressorEnvelope
                                                 ? compressorAttack
                                                 : compressorRelease;
            state.compressorEnvelope = envelopeCoefficient * state.compressorEnvelope
                                       + (1.0f - envelopeCoefficient) * std::abs (wet);
            if (comp > 0.0f && state.compressorEnvelope > 0.5f)
            {
                const auto gain = std::pow (
                    0.5f / state.compressorEnvelope, 1.0f - 1.0f / compressorRatio);
                wet *= gain;
            }

            state.wowPhase += twoPi * (0.5 + wow * 1.35) / currentSampleRate;
            state.flutterPhase += twoPi * (7.0 + flutter * 18.0) / currentSampleRate;
            if (state.wowPhase >= twoPi)
                state.wowPhase -= twoPi;
            if (state.flutterPhase >= twoPi)
                state.flutterPhase -= twoPi;

            const auto modulation = static_cast<float> (
                std::sin (state.wowPhase) * wow * wowDepth
                + std::sin (state.flutterPhase) * flutter * flutterDepth);
            const auto modulatedDelay = std::max (1.0f, baseDelay + modulation);

            delayLines[channelIndex][writePosition] = dryInput;
            delayLines[channelIndex + 2][writePosition] = wet;
            dryDelayed[channelIndex] = readDelay (
                delayLines[channelIndex], writePosition, baseDelay);
            wetDelayed[channelIndex] = readDelay (
                delayLines[channelIndex + 2], writePosition, modulatedDelay);
        }

        if (activeChannels == 2)
        {
            const auto mid = (wetDelayed[0] + wetDelayed[1]) * 0.5f;
            const auto side = (wetDelayed[0] - wetDelayed[1]) * 0.5f * width;
            wetDelayed[0] = mid + side;
            wetDelayed[1] = mid - side;
        }

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto output = dryDelayed[static_cast<std::size_t> (channel)]
                          + mix * (wetDelayed[static_cast<std::size_t> (channel)]
                                   - dryDelayed[static_cast<std::size_t> (channel)]);

            if (limiter > 0.0f)
            {
                const auto magnitude = std::abs (output);
                if (magnitude > limiterThreshold)
                {
                    const auto range = std::max (0.0001f, 1.0f - limiterThreshold);
                    const auto limitedMagnitude = limiterThreshold
                                                  + range * std::tanh (
                                                      (magnitude - limiterThreshold) / range);
                    const auto limited = std::copysign (limitedMagnitude, output);
                    output += limiter * (limited - output);
                }
            }

            channels[channel][sample] = output;
        }

        writePosition = (writePosition + 1) % delayLines[0].size();
    }
}
}
