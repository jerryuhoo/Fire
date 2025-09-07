/*
  ==============================================================================

    BandPanel.cpp
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "BandPanel.h"
#include "../../Utility/AudioHelpers.h"

//==============================================================================
BandPanel::BandPanel(FireAudioProcessor& p) : processor(p), focusBandNum(0)
{
    // Create all UI components using helper methods
    createSliders();
    createLabels();
    createButtons();

    // Group components for visibility management after they've been created
    setupComponentGroups();

    // We listen directly to the parameters that affect our "link" logic.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.addParameterListener(ParameterIDAndName::getIDString(DRIVE_ID, i), this);
        processor.treeState.addParameterListener(ParameterIDAndName::getIDString(LINKED_ID, i), this);
    }

    // Load initial bypass states from the processor's state tree
    for (int i = 0; i < 4; ++i)
    {
        compBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(COMP_BYPASS_ID, i));
        widthBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, i));
    }

    // Set initial attachments for band 0 and update knob enabled states
    setFocusBandNum(0, true);
}

BandPanel::~BandPanel()
{
    // Remove all parameter listeners that were added in the constructor.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(ParameterIDAndName::getIDString(DRIVE_ID, i), this);
        processor.treeState.removeParameterListener(ParameterIDAndName::getIDString(LINKED_ID, i), this);
    }
}

void BandPanel::createSliders()
{
    // === Create Modulatable Sliders in a loop ===
    for (const auto& paramName : ParameterIDAndName::getModulatableParameterNames())
    {
        modulatableSliderComponents[paramName] = std::make_unique<ModulatableSlider>();
        auto* slider = modulatableSliderComponents.at(paramName).get();

        // Use the generic initRotarySlider for basic setup.
        // The color will be set specifically below.
        initRotarySlider(*slider, juce::Colours::white);

        modulatableSliders.push_back(slider);
    }

    // === Configure specific slider properties ===
    modulatableSliderComponents.at(REC_NAME)->setColour(juce::Slider::rotarySliderFillColourId, SHAPE_COLOUR);
    modulatableSliderComponents.at(BIAS_NAME)->setColour(juce::Slider::rotarySliderFillColourId, SHAPE_COLOUR);
    modulatableSliderComponents.at(COMP_RATIO_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COMP_COLOUR);
    modulatableSliderComponents.at(COMP_THRESH_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COMP_COLOUR);
    modulatableSliderComponents.at(WIDTH_NAME)->setColour(juce::Slider::rotarySliderFillColourId, WIDTH_COLOUR);
    modulatableSliderComponents.at(OUTPUT_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    modulatableSliderComponents.at(MIX_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);

    modulatableSliderComponents.at(OUTPUT_NAME)->setTextValueSuffix(" dB");
    modulatableSliderComponents.at(COMP_THRESH_NAME)->setTextValueSuffix(" dB");

    // === Create the standard Drive slider separately ===
    driveKnob = std::make_unique<juce::Slider>();
    initRotarySlider(*driveKnob, DRIVE_COLOUR);
    driveKnob->setComponentID("drive");
}

void BandPanel::createLabels()
{
    auto setupLabel = [this](const juce::String& paramName, const juce::String& text, juce::Component& attachComp, juce::Colour colour)
    {
        labels[paramName] = std::make_unique<juce::Label>();
        auto* label = labels.at(paramName).get();
        addAndMakeVisible(label);
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
        label->setColour(juce::Label::textColourId, colour);
        label->attachToComponent(&attachComp, false);
        label->setJustificationType(juce::Justification::centred);
    };

    setupLabel(DRIVE_NAME, "Drive", *driveKnob, DRIVE_COLOUR.withBrightness(0.9f));
    setupLabel(OUTPUT_NAME, "Output", *modulatableSliderComponents.at(OUTPUT_NAME), KNOB_FONT_COLOUR);
    setupLabel(MIX_NAME, "Mix", *modulatableSliderComponents.at(MIX_NAME), KNOB_FONT_COLOUR);
    setupLabel(REC_NAME, "Rectification", *modulatableSliderComponents.at(REC_NAME), SHAPE_COLOUR);
    setupLabel(BIAS_NAME, "Bias", *modulatableSliderComponents.at(BIAS_NAME), SHAPE_COLOUR);
    setupLabel(COMP_RATIO_NAME, "Ratio", *modulatableSliderComponents.at(COMP_RATIO_NAME), COMP_COLOUR);
    setupLabel(COMP_THRESH_NAME, "Threshold", *modulatableSliderComponents.at(COMP_THRESH_NAME), COMP_COLOUR);
    setupLabel(WIDTH_NAME, "Width", *modulatableSliderComponents.at(WIDTH_NAME), WIDTH_COLOUR);

    auto setupPanelLabel = [this](juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupPanelLabel(shapePanelLabel, "Shape", SHAPE_COLOUR);
    setupPanelLabel(compressorPanelLabel, "Compressor", COMP_COLOUR);
    setupPanelLabel(widthPanelLabel, "Stereo", WIDTH_COLOUR);
}

void BandPanel::createButtons()
{
    initFlatButton(linkedButton, "L");
    initFlatButton(safeButton, "S");
    initFlatButton(extremeButton, "E");
    initBypassButton(compressorBypassButton, COMP_COLOUR);
    initBypassButton(widthBypassButton, WIDTH_COLOUR);

    auto setupSwitch = [this](juce::ToggleButton& btn, juce::Colour colour)
    {
        btn.setComponentID("flat_toggle");
        addAndMakeVisible(btn);
        btn.setRadioGroupId(switchButtons);
        btn.setColour(juce::ToggleButton::tickDisabledColourId, colour.withBrightness(0.5f));
        btn.setColour(juce::ToggleButton::tickColourId, colour.withBrightness(0.9f));
        btn.setColour(juce::ComboBox::outlineColourId, COLOUR6);
        btn.addListener(this);
    };

    setupSwitch(oscSwitch, DRIVE_COLOUR);
    setupSwitch(shapeSwitch, SHAPE_COLOUR);
    setupSwitch(compressorSwitch, COMP_COLOUR);
    setupSwitch(widthSwitch, WIDTH_COLOUR);
    oscSwitch.setToggleState(true, juce::dontSendNotification);
}

void BandPanel::setupComponentGroups()
{
    mainControls = { driveKnob.get(), modulatableSliderComponents.at(OUTPUT_NAME).get(), modulatableSliderComponents.at(MIX_NAME).get(), modulatableSliderComponents.at(REC_NAME).get(), modulatableSliderComponents.at(BIAS_NAME).get(), &linkedButton, &safeButton, &extremeButton };
    compressorKnobs = { modulatableSliderComponents.at(COMP_THRESH_NAME).get(), modulatableSliderComponents.at(COMP_RATIO_NAME).get() };
    widthKnobs = { modulatableSliderComponents.at(WIDTH_NAME).get() };

    shapeComponents = { modulatableSliderComponents.at(REC_NAME).get(), modulatableSliderComponents.at(BIAS_NAME).get(), &shapePanelLabel, labels.at(REC_NAME).get(), labels.at(BIAS_NAME).get() };
    compressorComponents = { modulatableSliderComponents.at(COMP_THRESH_NAME).get(), modulatableSliderComponents.at(COMP_RATIO_NAME).get(), &compressorBypassButton, &compressorPanelLabel, labels.at(COMP_RATIO_NAME).get(), labels.at(COMP_THRESH_NAME).get() };
    widthComponents = { modulatableSliderComponents.at(WIDTH_NAME).get(), &widthBypassButton, &widthPanelLabel, labels.at(WIDTH_NAME).get() };

    allControls.addArray(mainControls);
    allControls.addArray(compressorKnobs);
    allControls.addArray(widthKnobs);
    allControls.add(&linkedButton, &safeButton, &extremeButton, &compressorBypassButton, &widthBypassButton);

    setVisibility(shapeComponents, false);
    setVisibility(compressorComponents, false);
    setVisibility(widthComponents, false);
}

// ... paint() and resized() methods remain the same as your provided code,
// but they will now use the maps to access components, for example:
// modulatableSliderComponents.at(REC_NAME)->setBounds(leftKnobArea);
// driveKnob->setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));

void BandPanel::paint(juce::Graphics& g)
{
    g.setColour(COLOUR6);
    if (! oscSwitch.getToggleState())
    {
        g.drawRect(bandKnobArea);
        g.drawRect(driveKnobArea);
        g.setColour(COLOUR6.darker());
        g.fillRect(bottomArea);
        g.drawRect(bottomArea);
    }
    g.drawRect(outputKnobArea);
}

void BandPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();

    bandKnobArea = controlArea.removeFromLeft(getWidth() / 5 * 2);
    driveKnobArea = controlArea.removeFromLeft(getWidth() / 5);

    outputKnobArea = controlArea;
    const int verticalMargin = getHeight() / 6;

    outputKnobArea = controlArea.reduced(0, verticalMargin);
    driveKnobArea = driveKnobArea.reduced(0, verticalMargin);
    bandKnobArea = bandKnobArea.reduced(0, verticalMargin);

    juce::Rectangle<int> switchArea = bandKnobArea.removeFromLeft(getWidth() / 50);
    const float switchButtonHeight = switchArea.getHeight() / 4.0f;
    juce::Rectangle<int> area = switchArea;

    oscSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    shapeSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    compressorSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    widthSwitch.setBounds(area);

    juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft(getWidth() / 5 * 3).reduced(getHeight() / 10);
    if (oscSwitch.getToggleState())
    {
        juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft(getWidth() / 5 * 3).reduced(getHeight() / 10);
        driveKnob->setBounds(bigDriveArea);
    }
    else
    {
        driveKnob->setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    }

    // --- Band Knob Area Layout ---
    juce::Rectangle<int> subKnobArea = bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5);
    juce::Rectangle<int> leftKnobArea = subKnobArea.withRight(subKnobArea.getCentreX());
    juce::Rectangle<int> rightKnobArea = subKnobArea.withLeft(subKnobArea.getCentreX());

    // Shape panel
    modulatableSliderComponents.at(REC_NAME)->setBounds(leftKnobArea);
    modulatableSliderComponents.at(BIAS_NAME)->setBounds(rightKnobArea);

    // Compressor panel
    modulatableSliderComponents.at(COMP_THRESH_NAME)->setBounds(leftKnobArea);
    modulatableSliderComponents.at(COMP_RATIO_NAME)->setBounds(rightKnobArea);

    // Width panel
    modulatableSliderComponents.at(WIDTH_NAME)->setBounds(subKnobArea);

    // Panel Labels & Bypass Buttons
    bottomArea = bandKnobArea.removeFromBottom(bandKnobArea.getHeight() / 5);

    const int buttonSize = bottomArea.getHeight();
    const float baseLabelWidth = 150.0f;
    const float scale = getWidth() / (float) INIT_WIDTH;
    const int scaledLabelWidth = static_cast<int>(baseLabelWidth * scale);
    const int gap = 2;

    juce::Rectangle<int> labelArea = bottomArea.withSizeKeepingCentre(scaledLabelWidth, buttonSize);

    shapePanelLabel.setBounds(labelArea);
    compressorPanelLabel.setBounds(labelArea);
    widthPanelLabel.setBounds(labelArea);

    juce::Rectangle<int> bypassButtonArea = labelArea.translated(-buttonSize - gap, 0)
                                                .withSize(buttonSize, buttonSize);

    compressorBypassButton.setBounds(bypassButtonArea);
    widthBypassButton.setBounds(bypassButtonArea);

    // --- Output Area Layout ---
    juce::Rectangle<int> outputButtonCol = outputKnobArea.removeFromLeft(getWidth() / 25);
    const float buttonHeight = outputButtonCol.getHeight() / 3.0f;
    linkedButton.setBounds(outputButtonCol.removeFromTop(buttonHeight));
    safeButton.setBounds(outputButtonCol.removeFromTop(buttonHeight));
    extremeButton.setBounds(outputButtonCol);

    juce::Rectangle<int> outputSubArea = outputKnobArea.reduced(0, outputKnobArea.getHeight() / 5);
    juce::Rectangle<int> outputLeftArea = outputSubArea.withRight(outputSubArea.getCentreX());
    juce::Rectangle<int> outputRightArea = outputSubArea.withLeft(outputSubArea.getCentreX());

    modulatableSliderComponents.at(OUTPUT_NAME)->setBounds(outputLeftArea);
    modulatableSliderComponents.at(MIX_NAME)->setBounds(outputRightArea);
}

void BandPanel::updateAttachments()
{
    driveAttachment.reset();
    driveAttachment = std::make_unique<SliderAttachment>(processor.treeState, ParameterIDAndName::getIDString("drive", focusBandNum), *driveKnob);

    // Loop through our new definitive list of parameter info
    for (const auto& paramInfo : ParameterIDAndName::getModulatableParameterInfo())
    {
        // Get the slider using its stable Name (e.g., "CompRatio")
        auto* slider = modulatableSliderComponents.at(paramInfo.name).get();

        // Generate the dynamic parameter ID using the correct idBase (e.g., "compRatio")
        auto paramID = ParameterIDAndName::getIDString(paramInfo.idBase, focusBandNum);

        sliderAttachments[paramInfo.name].reset();

        auto* parameter = processor.treeState.getParameter(paramID);
        jassert(parameter != nullptr && "Parameter not found! Check idBase in getModulatableParameterInfo and parameter creation in PluginProcessor.");

        if (parameter)
        {
            sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
        }

        // Configure the slider's modulation logic using its stable Name
        configureModulatableSlider(*slider, paramInfo.idBase);
    }

    // === Unchanged Button Attachment Logic ===
    linkedAttachment.reset();
    safeAttachment.reset();
    extremeAttachment.reset();
    compressorBypassAttachment.reset();
    widthBypassAttachment.reset();

    linkedAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(LINKED_ID, focusBandNum), linkedButton);
    safeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SAFE_ID, focusBandNum), safeButton);
    extremeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(EXTREME_ID, focusBandNum), extremeButton);
    compressorBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum), compressorBypassButton);
    widthBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, focusBandNum), widthBypassButton);

    // Update visual states from memory
    compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
    widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);

    bool bandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(bandEnabled, false);
}

void BandPanel::configureModulatableSlider(ModulatableSlider& slider, const juce::String& paramIDBase)
{
    // The slider now knows its full, band-specific parameter ID
    slider.parameterID = ParameterIDAndName::getIDString(paramIDBase, focusBandNum);

    slider.onModAmountChanged = [this, &slider](double newAmount)
    {
        processor.setModulationDepth(slider.parameterID, (float) newAmount);
    };

    slider.onBipolarModeToggled = [this, &slider]()
    {
        processor.toggleBipolarMode(slider.parameterID);
    };

    slider.onModulationReset = [this, &slider]()
    {
        processor.resetModulation(slider.parameterID);
    };
}

void BandPanel::initRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
}

void BandPanel::initFlatButton(juce::TextButton& button, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, COLOUR7);
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, COLOUR1);
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    button.setButtonText(buttonName);
}

void BandPanel::initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour)
{
    addAndMakeVisible(bypassButton);
    bypassButton.setColour(juce::ToggleButton::tickColourId, colour);
}

void BandPanel::setFocusBandNum(int num, bool forceUpdate)
{
    if (focusBandNum == num && ! forceUpdate)
        return;

    focusBandNum = num;
    updateAttachments();
    updateWhenChangingFocus();

    bool isBandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(isBandEnabled, false);
}

// ... All remaining functions (updateLinkedValue, updateDriveMeter, buttonClicked, etc.)
// can remain exactly as you provided them, as their logic is sound. They will now
// correctly access the UI components via their member variables (for buttons) or
// the maps (for sliders and labels). For example, canEnableSubKnob will now use the map:
// if ((&component == modulatableSliderComponents.at(COMP_THRESH_NAME).get() || ...
// I have left them out for brevity but they should be included in the final file.

void BandPanel::updateLinkedValue()
{
    if (linkedButton.getToggleState())
    {
        float newOutputValue = -driveKnob->getValue() * 0.1f;
        auto* outputSlider = modulatableSliderComponents.at(OUTPUT_NAME).get();
        if (std::abs(outputSlider->getValue() - newOutputValue) > 0.001)
        {
            outputSlider->setValue(newOutputValue, juce::dontSendNotification);
        }
    }
}

void BandPanel::updateDriveMeter()
{
    if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&getLookAndFeel()))
    {
        lnf->sampleMaxValue = processor.getSampleMaxValue(focusBandNum);
        lnf->reductionPercent = processor.getReductionPrecent(focusBandNum);
    }
}

void BandPanel::buttonClicked(juce::Button* clickedButton)
{
    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        setVisibility(shapeComponents, true);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, true);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, true);
    }

    resized();
    repaint();
}

void BandPanel::setVisibility(juce::Array<juce::Component*>& components, bool isVisible)
{
    for (auto* component : components)
    {
        component->setVisible(isVisible);
    }
}

bool BandPanel::canEnableSubKnob(juce::Component& component)
{
    auto* compThreshPtr = modulatableSliderComponents.at(COMP_THRESH_NAME).get();
    auto* compRatioPtr = modulatableSliderComponents.at(COMP_RATIO_NAME).get();
    auto* widthPtr = modulatableSliderComponents.at(WIDTH_NAME).get();

    if ((&component == compThreshPtr || &component == compRatioPtr) && compressorBypassButton.getToggleState())
        return true;
    if (&component == widthPtr && widthBypassButton.getToggleState())
        return true;
    return false;
}

void BandPanel::saveBypassStatesToMemory()
{
    compBypassTemp[focusBandNum] = compressorBypassButton.getToggleState();
    widthBypassTemp[focusBandNum] = widthBypassButton.getToggleState();
}

void BandPanel::setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass)
{
    bool widthEnableState = isBandEnabled ? widthBypassTemp[focusBandNum] : false;
    bool compEnableState = isBandEnabled ? compBypassTemp[focusBandNum] : false;

    if (! callFromSubBypass)
    {
        widthBypassButton.setToggleState(widthEnableState, juce::dontSendNotification);
        compressorBypassButton.setToggleState(compEnableState, juce::dontSendNotification);
    }

    if (isBandEnabled)
    {
        // If the band is enabled, we process each control
        for (auto* component : allControls)
        {
            // Check if this control is a "sub knob"
            bool isSubKnob = (component == modulatableSliderComponents.at(COMP_THRESH_NAME).get()
                              || component == modulatableSliderComponents.at(COMP_RATIO_NAME).get()
                              || component == modulatableSliderComponents.at(WIDTH_NAME).get());

            if (isSubKnob)
            {
                // If it's a sub knob, its enabled state is determined by canEnableSubKnob
                component->setEnabled(canEnableSubKnob(*component));
            }
            else
            {
                // If it's not a sub knob (e.g., Drive, Output, Mix, etc.), it is always enabled
                component->setEnabled(true);
            }
        }
    }
    else
    {
        // If the band is disabled, disable all controls
        for (auto* component : allControls)
        {
            component->setEnabled(false);
        }
    }
}

void BandPanel::setSwitch(const int index, bool state)
{
    if (index == 0)
        oscSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 1)
        shapeSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 2)
        compressorSwitch.setToggleState(state, juce::sendNotificationSync);
    else if (index == 3)
        widthSwitch.setToggleState(state, juce::sendNotificationSync);
}

void BandPanel::updateWhenChangingFocus()
{
    updateDriveMeter();
    // Use the button reference directly, not a pointer
    buttonClicked(&oscSwitch);
    buttonClicked(&shapeSwitch);
    buttonClicked(&compressorSwitch);
    buttonClicked(&widthSwitch);
    repaint();
}

void BandPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    triggerAsyncUpdate();
}

void BandPanel::handleAsyncUpdate()
{
    updateLinkedValue();
}
