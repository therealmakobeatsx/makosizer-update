#include "LCDDisplay.h"

#include <array>
#include <cstddef>
#include <cmath>
#include <utility>

LCDDisplay::LCDDisplay()
{
    setOpaque (true);
}

float LCDDisplay::clampParameter (float value) noexcept
{
    return std::isfinite (value) ? juce::jlimit (0.0f, 1.0f, value) : 0.0f;
}

void LCDDisplay::setState (State newState)
{
    newState.drive = clampParameter (newState.drive);
    newState.crush = clampParameter (newState.crush);
    newState.noise = clampParameter (newState.noise);
    newState.wow = clampParameter (newState.wow);
    newState.flutter = clampParameter (newState.flutter);
    newState.outputLevel = clampParameter (newState.outputLevel);

    if (newState.presetName == state.presetName
        && newState.drive == state.drive
        && newState.crush == state.crush
        && newState.noise == state.noise
        && newState.wow == state.wow
        && newState.flutter == state.flutter
        && newState.outputLevel == state.outputLevel)
        return;

    state = std::move (newState);
    repaint();
}

void LCDDisplay::paint (juce::Graphics& g)
{
    const juce::Colour caseColour (0xff292d28);
    const juce::Colour bezelColour (0xff111510);
    const juce::Colour screenColour (0xff132017);
    const juce::Colour brightGreen (0xffb5ff9b);
    const juce::Colour midGreen (0xff72bd62);
    const juce::Colour dimGreen (0xff31583a);

    g.fillAll (caseColour);
    g.setColour (juce::Colour (0xff41473f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 9.0f);
    g.setColour (bezelColour);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (5.0f), 7.0f);

    auto screen = getLocalBounds().reduced (11);
    if (screen.isEmpty())
        return;

    g.setColour (screenColour);
    g.fillRoundedRectangle (screen.toFloat(), 3.0f);

    g.saveState();
    g.reduceClipRegion (screen);
    g.setColour (juce::Colour (0x0d9aff83));
    for (auto y = screen.getY(); y < screen.getBottom(); y += 4)
        g.fillRect (screen.getX(), y, screen.getWidth(), 1);
    g.restoreState();

    auto content = screen.reduced (10, 7);
    const auto headerHeight = juce::jlimit (20, 32, content.getHeight() / 4);
    auto header = content.removeFromTop (headerHeight);

    g.setColour (midGreen);
    g.setFont (juce::jmax (10.0f, header.getHeight() * 0.36f));
    g.drawFittedText ("PRESET", header.removeFromTop (header.getHeight() / 2),
                      juce::Justification::centredLeft, 1);

    g.setColour (brightGreen);
    g.setFont (juce::jmax (12.0f, header.getHeight() * 0.72f));
    g.drawFittedText (state.presetName, header,
                      juce::Justification::centredLeft, 1);

    content.removeFromTop (4);
    const auto columnGap = 12;
    const auto rowGap = 3;
    const auto columnWidth = juce::jmax (0, (content.getWidth() - columnGap) / 2);
    const auto rowHeight = juce::jmax (0, (content.getHeight() - rowGap * 2) / 3);

    const std::array<std::pair<const char*, float>, 6> metrics {{
        { "DRIVE", state.drive },
        { "CRUSH", state.crush },
        { "NOISE", state.noise },
        { "WOW", state.wow },
        { "FLUTTER", state.flutter },
        { "OUTPUT", state.outputLevel }
    }};

    for (std::size_t i = 0; i < metrics.size(); ++i)
    {
        const auto column = static_cast<int> (i % 2);
        const auto row = static_cast<int> (i / 2);
        auto cell = juce::Rectangle<int> (content.getX() + column * (columnWidth + columnGap),
                                          content.getY() + row * (rowHeight + rowGap),
                                          columnWidth, rowHeight);
        if (cell.isEmpty())
            continue;

        auto textRow = cell.removeFromTop (juce::jmax (12, cell.getHeight() - 4));
        g.setColour (midGreen);
        g.setFont (juce::jmax (9.0f, textRow.getHeight() * 0.38f));
        g.drawFittedText (metrics[i].first, textRow.removeFromLeft (textRow.getWidth() / 2),
                          juce::Justification::centredLeft, 1);

        g.setColour (brightGreen);
        g.setFont (juce::jmax (10.0f, textRow.getHeight() * 0.48f));
        g.drawFittedText (juce::String (metrics[i].second * 100.0f, 0) + "%",
                          textRow, juce::Justification::centredRight, 1);

        g.setColour (dimGreen);
        g.fillRect (cell.getX(), cell.getBottom() - 2, cell.getWidth(), 2);
        g.setColour (midGreen);
        g.fillRect (cell.getX(), cell.getBottom() - 2,
                    juce::roundToInt (cell.getWidth() * metrics[i].second), 2);
    }
}
