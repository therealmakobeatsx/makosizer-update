#include "LedMeter.h"

#include <array>
#include <cmath>

namespace
{
constexpr int segmentCount = 20;
constexpr double peakHoldDurationMs = 700.0;
constexpr float peakFallPerSecond = 0.8f;

const std::array<juce::Colour, 3> segmentColours {{
    juce::Colour (0xff55b765),
    juce::Colour (0xffe2bd55),
    juce::Colour (0xffd65349)
}};

juce::Colour colourForSegment (int segment)
{
    if (segment >= 17)
        return segmentColours[2];

    if (segment >= 14)
        return segmentColours[1];

    return segmentColours[0];
}

void drawChannel (juce::Graphics& graphics, juce::Rectangle<float> bounds,
                  const char* label, float level, float peak)
{
    constexpr float labelHeight = 16.0f;
    constexpr float segmentGap = 2.0f;

    graphics.setColour (juce::Colour (0xff9fa995));
    graphics.setFont (11.0f);
    graphics.drawFittedText (label, bounds.removeFromTop (labelHeight).toNearestInt(),
                             juce::Justification::centred, 1);

    auto track = bounds.reduced (3.0f, 1.0f);
    if (track.isEmpty())
        return;

    const auto segmentHeight = (track.getHeight() - segmentGap * (segmentCount - 1))
                              / static_cast<float> (segmentCount);
    if (segmentHeight <= 0.0f)
        return;

    const auto activeSegments = level * static_cast<float> (segmentCount);
    const auto fullSegments = static_cast<int> (activeSegments);
    const auto partialSegment = activeSegments - static_cast<float> (fullSegments);
    const auto peakSegment = juce::jlimit (0, segmentCount - 1,
                                           juce::roundToInt (peak * (segmentCount - 1)));

    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const auto y = track.getBottom()
                       - static_cast<float> (segment + 1) * segmentHeight
                       - static_cast<float> (segment) * segmentGap;
        auto segmentBounds = juce::Rectangle<float> (track.getX(), y,
                                                       track.getWidth(), segmentHeight);
        const auto colour = colourForSegment (segment);

        if (segment < fullSegments)
        {
            graphics.setColour (colour);
        }
        else if (segment == fullSegments && partialSegment > 0.0f)
        {
            graphics.setColour (colour.withAlpha (partialSegment));
        }
        else
        {
            graphics.setColour (colour.withAlpha (0.18f));
        }

        graphics.fillRoundedRectangle (segmentBounds, 1.5f);

        if (segment == peakSegment && peak > 0.0f)
        {
            graphics.setColour (juce::Colour (0xffe5e9d7));
            graphics.drawRoundedRectangle (segmentBounds.reduced (0.5f), 1.0f, 1.0f);
        }
    }
}
}

LedMeter::LedMeter()
{
    setOpaque (true);
    lastUpdateTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

float LedMeter::clampLevel (float level) noexcept
{
    return std::isfinite (level) ? juce::jlimit (0.0f, 1.0f, level) : 0.0f;
}

void LedMeter::setLevels (float leftLevel, float rightLevel) noexcept
{
    targetLeftLevel.store (clampLevel (leftLevel), std::memory_order_relaxed);
    targetRightLevel.store (clampLevel (rightLevel), std::memory_order_relaxed);
}

void LedMeter::setMonoLevel (float level) noexcept
{
    const auto clampedLevel = clampLevel (level);
    targetLeftLevel.store (clampedLevel, std::memory_order_relaxed);
    targetRightLevel.store (clampedLevel, std::memory_order_relaxed);
}

bool LedMeter::updateChannel (ChannelState& channel, float targetLevel,
                             double nowMs, float elapsedSeconds) noexcept
{
    const auto previousLevel = channel.level;
    const auto previousPeak = channel.peak;
    const auto responseTime = targetLevel > channel.level ? 0.035f : 0.18f;
    const auto smoothing = 1.0f - std::exp (-elapsedSeconds / responseTime);
    channel.level += (targetLevel - channel.level) * smoothing;

    if (channel.level > channel.peak)
    {
        channel.peak = channel.level;
        channel.peakHoldUntilMs = nowMs + peakHoldDurationMs;
    }
    else if (nowMs > channel.peakHoldUntilMs)
    {
        channel.peak = juce::jmax (channel.level,
                                   channel.peak - peakFallPerSecond * elapsedSeconds);
    }

    return std::abs (channel.level - previousLevel) > 0.001f
           || std::abs (channel.peak - previousPeak) > 0.001f;
}

void LedMeter::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto elapsedSeconds = static_cast<float> (
        juce::jlimit (0.0, 0.1, (nowMs - lastUpdateTimeMs) / 1000.0));
    lastUpdateTimeMs = nowMs;

    const auto leftChanged = updateChannel (
        leftChannel, targetLeftLevel.load (std::memory_order_relaxed),
        nowMs, elapsedSeconds);
    const auto rightChanged = updateChannel (
        rightChannel, targetRightLevel.load (std::memory_order_relaxed),
        nowMs, elapsedSeconds);

    if (leftChanged || rightChanged)
        repaint();
}

void LedMeter::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.fillAll (juce::Colour (0xff252a25));

    graphics.setColour (juce::Colour (0xff737b6d));
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    auto face = bounds.reduced (5.0f);
    graphics.setColour (juce::Colour (0xff101510));
    graphics.fillRoundedRectangle (face, 3.0f);

    auto channels = face.reduced (4.0f, 4.0f);
    const auto channelGap = 8.0f;
    const auto channelWidth = (channels.getWidth() - channelGap) * 0.5f;
    if (channelWidth <= 0.0f)
        return;

    auto leftBounds = juce::Rectangle<float> (channels.getX(), channels.getY(),
                                               channelWidth, channels.getHeight());
    auto rightBounds = juce::Rectangle<float> (channels.getX() + channelWidth + channelGap,
                                                channels.getY(), channelWidth,
                                                channels.getHeight());

    drawChannel (graphics, leftBounds, "L", leftChannel.level, leftChannel.peak);
    drawChannel (graphics, rightBounds, "R", rightChannel.level, rightChannel.peak);
}
