#include "MakosizerXLookAndFeel.h"

#include <array>
#include <iostream>
#include <utility>

namespace
{
juce::Image renderKnob (MakosizerXLookAndFeel& lookAndFeel,
                        const juce::String& name, int size, float position)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics graphics (image);
    juce::Slider slider;
    slider.setName (name);
    lookAndFeel.drawRotarySlider (
        graphics, 0, 0, size, size, position,
        juce::MathConstants<float>::pi * 1.2f,
        juce::MathConstants<float>::pi * 2.8f, slider);
    return image;
}

bool imagesDiffer (const juce::Image& first, const juce::Image& second)
{
    for (int y = 0; y < first.getHeight(); ++y)
        for (int x = 0; x < first.getWidth(); ++x)
            if (first.getPixelAt (x, y).getARGB()
                != second.getPixelAt (x, y).getARGB())
                return true;

    return false;
}

bool indicatorMovesAcrossFullRange (MakosizerXLookAndFeel& lookAndFeel,
                                    const juce::String& name, int size)
{
    constexpr int steps = 100;
    auto previous = renderKnob (lookAndFeel, name, size, 0.0f);
    const auto minimum = previous;
    juce::Image midpoint;

    for (int step = 1; step <= steps; ++step)
    {
        const auto position = static_cast<float> (step) / steps;
        auto current = renderKnob (lookAndFeel, name, size, position);
        if (! imagesDiffer (previous, current))
            return false;

        if (step == steps / 2)
            midpoint = current;
        previous = std::move (current);
    }

    return imagesDiffer (minimum, midpoint)
           && imagesDiffer (midpoint, previous)
           && imagesDiffer (minimum, previous);
}
}

int main()
{
    MakosizerXLookAndFeel lookAndFeel;
    constexpr std::array<int, 3> knobSizes {{ 100, 114, 220 }};
    constexpr std::array<const char*, 2> knobNames {{ "Drive", "Destroy" }};

    for (const auto* name : knobNames)
        for (const auto size : knobSizes)
            if (! indicatorMovesAcrossFullRange (lookAndFeel, name, size))
            {
                std::cerr << "The " << name << " indicator did not move smoothly at "
                          << size << " px.\n";
                return 1;
            }

    return 0;
}
