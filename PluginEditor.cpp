#include "PluginEditor.h"

#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace
{
struct ControlPlacement
{
    const char* parameterId;
    const char* label;
    int x;
    int y;
    int width;
    int height;
};

// LOCKED: Native-artwork coordinates. Layout changes require an explicit design revision.
constexpr std::array<ControlPlacement, 14> controlPlacements {{
    { "drive", "Drive", 293, 365, 114, 114 },
    { "vinyl", "Vinyl", 472, 365, 114, 114 },
    { "destroy", "Destroy", 650, 368, 220, 220 },
    { "wow", "Wow", 943, 365, 114, 114 },
    { "flutter", "Flutter", 1116, 365, 114, 114 },
    { "noise", "Noise", 293, 569, 114, 114 },
    { "dust", "Dust", 465, 569, 114, 114 },
    { "crush", "Crush", 943, 569, 114, 114 },
    { "comp", "Comp", 1116, 569, 114, 114 },
    { "hpf", "HPF", 300, 790, 100, 100 },
    { "lpf", "LPF", 453, 790, 100, 100 },
    { "limiter", "Limiter", 628, 790, 100, 100 },
    { "width", "Width", 962, 790, 100, 100 },
    { "mix", "Mix", 1129, 790, 100, 100 }
}};

struct OverlayPlacement
{
    int x;
    int y;
    int width;
    int height;
};
constexpr std::array<OverlayPlacement, 9> presetPlacements {{
    { 56, 163, 180, 34 },
    { 56, 201, 180, 34 },
    { 56, 239, 180, 34 },
    { 56, 277, 180, 34 },
    { 56, 315, 180, 34 },
    { 56, 353, 180, 34 },
    { 56, 391, 180, 34 },
    { 56, 429, 180, 34 },
    { 56, 467, 180, 34 }
}};

constexpr std::array<OverlayPlacement, 2> presetFileButtonPlacements {{
    { 56, 525, 84, 42 },
    { 151, 525, 85, 42 }
}};

constexpr std::array<OverlayPlacement, 4> oversamplingPlacements {{
    { 1302, 704, 40, 52 },
    { 1349, 704, 40, 52 },
    { 1396, 704, 40, 52 },
    { 1443, 704, 40, 52 }
}};

constexpr std::array<const char*, 4> oversamplingNames {{
    "1x", "2x", "4x", "8x"
}};

class ArtworkButton final : public juce::Button
{
public:
    explicit ArtworkButton (const juce::String& name)
        : juce::Button (name)
    {
    }

    void paintButton (juce::Graphics& graphics, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (graphics, shouldDrawButtonAsHighlighted,
                            shouldDrawButtonAsDown);
    }
};

}

