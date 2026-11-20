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
    prepareToPlay (currentSampleRate, static_cast<int> (maximumBlockSize));
}

void MakosizerXDSP::prepareToPlay (double sampleRate,
                                   int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = std::isfinite (sampleRate) ? std::max (1000.0, sampleRate) : 44100.0;
    maximumBlockSize = static_cast<std::size_t> (
        std::max (1, maximumExpectedSamplesPerBlock));
    nonlinearBuffer.setSize (2, static_cast<int> (maximumBlockSize), false, false, true);

    maximumOversamplingLatencySamples = 0;
    for (std::size_t i = 0; i < oversamplingProcessors.size(); ++i)
    {
        const auto factorExponent = static_cast<int> (i) + 1;
        auto processor = std::make_unique<juce::dsp::Oversampling<float>> (
            2, factorExponent,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true, true);
        processor->initProcessing (maximumBlockSize);
        processor->reset();

        oversamplingLatencySamples[i] = static_cast<int> (
            std::ceil (processor->getLatencyInSamples()));
        maximumOversamplingLatencySamples = std::max (
            maximumOversamplingLatencySamples, oversamplingLatencySamples[i]);
        oversamplingProcessors[i] = std::move (processor);
    }

    const auto fixedDelay = std::max (
        1, static_cast<int> (std::round (currentSampleRate * 0.005)));
    latencySamples = 2 * (fixedDelay + maximumOversamplingLatencySamples);

    const auto maximumModulationSamples = static_cast<std::size_t> (
        std::ceil (currentSampleRate * 0.0040));
    const auto delayLength = static_cast<std::size_t> (latencySamples)
                             + maximumModulationSamples + 4;

    for (auto& line : delayLines)
        line.assign (delayLength, 0.0f);

    for (auto& line : bypassDelayLines)
        line.assign (static_cast<std::size_t> (latencySamples) + 1, 0.0f);

    for (auto& line : limiterDelayLines)
        line.assign (static_cast<std::size_t> (fixedDelay * 8 + 1), 0.0f);

    reset();
}

void MakosizerXDSP::reset() noexcept
{
    for (auto& line : delayLines)
        std::fill (line.begin(), line.end(), 0.0f);
    for (auto& line : bypassDelayLines)
        std::fill (line.begin(), line.end(), 0.0f);
    for (auto& line : limiterDelayLines)
        std::fill (line.begin(), line.end(), 0.0f);

    writePosition = 0;
    bypassWritePosition = 0;
    limiterWritePosition = 0;
    limiterGainReductionDb.store (0.0f, std::memory_order_relaxed);
    lastOversamplingIndex = -1;
    for (auto& processor : oversamplingProcessors)
        if (processor != nullptr)
            processor->reset();

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

void MakosizerXDSP::processNonlinearStages (juce::dsp::AudioBlock<float> block,
                                           float drive, float destroy,
                                           float vinyl, float crush, float comp,
                                           float sampleRate) noexcept
{
    const auto driveGain = 1.0f + drive * 23.0f;
    const auto driveMakeup = 1.0f / std::sqrt (driveGain);
    const auto destroyGain = 1.0f + destroy * 7.0f;
    const auto destroyBias = destroy * 0.18f;
    const auto destroyBiasOutput = std::tanh (destroyBias);
    const auto crushBits = 16.0f - crush * 12.0f;
    const auto crushLevels = std::exp2 (crushBits - 1.0f);
    const auto vinylSmoothing = 1.0f - std::exp (-1.0f / (0.015f * sampleRate));

    for (std::size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* samples = block.getChannelPointer (channel);
        auto& state = channelStates[channel];
        for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            state.smoothedNonlinearVinyl += vinylSmoothing
                * (vinyl - state.smoothedNonlinearVinyl);
            auto wet = samples[sample];

            if (drive > 0.0f)
            {
                const auto saturated = std::tanh (wet * driveGain) * driveMakeup;
                wet += drive * (saturated - wet);
            }

            if (destroy > 0.0f)
            {
                const auto saturated = std::tanh (wet * destroyGain + destroyBias)
                                       - destroyBiasOutput;
                wet += destroy * 0.65f * (saturated - wet);
            }

            if (state.smoothedNonlinearVinyl > 0.0f)
            {
                const auto saturated = std::tanh (
                    wet * (1.0f + state.smoothedNonlinearVinyl * 3.8f));
                wet += state.smoothedNonlinearVinyl * 0.48f * (saturated - wet);
            }

            if (crush > 0.0f)
            {
                const auto bounded = std::clamp (wet, -1.0f, 1.0f);
                const auto quantized = std::round (bounded * crushLevels) / crushLevels;
                wet += crush * (quantized - wet);
            }

            if (comp > 0.0f)
            {
                const auto glueDrive = 1.0f + comp * 0.75f;
                wet = std::tanh (wet * glueDrive) / std::tanh (glueDrive);
            }

            samples[sample] = wet;
        }
    }
}

