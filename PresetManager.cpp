#include "PresetManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>

namespace
{
constexpr std::size_t parameterCount = 14;

constexpr std::array<const char*, parameterCount> parameterIds {{
    "drive", "vinyl", "destroy", "wow", "flutter", "noise", "dust",
    "crush", "comp", "hpf", "lpf", "limiter", "width", "mix"
}};
constexpr const char* oversamplingParameterId = "oversampling";

struct FactoryPreset
{
    const char* name;
    const char* description;
    std::array<float, parameterCount> values;
};

constexpr std::array<FactoryPreset, 8> factoryPresets {{
    { "INIT", "A clean initial starting point.",
      {{ 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,
         0.00f, 20.0f, 20000.0f, 0.00f, 1.00f, 1.00f }} },
    { "SP303 VIBES", "Sampler-style crunch with a compact, punchy tone.",
      {{ 0.22f, 0.35f, 0.30f, 0.08f, 0.08f, 0.18f, 0.20f, 0.68f,
         0.30f, 35.0f, 14500.0f, 0.14f, 1.00f, 1.00f }} },
    { "DILLA DUST", "A softened beat with pronounced dust and surface texture.",
      {{ 0.12f, 0.22f, 0.10f, 0.12f, 0.08f, 0.42f, 0.80f, 0.24f,
         0.18f, 30.0f, 12500.0f, 0.08f, 1.00f, 1.00f }} },
    { "MPC CRUNCH", "Firm saturation, crushing, and stronger limiting.",
      {{ 0.42f, 0.12f, 0.75f, 0.03f, 0.02f, 0.10f, 0.05f, 0.85f,
         0.55f, 30.0f, 18000.0f, 0.68f, 1.00f, 1.00f }} },
    { "CASSETTE MELT", "Soft tape wear, modulation, and rounded transients.",
      {{ 0.22f, 0.55f, 0.35f, 0.28f, 0.32f, 0.28f, 0.36f, 0.38f,
         0.28f, 35.0f, 12500.0f, 0.18f, 0.95f, 1.00f }} },
    { "LOFI RADIO", "Narrower, noisy tape character with softened highs.",
      {{ 0.28f, 0.40f, 0.32f, 0.28f, 0.18f, 0.60f, 0.50f, 0.35f,
         0.35f, 120.0f, 8000.0f, 0.25f, 0.85f, 1.00f }} },
    { "UNDERGROUND", "Heavy degradation with unstable motion and texture.",
      {{ 0.72f, 0.45f, 0.95f, 0.40f, 0.40f, 0.55f, 0.65f, 0.82f,
         0.52f, 70.0f, 10000.0f, 0.65f, 0.75f, 1.00f }} },
    { "DIRTY MASTER", "A forceful, saturated master with firm compression.",
      {{ 0.55f, 0.32f, 0.55f, 0.12f, 0.10f, 0.12f, 0.08f, 0.48f,
         0.72f, 30.0f, 13500.0f, 0.72f, 1.00f, 1.00f }} }
}};

juce::Result validatePresetState (
    juce::ValueTree& state,
    juce::AudioProcessorValueTreeState& parameterState,
    const juce::Identifier& expectedType,
    bool allowMissingOversampling)
{
    if (! state.isValid() || state.getType() != expectedType)
        return juce::Result::fail ("Preset file has an invalid parameter state.");

    const juce::Identifier idProperty ("id");
    const juce::Identifier valueProperty ("value");
    std::array<bool, parameterCount> foundParameters {};
    bool foundOversampling = false;

    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto parameterStateTree = state.getChild (childIndex);
        if (! parameterStateTree.hasProperty (idProperty))
            return juce::Result::fail ("Preset contains a parameter without an ID.");

        const auto parameterId = parameterStateTree.getProperty (idProperty).toString();
        const auto found = std::find_if (parameterIds.begin(), parameterIds.end(),
                                         [&parameterId] (const char* knownId)
                                         {
                                             return parameterId == knownId;
                                         });
        const auto isOversampling = parameterId == oversamplingParameterId;
        if (found == parameterIds.end() && ! isOversampling)
            return juce::Result::fail ("Preset contains an unknown parameter: " + parameterId);

        if (isOversampling)
        {
            if (foundOversampling)
                return juce::Result::fail ("Preset contains a duplicate parameter: "
                                           + parameterId);
            foundOversampling = true;
        }
        else
        {
            const auto parameterIndex = static_cast<std::size_t> (
                std::distance (parameterIds.begin(), found));
            if (foundParameters[parameterIndex])
                return juce::Result::fail ("Preset contains a duplicate parameter: "
                                           + parameterId);
            foundParameters[parameterIndex] = true;
        }

        if (! parameterStateTree.hasProperty (valueProperty))
            return juce::Result::fail ("Preset is missing parameter value: " + parameterId);

        const auto value = parameterStateTree.getProperty (valueProperty);
        double numericValue = 0.0;
        if (value.isInt() || value.isInt64() || value.isDouble())
        {
            numericValue = static_cast<double> (value);
        }
        else if (value.isString())
        {
            const auto textValue = value.toString().trim().toStdString();
            char* parsedEnd = nullptr;
            numericValue = std::strtod (textValue.c_str(), &parsedEnd);
            if (parsedEnd == textValue.c_str() || *parsedEnd != '\0')
                return juce::Result::fail (
                    "Preset parameter value is not numeric: " + parameterId);
        }
        else
        {
            return juce::Result::fail ("Preset parameter value is not numeric: " + parameterId);
        }

        if (! std::isfinite (numericValue))
            return juce::Result::fail ("Preset parameter value is not finite: " + parameterId);

        auto* parameter = parameterState.getParameter (parameterId);
        if (parameter == nullptr)
            return juce::Result::fail ("Processor is missing parameter: " + parameterId);

        const auto clampedValue = parameter->getNormalisableRange().snapToLegalValue (
            static_cast<float> (numericValue));
        if (! std::isfinite (clampedValue))
            return juce::Result::fail ("Preset parameter value is invalid: " + parameterId);
        parameterStateTree.setProperty (valueProperty, clampedValue, nullptr);
    }