MakosizerXAudioProcessorEditor::MakosizerXAudioProcessorEditor (
    MakosizerXAudioProcessor& processor)
    : juce::AudioProcessorEditor (processor),
      audioProcessor (processor)
{
    backgroundImage = juce::ImageCache::getFromMemory (
        BinaryData::makosizerx_background_png,
        BinaryData::makosizerx_background_pngSize);
    destroyKnobImage = juce::ImageCache::getFromMemory (
        BinaryData::dark_metal_knob_png,
        BinaryData::dark_metal_knob_pngSize);
    jassert (destroyKnobImage.isValid());

    destroyKnobArtwork.setImage (destroyKnobImage);
    destroyKnobArtwork.setBounds (638, 348, 244, 244);
    destroyKnobArtwork.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (destroyKnobArtwork);

    for (std::size_t i = 0; i < controlPlacements.size(); ++i)
    {
        const auto& placement = controlPlacements[i];
        auto& slider = sliders[i];

        slider.setName (placement.label);
        slider.setTooltip (placement.label);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setLookAndFeel (&rotaryLookAndFeel);
        slider.setOpaque (false);
        slider.setBounds (placement.x, placement.y, placement.width, placement.height);
        addAndMakeVisible (slider);
        slider.addListener (this);

        attachments[i] = std::make_unique<SliderAttachment> (
            processor.getParameterState(), placement.parameterId, slider);
    }

    const auto presets = processor.getPresetManager().getFactoryPresets();
    jassert (presets.size() == presetButtons.size());
    for (std::size_t i = 0; i < presetButtons.size() && i < presets.size(); ++i)
    {
        auto button = std::make_unique<ArtworkButton> (presets[i].name);
        button->setTitle (presets[i].name);
        button->setDescription (presets[i].description);
        button->setBounds (presetPlacements[i].x, presetPlacements[i].y,
                           presetPlacements[i].width, presetPlacements[i].height);
        button->setWantsKeyboardFocus (true);
        button->setClickingTogglesState (true);
        button->onClick = [this, name = presets[i].name]
        {
            loadPreset (name);
        };
        addAndMakeVisible (*button);
        presetButtons[i] = std::move (button);
    }

    constexpr std::array<const char*, 2> presetFileButtonNames {{
        "LOAD", "SAVE"
    }};
    for (std::size_t i = 0; i < presetFileButtons.size(); ++i)
    {
        auto button = std::make_unique<ArtworkButton> (presetFileButtonNames[i]);
        button->setTitle (presetFileButtonNames[i]);
        button->setDescription (i == 0 ? "Load a MakosizerX preset file"
                                       : "Save the current MakosizerX preset");
        button->setBounds (presetFileButtonPlacements[i].x,
                           presetFileButtonPlacements[i].y,
                           presetFileButtonPlacements[i].width,
                           presetFileButtonPlacements[i].height);
        button->setWantsKeyboardFocus (true);
        button->onClick = [this, i]
        {
            if (i == 0)
                openPresetLoadDialog();
            else
                openPresetSaveDialog();
        };
        addAndMakeVisible (*button);
        presetFileButtons[i] = std::move (button);
    }

    for (std::size_t i = 0; i < oversamplingButtons.size(); ++i)
    {
        auto button = std::make_unique<ArtworkButton> (oversamplingNames[i]);
        button->setTitle (oversamplingNames[i]);
        button->setDescription ("Set oversampling to " + juce::String (oversamplingNames[i]));
        button->setBounds (oversamplingPlacements[i].x, oversamplingPlacements[i].y,
                           oversamplingPlacements[i].width,
                           oversamplingPlacements[i].height);
        button->setWantsKeyboardFocus (true);
        button->setClickingTogglesState (true);
        button->onClick = [this, i]
        {
            setOversamplingMode (static_cast<int> (i));
        };
        addAndMakeVisible (*button);
        oversamplingButtons[i] = std::move (button);
    }

    lcdDisplay.setBounds (1000, 151, 232, 130);
    addAndMakeVisible (lcdDisplay);
    ledMeter.setBounds (1291, 113, 187, 526);
    addAndMakeVisible (ledMeter);
    // LOCKED: Match the PNG's native dimensions to preserve its 3:2 aspect ratio.
    setSize (1536, 1024);
    updateLCDDisplay();
    updateOversamplingSelection();
    startTimerHz (30);
}

MakosizerXAudioProcessorEditor::~MakosizerXAudioProcessorEditor()
{
    stopTimer();

    for (auto& slider : sliders)
    {
        slider.removeListener (this);
        slider.setLookAndFeel (nullptr);
    }
}

void MakosizerXAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    if (backgroundImage.isValid())
        graphics.drawImage (backgroundImage, getLocalBounds().toFloat());
    else
        graphics.fillAll (juce::Colour (0xff202020));

    const juce::Colour ledRed (0xffff3028);
    for (const auto& button : presetButtons)
    {
        if (button == nullptr)
            continue;

        if (! button->getToggleState())
            continue;

        const auto centre = juce::Point<float> (
            64.0f, button->getBounds().getCentreY() - 1.5f);
        graphics.setColour (ledRed.withAlpha (0.12f));
        graphics.fillEllipse (juce::Rectangle<float> (centre.x - 9.0f, centre.y - 9.0f,
                                                       18.0f, 18.0f));
        graphics.setColour (ledRed.withAlpha (0.28f));
        graphics.fillEllipse (juce::Rectangle<float> (centre.x - 6.0f, centre.y - 6.0f,
                                                       12.0f, 12.0f));
        graphics.setColour (ledRed);
        graphics.fillEllipse (juce::Rectangle<float> (centre.x - 4.0f, centre.y - 4.0f,
                                                       8.0f, 8.0f));
        graphics.setColour (juce::Colour (0xffffc4a8));
        graphics.fillEllipse (juce::Rectangle<float> (centre.x - 1.5f, centre.y - 2.0f,
                                                       2.5f, 2.5f));
    }

    for (const auto& button : oversamplingButtons)
    {
        if (button == nullptr)
            continue;

        if (button->getToggleState())
        {
            const auto centre = juce::Point<float> (
                button->getBounds().getCentreX(),
                static_cast<float> (button->getBounds().getBottom() - 6));
            graphics.setColour (ledRed.withAlpha (0.18f));
            graphics.fillEllipse (juce::Rectangle<float> (centre.x - 5.0f,
                                                           centre.y - 5.0f, 10.0f, 10.0f));
            graphics.setColour (ledRed);
            graphics.fillEllipse (juce::Rectangle<float> (centre.x - 2.5f,
                                                           centre.y - 2.5f, 5.0f, 5.0f));
        }
    }

    const auto gainReduction = audioProcessor.getLimiterGainReductionDb();
    const auto activeGainReductionSegments = juce::jlimit (
        0, 8, juce::roundToInt (gainReduction * (8.0f / 12.0f)));
    for (int segment = 0; segment < activeGainReductionSegments; ++segment)
    {
        const auto x = 817.0f;
        const auto y = 790.0f + static_cast<float> (7 - segment) * 16.0f;
        const auto colour = segment < 4 ? juce::Colour (0xff54d85e)
                                        : (segment < 6 ? juce::Colour (0xffffd84a)
                                                       : ledRed);
        graphics.setColour (colour.withAlpha (0.35f));
        graphics.fillRoundedRectangle (x - 3.0f, y - 3.0f, 28.0f, 18.0f, 3.0f);
        graphics.setColour (colour.withAlpha (0.85f));
        graphics.fillRoundedRectangle (x, y, 22.0f, 12.0f, 2.0f);
    }
}

void MakosizerXAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    if (! presetChangeActive)
        audioProcessor.getPresetManager().markCurrentPresetCustom();
    updateLCDDisplay();
}

void MakosizerXAudioProcessorEditor::timerCallback()
{
    const auto levels = audioProcessor.getAndResetOutputLevels();
    ledMeter.setLevels (levels.left, levels.right);
    outputLevel = juce::jmax (levels.left, levels.right);
    repaint();
    updateLCDDisplay();
    updateOversamplingSelection();
}

void MakosizerXAudioProcessorEditor::updateLCDDisplay()
{
    LCDDisplay::State state;
    state.presetName = audioProcessor.getPresetManager().getCurrentPresetName();
    if (state.presetName.isEmpty())
        state.presetName = "INIT";
    const auto sampleRate = audioProcessor.getSampleRate();
    state.sampleRate = sampleRate > 0.0
        ? (sampleRate >= 1000.0
               ? juce::String (sampleRate / 1000.0, 1) + " kHz"
               : juce::String (juce::roundToInt (sampleRate)) + " Hz")
        : juce::String ("OFFLINE");
    state.oversamplingFactor = audioProcessor.getOversamplingFactor();
    state.status = outputLevel > 0.001f ? "SIGNAL" : "READY";
    lcdDisplay.setState (state);
    updatePresetSelection();
}

void MakosizerXAudioProcessorEditor::updatePresetSelection()
{
    auto currentPreset = audioProcessor.getPresetManager().getCurrentPresetName();
    if (currentPreset.isEmpty())
        currentPreset = "INIT";

    auto selectedIndex = presetButtons.size() - 1;
    for (std::size_t i = 0; i < presetButtons.size(); ++i)
    {
        if (presetButtons[i] != nullptr
            && presetButtons[i]->getName().equalsIgnoreCase (currentPreset))
        {
            selectedIndex = i;
            break;
        }
    }

    for (std::size_t i = 0; i < presetButtons.size(); ++i)
        if (presetButtons[i] != nullptr)
            presetButtons[i]->setToggleState (i == selectedIndex,
                                              juce::dontSendNotification);
    repaint();
}

void MakosizerXAudioProcessorEditor::updateOversamplingSelection()
{
    const auto factor = audioProcessor.getOversamplingFactor();
    for (std::size_t i = 0; i < oversamplingButtons.size(); ++i)
    {
        const auto choiceFactor = 1 << static_cast<int> (i);
        oversamplingButtons[i]->setToggleState (choiceFactor == factor,
                                                juce::dontSendNotification);
    }
    repaint();
}

void MakosizerXAudioProcessorEditor::loadPreset (const juce::String& presetName)
{
    presetChangeActive = true;
    const auto result = audioProcessor.getPresetManager().loadFactoryPreset (presetName);
    presetChangeActive = false;

    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "Preset", result.getErrorMessage());

    updateLCDDisplay();
}