void MakosizerXDSP::processLimiterStages (
    juce::dsp::AudioBlock<float> block, float limiter, int oversamplingFactor,
    float& maximumGainReductionDb) noexcept
{
    const auto internalRate = static_cast<float> (currentSampleRate)
                             * static_cast<float> (oversamplingFactor);
    const auto limiterControlSmoothing = 1.0f - std::exp (-1.0f / (0.005f * internalRate));
    const auto detectorAttack = std::exp (-1.0f / (0.00015f * internalRate));
    const auto detectorRelease = std::exp (-1.0f / (0.080f * internalRate));
    const auto gainAttack = std::exp (-1.0f / (0.00015f * internalRate));
    const auto gainRelease = std::exp (-1.0f / (0.080f * internalRate));
    const auto baseLookAheadSamples = std::max (
        1, static_cast<int> (std::round (currentSampleRate * 0.005)));
    const auto lookAheadSamples = static_cast<std::size_t> (
        baseLookAheadSamples * oversamplingFactor);
    const auto channelCount = std::min<std::size_t> (block.getNumChannels(), 2);

    for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
    {
        float linkedMagnitude = 0.0f;
        for (std::size_t channel = 0; channel < channelCount; ++channel)
            linkedMagnitude = std::max (linkedMagnitude,
                                        std::abs (block.getSample (channel, sample)));

        float sharedControl = 0.0f;
        float linkedEnvelope = 0.0f;
        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            auto& state = channelStates[channel];
            if (! state.limiterControlInitialized)
            {
                state.smoothedLimiter = limiter;
                state.limiterControlInitialized = true;
            }
            else
            {
                state.smoothedLimiter += limiterControlSmoothing
                    * (limiter - state.smoothedLimiter);
            }
            sharedControl = std::max (sharedControl, state.smoothedLimiter);

            const auto detectorCoefficient = linkedMagnitude > state.limiterEnvelope
                                                 ? detectorAttack
                                                 : detectorRelease;
            state.limiterEnvelope = detectorCoefficient * state.limiterEnvelope
                                    + (1.0f - detectorCoefficient) * linkedMagnitude;
            linkedEnvelope = std::max (linkedEnvelope, state.limiterEnvelope);
        }

        const auto limiterCeilingDb = -0.1f - sharedControl * 11.9f;
        const auto limiterCeiling = std::pow (10.0f, limiterCeilingDb / 20.0f);
        const auto targetGain = sharedControl > 0.0001f
                                    ? std::min (1.0f, limiterCeiling
                                                        / std::max (linkedEnvelope, 1.0e-9f))
                                    : 1.0f;

        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            auto& state = channelStates[channel];
            const auto gainCoefficient = targetGain < state.limiterGain
                                             ? gainAttack
                                             : gainRelease;
            state.limiterGain = gainCoefficient * state.limiterGain
                                + (1.0f - gainCoefficient) * targetGain;

            auto& delayLine = limiterDelayLines[channel];
            delayLine[limiterWritePosition] = block.getSample (channel, sample);
            const auto readPosition = (limiterWritePosition + delayLine.size()
                                       - lookAheadSamples) % delayLine.size();
            auto output = delayLine[readPosition] * state.limiterGain;

            if (sharedControl > 0.0001f && std::abs (output) > limiterCeiling)
            {
                const auto correction = limiterCeiling / std::abs (output);
                output *= correction;
                state.limiterGain *= correction;
            }

            maximumGainReductionDb = std::max (
                maximumGainReductionDb,
                -20.0f * std::log10 (std::max (state.limiterGain, 1.0e-9f)));
            block.setSample (channel, sample, output);
        }

        limiterWritePosition = (limiterWritePosition + 1) % limiterDelayLines[0].size();
    }
}

void MakosizerXDSP::processBypassed (float* const* channels, int numChannels,
                                     int numSamples) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0
        || bypassDelayLines[0].empty())
        return;

    const auto activeChannels = std::min (numChannels, 2);
    const auto delaySamples = static_cast<float> (latencySamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto& delayLine = bypassDelayLines[static_cast<std::size_t> (channel)];
            delayLine[bypassWritePosition] = channels[channel][sample];
            channels[channel][sample] = readDelay (
                delayLine, bypassWritePosition, delaySamples);
        }
        bypassWritePosition = (bypassWritePosition + 1) % bypassDelayLines[0].size();
    }
}

