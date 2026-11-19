#include "LCDDisplay.h"

#include <array>
#include <utility>

LCDDisplay::LCDDisplay()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void LCDDisplay::setState (State newState)
{
    if (newState.presetName == state.presetName
        && newState.sampleRate == state.sampleRate
        && newState.oversamplingFactor == state.oversamplingFactor
        && newState.bitDepth == state.bitDepth
        && newState.status == state.status)
        return;

    state = std::move (newState);
    repaint();
}

void LCDDisplay::paint (juce::Graphics& graphics)
{
    const auto valueColumn = getLocalBounds().withTrimmedLeft (107).reduced (0, 3);
    if (valueColumn.isEmpty())
        return;

    const std::array<juce::String, 5> values {{
        state.presetName,
        state.sampleRate,
        juce::String (state.oversamplingFactor) + "x",
        state.bitDepth,
        state.status
    }};

    graphics.setColour (juce::Colour (0xff07140a));
    graphics.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const auto row = valueColumn.withY (valueColumn.getY() + static_cast<int> (i) * 24)
                                    .withHeight (19);
        graphics.drawFittedText (values[i], row,
                                 juce::Justification::centredLeft, 1);
    }
}