void MakosizerXAudioProcessorEditor::openPresetSaveDialog()
{
    if (presetFileChooser != nullptr)
        return;

    auto presetName = audioProcessor.getPresetManager().getCurrentPresetName();
    if (presetName.isEmpty())
        presetName = "INIT";

    auto fileName = juce::File::createLegalFileName (presetName);
    if (fileName.isEmpty())
        fileName = "CUSTOM";

    const auto initialFile = juce::File::getSpecialLocation (
        juce::File::userDocumentsDirectory).getChildFile (
            fileName + PresetManager::presetFileExtension);
    presetFileChooser = std::make_unique<juce::FileChooser> (
        "Save MakosizerX Preset", initialFile, "*.makopreset");

    const juce::Component::SafePointer<MakosizerXAudioProcessorEditor> safeThis (this);
    presetFileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto selectedFile = chooser.getResult();
            juce::MessageManager::callAsync (
                [safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->presetFileChooser.reset();
                });
            if (selectedFile.getFullPathName().isEmpty())
                return;

            if (! selectedFile.getFileName().endsWithIgnoreCase (
                     PresetManager::presetFileExtension))
            {
                selectedFile = selectedFile.getParentDirectory().getChildFile (
                    selectedFile.getFileNameWithoutExtension()
                    + PresetManager::presetFileExtension);
            }

            if (! selectedFile.existsAsFile())
            {
                safeThis->savePresetToFile (selectedFile);
                return;
            }

            const juce::Component::SafePointer<MakosizerXAudioProcessorEditor> overwriteTarget (
                safeThis.getComponent());
            juce::AlertWindow::showOkCancelBox (
                juce::AlertWindow::WarningIcon, "Overwrite Preset",
                "A preset file with this name already exists. Overwrite it?",
                "Overwrite", "Cancel", safeThis.getComponent(),
                juce::ModalCallbackFunction::create (
                    [overwriteTarget, selectedFile] (int result)
                    {
                        if (result == 1 && overwriteTarget != nullptr)
                            overwriteTarget->savePresetToFile (selectedFile);
                    }));
        });
}

void MakosizerXAudioProcessorEditor::openPresetLoadDialog()
{
    if (presetFileChooser != nullptr)
        return;

    const auto initialDirectory = juce::File::getSpecialLocation (
        juce::File::userDocumentsDirectory);
    presetFileChooser = std::make_unique<juce::FileChooser> (
        "Load MakosizerX Preset", initialDirectory, "*.makopreset");

    const juce::Component::SafePointer<MakosizerXAudioProcessorEditor> safeThis (this);
    presetFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto selectedFile = chooser.getResult();
            juce::MessageManager::callAsync (
                [safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->presetFileChooser.reset();
                });
            if (selectedFile.getFullPathName().isEmpty())
                return;

            if (! selectedFile.getFileName().endsWithIgnoreCase (
                     PresetManager::presetFileExtension))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon, "Load Preset",
                    "Please select a .makopreset file.");
                return;
            }

            safeThis->loadPresetFromFile (selectedFile);
        });
}

void MakosizerXAudioProcessorEditor::savePresetToFile (const juce::File& file)
{
    auto presetName = audioProcessor.getPresetManager().getCurrentPresetName();
    if (presetName.isEmpty())
        presetName = "INIT";

    const auto result = audioProcessor.getPresetManager().savePreset (file, presetName);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "Save Preset", result.getErrorMessage());
        return;
    }

    updateLCDDisplay();
}

void MakosizerXAudioProcessorEditor::loadPresetFromFile (const juce::File& file)
{
    const auto result = audioProcessor.getPresetManager().loadPreset (file);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "Load Preset", result.getErrorMessage());
        return;
    }

    updateLCDDisplay();
    updateOversamplingSelection();
    const juce::Component::SafePointer<MakosizerXAudioProcessorEditor> safeThis (this);
    juce::MessageManager::callAsync (
        [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->updateLCDDisplay();
        });
}

void MakosizerXAudioProcessorEditor::setOversamplingMode (int choiceIndex)
{
    auto* parameter = audioProcessor.getParameterState().getParameter ("oversampling");
    if (parameter == nullptr || choiceIndex < 0 || choiceIndex >= 4)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "Oversampling",
            "The requested oversampling mode is unavailable.");
        return;
    }

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (
        parameter->convertTo0to1 (static_cast<float> (choiceIndex)));
    parameter->endChangeGesture();
    updateOversamplingSelection();
    updateLCDDisplay();
}