void MakosizerXDSP::process (float* const* channels, int numChannels, int numSamples,
                             const DSPParameters& parameters) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0
        || delayLines[0].empty() || bypassDelayLines[0].empty()
        || maximumBlockSize == 0)
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
    const auto oversamplingIndex = std::clamp (
        std::isfinite (parameters.oversampling)
            ? static_cast<int> (std::round (parameters.oversampling)) : 0,
        0, 3);
    const auto sampleRate = static_cast<float> (currentSampleRate);

    if (oversamplingIndex != lastOversamplingIndex)
    {
        if (oversamplingIndex > 0)
            oversamplingProcessors[static_cast<std::size_t> (oversamplingIndex - 1)]->reset();
        lastOversamplingIndex = oversamplingIndex;
    }

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
    const auto vinylNoiseHighPassCoefficient = std::exp (
        static_cast<float> (-twoPi) * 220.0f / sampleRate);
    const auto vinylNoiseLowPassCoefficient = 1.0f - std::exp (
        static_cast<float> (-twoPi) * 7800.0f / sampleRate);
    const auto vinylControlSmoothing = 1.0f - std::exp (-1.0f / (0.015f * sampleRate));
    const auto vinylCrackleDecay = std::exp (-1.0f / (0.008f * sampleRate));
    const auto vinylPopDecay = std::exp (-1.0f / (0.018f * sampleRate));
    const auto compressorAttack = std::exp (-1.0f / (0.006f * sampleRate));
    const auto compressorRelease = std::exp (-1.0f / (0.145f * sampleRate));
    const auto compressorRatio = 3.0f + comp * 13.0f;
    const auto compressorThreshold = 0.34f - comp * 0.28f;
    const auto compressorMakeup = 1.0f + comp * 0.55f;
    const auto fixedDelay = static_cast<float> (
        std::max (1, static_cast<int> (std::round (currentSampleRate * 0.005))));
    const auto selectedOversamplingLatency = oversamplingIndex == 0
        ? 0
        : oversamplingLatencySamples[static_cast<std::size_t> (oversamplingIndex - 1)];
    const auto dryDelay = static_cast<float> (
        latencySamples - selectedOversamplingLatency
        - std::max (1, static_cast<int> (std::round (currentSampleRate * 0.005))));
    const auto wetBaseDelay = fixedDelay
                              + 2.0f * static_cast<float> (
                                  maximumOversamplingLatencySamples
                                  - selectedOversamplingLatency);
    const auto wowDepth = static_cast<float> (currentSampleRate * 0.0028);
    const auto flutterDepth = static_cast<float> (currentSampleRate * 0.0009);
    auto maximumLimiterGainReductionDb = 0.0f;

    auto blockOffset = 0;
    while (blockOffset < numSamples)
    {
        const auto blockSize = std::min (
            numSamples - blockOffset, static_cast<int> (maximumBlockSize));

        for (int sample = 0; sample < blockSize; ++sample)
        {
            for (int channel = 0; channel < activeChannels; ++channel)
            {
                const auto channelIndex = static_cast<std::size_t> (channel);
                auto& state = channelStates[channelIndex];
                const auto input = channels[channel][blockOffset + sample];
                bypassDelayLines[channelIndex][bypassWritePosition] = input;
                auto wet = input;
                state.smoothedVinyl += vinylControlSmoothing
                    * (vinyl - state.smoothedVinyl);
                const auto vinylAmount = state.smoothedVinyl;

                state.highPassOutput = highPassCoefficient
                                       * (state.highPassOutput + wet
                                          - state.previousHighPassInput);
                state.previousHighPassInput = wet;
                wet = state.highPassOutput;

                state.lowPassOutput += lowPassCoefficient * (wet - state.lowPassOutput);
                wet = state.lowPassOutput;

                state.vinylLowPassOutput += vinylCoefficient
                                            * (wet - state.vinylLowPassOutput);
                wet += vinylAmount * (state.vinylLowPassOutput - wet);

                auto& randomState = state.randomState;
                const auto surfaceNoise = randomBipolar (randomState);
                state.vinylNoiseHighPass = vinylNoiseHighPassCoefficient
                    * (state.vinylNoiseHighPass + surfaceNoise
                       - state.vinylNoisePrevious);
                state.vinylNoisePrevious = surfaceNoise;
                state.vinylNoiseLowPass += vinylNoiseLowPassCoefficient
                    * (state.vinylNoiseHighPass - state.vinylNoiseLowPass);
                wet += vinylAmount * state.vinylNoiseLowPass * 0.008f;

                const auto crackleAmount = std::clamp (
                    (vinylAmount - 0.25f) * 4.0f, 0.0f, 1.0f);
                const auto popAmount = std::clamp (
                    (vinylAmount - 0.50f) * 4.0f, 0.0f, 1.0f);
                const auto rumbleAmount = std::clamp (
                    (vinylAmount - 0.75f) * 4.0f, 0.0f, 1.0f);
                state.vinylCrackle *= vinylCrackleDecay;
                if (crackleAmount > 0.0f
                    && randomUnit (randomState) < crackleAmount * 0.00006f)
                    state.vinylCrackle += randomBipolar (randomState)
                                           * (0.025f + crackleAmount * 0.025f);
                wet += state.vinylCrackle;

                state.vinylPop *= vinylPopDecay;
                if (popAmount > 0.0f
                    && randomUnit (randomState) < popAmount * 0.000012f)
                    state.vinylPop += randomBipolar (randomState)
                                       * (0.035f + randomUnit (randomState) * 0.075f);
                wet += state.vinylPop;

                state.rumblePhase += twoPi * (31.0 + 17.0 * channel)
                                    / currentSampleRate;
                if (state.rumblePhase >= twoPi)
                    state.rumblePhase -= twoPi;
                if (rumbleAmount > 0.0f)
                    wet += rumbleAmount * static_cast<float> (std::sin (state.rumblePhase))
                           * 0.0015f;

                wet += randomBipolar (randomState) * noise * 0.012f;
                if (dust > 0.0f && randomUnit (randomState) < dust * 0.00003f)
                {
                    const auto click = 0.04f + randomUnit (randomState) * 0.32f;
                    wet += randomBipolar (randomState) * click * dust;
                    state.dustDropoutRemaining = sampleRate
                                                 * (0.008f + randomUnit (randomState) * 0.045f)
                                                 * dust;
                    state.dustDropoutGain = randomUnit (randomState) < 0.5f ? 0.0f : 0.12f;
                }
                if (state.dustDropoutRemaining > 0.0f)
                {
                    wet *= 1.0f - dust + dust * state.dustDropoutGain;
                    state.dustDropoutRemaining -= 1.0f;
                }

                nonlinearBuffer.setSample (channel, sample, wet);
            }
            bypassWritePosition = (bypassWritePosition + 1) % bypassDelayLines[0].size();
        }

        if (activeChannels == 1)
            for (int sample = 0; sample < blockSize; ++sample)
                nonlinearBuffer.setSample (
                    1, sample, nonlinearBuffer.getSample (0, sample));

        juce::dsp::AudioBlock<float> baseRateBlock (
            nonlinearBuffer.getArrayOfWritePointers(),
            2,
            static_cast<std::size_t> (blockSize));
        if (oversamplingIndex == 0)
        {
            processNonlinearStages (baseRateBlock, drive, destroy, vinyl, crush,
                                    comp, sampleRate);
        }
        else
        {
            const auto oversamplingFactor = 1 << oversamplingIndex;
            auto& oversampling = *oversamplingProcessors[
                static_cast<std::size_t> (oversamplingIndex - 1)];
            auto oversampledBlock = oversampling.processSamplesUp (baseRateBlock);
            processNonlinearStages (oversampledBlock, drive, destroy, vinyl, crush,
                                    comp, sampleRate * static_cast<float> (oversamplingFactor));
            oversampling.processSamplesDown (baseRateBlock);
        }

        for (int sample = 0; sample < blockSize; ++sample)
        {
            std::array<float, 2> dryDelayed {};
            std::array<float, 2> wetDelayed {};
            std::array<float, 2> wet {};
            const auto inputSample = blockOffset + sample;
            const auto linkedDetectorLevel = activeChannels > 1
                ? std::max (std::abs (channels[0][inputSample]),
                            std::abs (channels[1][inputSample]))
                : std::abs (channels[0][inputSample]);

            for (int channel = 0; channel < activeChannels; ++channel)
            {
                const auto channelIndex = static_cast<std::size_t> (channel);
                auto& state = channelStates[channelIndex];
                auto processed = nonlinearBuffer.getSample (channel, sample);

                const auto envelopeCoefficient = linkedDetectorLevel
                                                        > state.compressorEnvelope
                                                    ? compressorAttack
                                                    : compressorRelease;
                state.compressorEnvelope = envelopeCoefficient * state.compressorEnvelope
                                           + (1.0f - envelopeCoefficient)
                                                 * linkedDetectorLevel;
                if (comp > 0.0f)
                {
                    processed *= compressorMakeup;
                    if (state.compressorEnvelope > compressorThreshold)
                    {
                        const auto kneeWidth = compressorThreshold * 0.18f;
                        const auto over = state.compressorEnvelope - (compressorThreshold - kneeWidth);
                        const auto knee = over > 0.0f
                            ? juce::jlimit (0.0f, 1.0f, over / (2.0f * kneeWidth))
                            : 0.0f;
                        const auto effectiveRatio = 1.0f
                            + (compressorRatio - 1.0f) * knee;
                        const auto gain = std::pow (
                            compressorThreshold / state.compressorEnvelope,
                            1.0f - 1.0f / effectiveRatio);
                        processed *= gain;
                    }
                }
                wet[static_cast<std::size_t> (channel)] = processed;
            }

            for (int channel = 0; channel < activeChannels; ++channel)
            {
                const auto channelIndex = static_cast<std::size_t> (channel);
                auto& state = channelStates[channelIndex];

                state.wowPhase += twoPi * (0.5 + wow * 1.35) / currentSampleRate;
                state.flutterPhase += twoPi * (7.0 + flutter * 18.0) / currentSampleRate;
                if (state.wowPhase >= twoPi)
                    state.wowPhase -= twoPi;
                if (state.flutterPhase >= twoPi)
                    state.flutterPhase -= twoPi;

                const auto modulation = static_cast<float> (
                    std::sin (state.wowPhase) * wow * wowDepth
                    + std::sin (state.flutterPhase) * flutter * flutterDepth);
                state.vinylWowPhase += twoPi * 0.37 / currentSampleRate;
                state.vinylFlutterPhase += twoPi * 6.3 / currentSampleRate;
                if (state.vinylWowPhase >= twoPi)
                    state.vinylWowPhase -= twoPi;
                if (state.vinylFlutterPhase >= twoPi)
                    state.vinylFlutterPhase -= twoPi;
                const auto vinylModulationAmount = std::clamp (
                    (state.smoothedVinyl - 0.75f) * 4.0f, 0.0f, 1.0f);
                const auto vinylModulation = vinylModulationAmount > 0.0f
                    ? static_cast<float> (
                          std::sin (state.vinylWowPhase) * 0.00006
                          + std::sin (state.vinylFlutterPhase) * 0.000015)
                          * vinylModulationAmount * sampleRate
                    : 0.0f;
                const auto modulatedDelay = std::max (
                    1.0f, wetBaseDelay + modulation + vinylModulation);
                const auto dryInput = channels[channel][inputSample];

                delayLines[channelIndex][writePosition] = dryInput;
                delayLines[channelIndex + 2][writePosition] =
                    wet[channelIndex];
                dryDelayed[channelIndex] = readDelay (
                    delayLines[channelIndex], writePosition, dryDelay);
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
                const auto output = dryDelayed[static_cast<std::size_t> (channel)]
                                    + mix * (wetDelayed[static_cast<std::size_t> (channel)]
                                             - dryDelayed[static_cast<std::size_t> (channel)]);
                nonlinearBuffer.setSample (channel, sample, output);
            }

            writePosition = (writePosition + 1) % delayLines[0].size();
        }

        if (oversamplingIndex == 0)
        {
            processLimiterStages (baseRateBlock, limiter, 1,
                                  maximumLimiterGainReductionDb);
        }
        else
        {
            const auto oversamplingFactor = 1 << oversamplingIndex;
            auto& oversampling = *oversamplingProcessors[
                static_cast<std::size_t> (oversamplingIndex - 1)];
            auto oversampledBlock = oversampling.processSamplesUp (baseRateBlock);
            processLimiterStages (oversampledBlock, limiter, oversamplingFactor,
                                  maximumLimiterGainReductionDb);
            oversampling.processSamplesDown (baseRateBlock);
        }

        for (int sample = 0; sample < blockSize; ++sample)
            for (int channel = 0; channel < activeChannels; ++channel)
                channels[channel][blockOffset + sample] =
                    nonlinearBuffer.getSample (channel, sample);

        blockOffset += blockSize;
    }

    limiterGainReductionDb.store (maximumLimiterGainReductionDb,
                                  std::memory_order_relaxed);
}
}
