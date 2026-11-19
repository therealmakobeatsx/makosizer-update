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
    { "drive", "Drive", 298, 367, 115, 110 },
    { "vinyl", "Vinyl", 480, 367, 115, 110 },
    { "destroy", "Destroy", 650, 364, 240, 229 },
    { "wow", "Wow", 946, 367, 115, 110 },
    { "flutter", "Flutter", 1116, 367, 115, 110 },
    { "noise", "Noise", 296, 575, 115, 110 },
    { "dust", "Dust", 470, 575, 115, 110 },
    { "crush", "Crush", 943, 575, 115, 110 },
    { "comp", "Comp", 1116, 575, 115, 110 },
    { "hpf", "HPF", 310, 795, 96, 91 },
    { "lpf", "LPF", 462, 795, 96, 91 },
    { "limiter", "Limiter", 628, 795, 96, 91 },
    { "width", "Width", 967, 795, 96, 91 },
    { "mix", "Mix", 1127, 795, 96, 91 }
}};

struct OverlayPlacement
{
    int x;
    int y;
    int width;
    int height;
};

constexpr std::array<OverlayPlacement, 14> readoutPlacements {{
    { 316, 484, 74, 28 },
    { 490, 484, 74, 28 },
    { 707, 608, 102, 38 },
    { 954, 484, 74, 28 },
    { 1127, 484, 74, 28 },
    { 316, 678, 74, 28 },
    { 490, 678, 74, 28 },
    { 954, 678, 74, 28 },
    { 1127, 678, 74, 28 },
    { 322, 879, 70, 27 },
    { 470, 879, 70, 27 },
    { 640, 879, 70, 27 },
    { 966, 879, 70, 27 },
    { 1126, 879, 70, 27 }
}};

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
    { 1291, 692, 38, 47 },
    { 1334, 692, 38, 47 },
    { 1377, 692, 38, 47 },
    { 1420, 692, 38, 47 }
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
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            graphics.setColour (juce::Colours::white.withAlpha (
                shouldDrawButtonAsDown ? 0.12f : 0.06f));
            graphics.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 3.0f);
        }

        if (hasKeyboardFocus (false))
        {
            graphics.setColour (juce::Colours::white.withAlpha (0.8f));
            graphics.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f),
                                           3.0f, 1.0f);
        }
    }
};

enum ControlIndex : std::size_t
{
    driveControl,
    vinylControl,
    destroyControl,
    wowControl,
    flutterControl,
    noiseControl,
    dustControl,
    crushControl,
    compControl,
    hpfControl,
    lpfControl,
    limiterControl,
    widthControl,
    mixControl
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

        auto& readout = controlReadouts[i];
        readout.setJustificationType (juce::Justification::centred);
        readout.setColour (juce::Label::textColourId, juce::Colour (0xffe6e5dc));
        readout.setColour (juce::Label::backgroundColourId, juce::Colour (0xff080a08));
        readout.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
        readout.setOpaque (true);
        readout.setBounds (readoutPlacements[i].x, readoutPlacements[i].y,
                           readoutPlacements[i].width, readoutPlacements[i].height);
        readout.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (readout);

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

    // The dark LCD inset and its printed frame share the approved artwork bounds.
    lcdDisplay.setBounds (222, 138, 706, 154);
    addAndMakeVisible (lcdDisplay);
    ledMeter.setBounds (1287, 117, 178, 510);
    addAndMakeVisible (ledMeter);
    // LOCKED: Match the PNG's native dimensions to preserve its 3:2 aspect ratio.
    setSize (1536, 1024);
    updateLCDDisplay();
    updateControlReadouts();
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

    graphics.setColour (juce::Colour (0xf2101110));
    graphics.fillRoundedRectangle (juce::Rectangle<float> (31.0f, 609.0f, 202.0f, 44.0f),
                                   3.0f);
    graphics.setColour (juce::Colour (0xffc7c4b8));
    graphics.setFont (juce::Font (juce::FontOptions (
        18.0f, juce::Font::bold | juce::Font::italic)));
    graphics.drawText ("MKBTS", 39, 614, 83, 24, juce::Justification::centredLeft);
    graphics.setFont (juce::Font (juce::FontOptions (
        17.0f, juce::Font::bold | juce::Font::italic)));
    graphics.drawText ("SX-666", 128, 614, 78, 24, juce::Justification::centredLeft);