    for (std::size_t i = 0; i < foundParameters.size(); ++i)
        if (! foundParameters[i])
            return juce::Result::fail ("Preset is missing parameter: " + juce::String (parameterIds[i]));

    if (! foundOversampling && ! allowMissingOversampling)
        return juce::Result::fail ("Preset is missing parameter: "
                                   + juce::String (oversamplingParameterId));

    return juce::Result::ok();
}
}

PresetManager::PresetManager (ParameterState& state) noexcept
    : parameterState (state)
{
}

std::vector<PresetManager::FactoryPresetInfo> PresetManager::getFactoryPresets()
{
    std::vector<FactoryPresetInfo> presets;
    presets.reserve (factoryPresets.size() + 1);

    for (const auto& preset : factoryPresets)
        presets.push_back ({ preset.name, preset.description });

    presets.push_back ({ "CUSTOM", "Keep the current parameter settings." });
    return presets;
}

juce::Result PresetManager::loadFactoryPreset (const juce::String& presetName)
{
    if (presetName.equalsIgnoreCase ("CUSTOM"))
    {
        markCurrentPresetCustom();
        return juce::Result::ok();
    }

    const FactoryPreset* selectedPreset = nullptr;
    for (const auto& preset : factoryPresets)
    {
        if (presetName.equalsIgnoreCase (preset.name))
        {
            selectedPreset = &preset;
            break;
        }
    }

    if (selectedPreset == nullptr)
        return juce::Result::fail ("Factory preset not found: " + presetName);

    std::array<juce::RangedAudioParameter*, parameterCount> parameters {};
    for (std::size_t i = 0; i < parameterIds.size(); ++i)
    {
        parameters[i] = parameterState.getParameter (parameterIds[i]);
        if (parameters[i] == nullptr)
            return juce::Result::fail ("Processor is missing parameter: "
                                       + juce::String (parameterIds[i]));

        if (! std::isfinite (selectedPreset->values[i]))
            return juce::Result::fail ("Factory preset contains an invalid value.");
    }

    for (std::size_t i = 0; i < parameterIds.size(); ++i)
    {
        auto* parameter = parameters[i];
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (selectedPreset->values[i]));
        parameter->endChangeGesture();
    }

    parameterState.state.setProperty ("presetName", selectedPreset->name, nullptr);
    return juce::Result::ok();
}

void PresetManager::markCurrentPresetCustom()
{
    parameterState.state.setProperty ("presetName", "CUSTOM", nullptr);
}

juce::Result PresetManager::savePreset (const juce::File& file,
                                        const juce::String& presetName)
{
    const auto name = presetName.trim();
    if (name.isEmpty())
        return juce::Result::fail ("Preset name cannot be empty.");

    auto state = parameterState.copyState();
    auto validation = validatePresetState (state, parameterState,
                                           parameterState.state.getType(), false);
    if (validation.failed())
        return validation;

    state.setProperty ("presetName", name, nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr)
        return juce::Result::fail ("Could not serialize preset state.");

    const auto directoryResult = file.getParentDirectory().createDirectory();
    if (directoryResult.failed())
        return directoryResult;

    if (! file.replaceWithText (xml->toString()))
        return juce::Result::fail ("Could not write preset file: " + file.getFullPathName());

    parameterState.state.setProperty ("presetName", name, nullptr);
    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("Preset file does not exist: " + file.getFullPathName());

    const auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return juce::Result::fail ("Preset file is not valid XML: " + file.getFullPathName());

    auto state = juce::ValueTree::fromXml (*xml);
    const auto validation = validatePresetState (state, parameterState,
                                                  parameterState.state.getType(), true);
    if (validation.failed())
        return validation;

    const juce::Identifier idProperty ("id");
    const auto oversamplingState = state.getChildWithProperty (
        idProperty, juce::var ("oversampling"));
    if (! oversamplingState.isValid())
    {
        const auto currentState = parameterState.copyState().getChildWithProperty (
            idProperty, juce::var (oversamplingParameterId));
        if (currentState.isValid())
            state.addChild (currentState.createCopy(), -1, nullptr);
    }

    if (state.getProperty ("presetName").toString().trim().isEmpty())
        state.setProperty ("presetName", "CUSTOM", nullptr);

    parameterState.replaceState (state);
    return juce::Result::ok();
}

juce::String PresetManager::getCurrentPresetName() const
{
    return parameterState.state.getProperty ("presetName").toString();
}
