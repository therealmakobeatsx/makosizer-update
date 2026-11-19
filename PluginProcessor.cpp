#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <memory>

void MakosizerXAudioProcessor::accumulatePeak (std::atomic<float>& peak,
                                               float level) noexcept
{
    if (! std::isfinite (level))
        return;

    const auto normalizedLevel = juce::jlimit (0.0f, 1.0f, level);
    auto previousPeak = peak.load (std::memory_order_relaxed);

    while (previousPeak < normalizedLevel
           && ! peak.compare_exchange_weak (previousPeak, normalizedLevel,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed))
    {
    }
}

MakosizerXAudioProcessor::MakosizerXAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameterState (*this, nullptr, "Parameters", createParameterLayout()),
    presetManager (parameterState),
    parameterValues {{
        parameterState.getRawParameterValue ("drive"),
        parameterState.getRawParameterValue ("vinyl"),
        parameterState.getRawParameterValue ("destroy"),
        parameterState.getRawParameterValue ("wow"),
        parameterState.getRawParameterValue ("flutter"),
        parameterState.getRawParameterValue ("noise"),
        parameterState.getRawParameterValue ("dust"),
        parameterState.getRawParameterValue ("crush"),
        parameterState.getRawParameterValue ("comp"),
        parameterState.getRawParameterValue ("hpf"),
        parameterState.getRawParameterValue ("lpf"),
        parameterState.getRawParameterValue ("limiter"),
        parameterState.getRawParameterValue ("width"),
        parameterState.getRawParameterValue ("mix")
    }},
      oversamplingValue (parameterState.getRawParameterValue ("oversampling"))
{
}

MakosizerXAudioProcessor::ParameterState::ParameterLayout
MakosizerXAudioProcessor::createParameterLayout()
{
    using Parameter = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    ParameterState::ParameterLayout layout;
    const auto addUnitParameter = [&layout] (const char* id, const char* name,
                                              float defaultValue = 0.0f)
    {
        layout.add (std::make_unique<Parameter> (
            juce::ParameterID { id, 1 }, name, Range (0.0f, 1.0f, 0.001f), defaultValue));
    };

    addUnitParameter ("drive", "Drive");
    addUnitParameter ("vinyl", "Vinyl");
    addUnitParameter ("destroy", "Destroy");
    addUnitParameter ("wow", "Wow");
    addUnitParameter ("flutter", "Flutter");
    addUnitParameter ("noise", "Noise");
    addUnitParameter ("dust", "Dust");
    addUnitParameter ("crush", "Crush");
    addUnitParameter ("comp", "Comp");

    layout.add (std::make_unique<Parameter> (
        juce::ParameterID { "hpf", 1 }, "HPF",
        Range (20.0f, 20000.0f, 1.0f, 0.25f), 20.0f));
    layout.add (std::make_unique<Parameter> (
        juce::ParameterID { "lpf", 1 }, "LPF",
        Range (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    addUnitParameter ("limiter", "Limiter");
    layout.add (std::make_unique<Parameter> (
        juce::ParameterID { "width", 1 }, "Width", Range (0.0f, 2.0f, 0.001f), 1.0f));
    addUnitParameter ("mix", "Mix", 1.0f);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "oversampling", 1 }, "Oversampling",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 0));

    return layout;
}

void MakosizerXAudioProcessor::prepareToPlay (double sampleRate,
                                             int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused (maximumExpectedSamplesPerBlock);
    dsp.prepareToPlay (sampleRate);
    setLatencySamples (dsp.getLatencySamples());
}

void MakosizerXAudioProcessor::releaseResources()
{
    dsp.reset();
}

bool MakosizerXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

    return output == layouts.getMainInputChannelSet();
}

void MakosizerXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto numSamples = buffer.getNumSamples();
    const auto numOutputChannels = juce::jmin (getTotalNumOutputChannels(),
                                               buffer.getNumChannels());
    if (numSamples <= 0 || numOutputChannels <= 0)
        return;

    const auto loadParameter = [this] (std::size_t index)
    {
        return parameterValues[index]->load (std::memory_order_relaxed);
    };
    const MakosizerX::DSPParameters parameters {
        loadParameter (0), loadParameter (1), loadParameter (2), loadParameter (3),
        loadParameter (4), loadParameter (5), loadParameter (6), loadParameter (7),
        loadParameter (8), loadParameter (9), loadParameter (10), loadParameter (11),
        loadParameter (12), loadParameter (13)
    };

    dsp.process (buffer.getArrayOfWritePointers(), numOutputChannels, numSamples, parameters);

    accumulatePeak (leftOutputPeak, buffer.getMagnitude (0, 0, numSamples));
    if (numOutputChannels > 1)
        accumulatePeak (rightOutputPeak, buffer.getMagnitude (1, 0, numSamples));
}

juce::AudioProcessorEditor* MakosizerXAudioProcessor::createEditor()
{
    return new MakosizerXAudioProcessorEditor (*this);
}

bool MakosizerXAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String MakosizerXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MakosizerXAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MakosizerXAudioProcessor::producesMidi() const
{
    return false;
}

bool MakosizerXAudioProcessor::isMidiEffect() const
{
    return false;
}

double MakosizerXAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MakosizerXAudioProcessor::getNumPrograms()
{
    return 1;
}

int MakosizerXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MakosizerXAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String MakosizerXAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void MakosizerXAudioProcessor::changeProgramName (int index,
                                                  const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void MakosizerXAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameterState.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void MakosizerXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameterState.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const juce::Identifier idProperty ("id");
            auto oversamplingState = state.getChildWithProperty (
                idProperty, juce::var ("oversampling"));
            if (! oversamplingState.isValid())
            {
                const auto currentOversamplingState = parameterState.copyState()
                    .getChildWithProperty (idProperty, juce::var ("oversampling"));
                if (currentOversamplingState.isValid())
                {
                    oversamplingState = currentOversamplingState.createCopy();
                    oversamplingState.setProperty (
                        "value", parameterState.getParameter ("oversampling")
                                     ->convertTo0to1 (0.0f),
                        nullptr);
                    state.addChild (oversamplingState, -1, nullptr);
                }
            }

            parameterState.replaceState (state);
        }
    }
}

MakosizerXAudioProcessor::OutputLevels
MakosizerXAudioProcessor::getAndResetOutputLevels() noexcept
{
    return {
        leftOutputPeak.exchange (0.0f, std::memory_order_relaxed),
        rightOutputPeak.exchange (0.0f, std::memory_order_relaxed)
    };
}

int MakosizerXAudioProcessor::getOversamplingFactor() const noexcept
{
    const auto selectedIndex = juce::jlimit (
        0, 3, juce::roundToInt (oversamplingValue->load (std::memory_order_relaxed)));
    return 1 << selectedIndex;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MakosizerXAudioProcessor();
}