    graphics.setColour (juce::Colour (0xff170707));
    graphics.fillRoundedRectangle (juce::Rectangle<float> (91.0f, 710.0f, 75.0f, 39.0f),
                                   4.0f);
    graphics.setColour (juce::Colour (0xffff4b40));
    graphics.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold)));
    graphics.drawText (juce::String (audioProcessor.getSamplerDisplayNumber()),
                       93, 712, 71, 34, juce::Justification::centred);

    constexpr std::array<int, 4> padCentresX {{ 68, 110, 153, 196 }};
    constexpr std::array<int, 3> padCentresY {{ 801, 844, 887 }};
    const auto currentPad = audioProcessor.getActivePadIndex();
    const auto activePad = currentPad < 0 ? 0 : currentPad;
    for (int pad = 0; pad < 12; ++pad)
    {
        const auto x = padCentresX[static_cast<std::size_t> (pad % 4)];
        const auto y = padCentresY[static_cast<std::size_t> (pad / 4)];
        graphics.setColour (pad == activePad ? juce::Colour (0xffff7950).withAlpha (0.92f)
                                              : juce::Colour (0xe0181918));
        graphics.fillEllipse (static_cast<float> (x - 10), static_cast<float> (y - 9),
                              20.0f, 18.0f);
        graphics.setColour (pad == activePad ? juce::Colours::black
                                              : juce::Colour (0xffeeece0));
        graphics.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        graphics.drawText (juce::String (pad + 1), x - 12, y - 10, 24, 20,
                           juce::Justification::centred);
    }

    const auto accent = juce::Colour (0xffff5148);
    for (std::size_t i = 0; i < presetButtons.size(); ++i)
    {
        const auto& button = presetButtons[i];
        if (button == nullptr)
            continue;

        const auto bounds = button->getBounds().toFloat().reduced (1.0f);
        const auto isSelected = button->getToggleState();
        const auto isDeselectedInit = i == 0 && ! isSelected;
        const auto overlayAlpha = isDeselectedInit ? 1.0f : 0.12f;
        graphics.setColour (juce::Colours::black.withAlpha (overlayAlpha));
        graphics.fillRoundedRectangle (bounds, 3.0f);
        if (isDeselectedInit)
        {
            graphics.setColour (juce::Colour (0xff393d39));
            graphics.fillRoundedRectangle (juce::Rectangle<float> (61.0f, 168.0f,
                                                                     24.0f, 24.0f), 3.0f);
            graphics.setColour (juce::Colour (0xffd0cec4));
            graphics.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::plain)));
            graphics.drawText ("INIT", 96, 163, 136, 34, juce::Justification::centredLeft);
        }
        if (isSelected)
        {
            graphics.setColour (accent.withAlpha (0.13f));
            graphics.fillRoundedRectangle (bounds, 3.0f);
            graphics.setColour (accent.withAlpha (0.78f));
            graphics.drawRoundedRectangle (bounds, 3.0f, 1.0f);
            graphics.setColour (accent);
            graphics.fillRoundedRectangle (
                juce::Rectangle<float> (bounds.getX() - 4.0f,
                                        bounds.getCentreY() - 10.0f,
                                        4.0f, 20.0f),
                2.0f);
        }
    }

    for (const auto& button : oversamplingButtons)
    {
        if (button == nullptr)
            continue;

        const auto bounds = button->getBounds().toFloat().reduced (1.0f);
        graphics.setColour (juce::Colours::black);
        graphics.fillRoundedRectangle (bounds, 3.0f);
        if (button->getToggleState())
        {
            graphics.setColour (accent.withAlpha (0.32f));
            graphics.fillRoundedRectangle (bounds, 3.0f);
            graphics.setColour (accent.withAlpha (0.95f));
            graphics.drawRoundedRectangle (bounds, 3.0f, 1.5f);
        }
        graphics.setColour (juce::Colour (0xffe6e5dc));
        graphics.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::plain)));
        graphics.drawFittedText (button->getName(), button->getBounds(),
                                 juce::Justification::centred, 1);
    }
}

void MakosizerXAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    if (! presetChangeActive)
        audioProcessor.getPresetManager().markCurrentPresetCustom();
    updateLCDDisplay();
    updateControlReadouts();
}

void MakosizerXAudioProcessorEditor::sliderDragStarted (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    sliderGestureActive = true;
    audioProcessor.getPresetManager().markCurrentPresetCustom();
    updateLCDDisplay();
}

void MakosizerXAudioProcessorEditor::sliderDragEnded (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    sliderGestureActive = false;
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

void MakosizerXAudioProcessorEditor::updateControlReadouts()
{
    const auto formatFrequency = [] (double frequency)
    {
        return frequency >= 1000.0
                   ? juce::String (frequency / 1000.0, 1) + " kHz"
                   : juce::String (juce::roundToInt (frequency)) + " Hz";
    };

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto value = sliders[i].getValue();
        juce::String text;
        switch (i)
        {
            case driveControl: text = juce::String (value * 30.0, 1) + " dB"; break;
            case hpfControl:
            case lpfControl:
            {
                const auto* parameter = audioProcessor.getParameterState().getParameter (
                    i == hpfControl ? "hpf" : "lpf");
                const auto actualValue = parameter != nullptr
                    ? parameter->convertFrom0to1 (static_cast<float> (value))
                    : static_cast<float> (value);
                text = formatFrequency (actualValue);
                break;
            }
            case limiterControl: text = juce::String (-0.1 - value * 11.9, 1) + " dB"; break;
            case widthControl: text = juce::String (value * 200.0, 0) + "%"; break;
            case noiseControl: text = juce::String (value * 100.0, 0) + "%"; break;
            default: text = juce::String (value * 100.0, 0) + "%"; break;
        }
        controlReadouts[i].setText (text, juce::dontSendNotification);
    }
}

void MakosizerXAudioProcessorEditor::updateLCDDisplay()
{
    LCDDisplay::State state;
    state.presetName = audioProcessor.getPresetManager().getCurrentPresetName();
    if (state.presetName.isEmpty())
        state.presetName = "INIT";
    state.drive = static_cast<float> (sliders[driveControl].getValue());
    state.crush = static_cast<float> (sliders[crushControl].getValue());
    state.noise = static_cast<float> (sliders[noiseControl].getValue());
    state.wow = static_cast<float> (sliders[wowControl].getValue());
    state.flutter = static_cast<float> (sliders[flutterControl].getValue());
    state.outputLevel = outputLevel;
    state.oversamplingFactor = audioProcessor.getOversamplingFactor();
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
    updateControlReadouts();
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
    updateControlReadouts();
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
