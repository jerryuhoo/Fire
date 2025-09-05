/*
  ==============================================================================

    BandPanel.cpp
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "BandPanel.h"

//==============================================================================
BandPanel::BandPanel(FireAudioProcessor& p) : processor(p)
{
    // auto setParameterIDs = [](std::vector<ModulatableSlider*>& knobs,
    //                           const juce::String& id1,
    //                           const juce::String& id2,
    //                           const juce::String& id3,
    //                           const juce::String& id4)
    // {
    //     knobs[0]->parameterID = id1;
    //     knobs[1]->parameterID = id2;
    //     knobs[2]->parameterID = id3;
    //     knobs[3]->parameterID = id4;
    // };

    modulatableKnobs.push_back({ &recKnob, REC_ID });
    modulatableKnobs.push_back({ &biasKnob, BIAS_ID });

    // We listen directly to the parameters that affect our "link" logic.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.addParameterListener(getParamID(DRIVE_ID, i), this);
        processor.treeState.addParameterListener(getParamID(LINKED_ID, i), this);
    }

    // Initialize a single set of sliders
    initRotarySlider(driveKnob, DRIVE_COLOUR);
    initRotarySlider(outputKnob, COLOUR1);
    initRotarySlider(mixKnob, COLOUR1);
    initRotarySlider(recKnob, SHAPE_COLOUR);
    initRotarySlider(biasKnob, SHAPE_COLOUR);
    initRotarySlider(compRatioKnob, COMP_COLOUR);
    initRotarySlider(compThreshKnob, COMP_COLOUR);
    initRotarySlider(widthKnob, WIDTH_COLOUR);

    driveKnob.setComponentID("drive");

    outputKnob.setTextValueSuffix(" dB");
    compThreshKnob.setTextValueSuffix(" dB");

    // Initialize a single set of buttons
    initFlatButton(linkedButton, "L");
    initFlatButton(safeButton, "S");
    initFlatButton(extremeButton, "E");
    initBypassButton(compressorBypassButton, COMP_COLOUR);
    initBypassButton(widthBypassButton, WIDTH_COLOUR);

    // Initialize labels
    auto setupLabel = [this](juce::Label& label, const juce::String& text, juce::Component& attachComp, juce::Colour colour)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withHeight(KNOB_FONT_SIZE)
                .withStyle("Plain") });
        label.setColour(juce::Label::textColourId, colour);
        label.attachToComponent(&attachComp, false);
        label.setJustificationType(juce::Justification::centred);
    };

    setupLabel(driveLabel, "Drive", driveKnob, DRIVE_COLOUR.withBrightness(0.9f));
    setupLabel(outputLabel, "Output", outputKnob, KNOB_FONT_COLOUR);
    setupLabel(mixLabel, "Mix", mixKnob, KNOB_FONT_COLOUR);
    setupLabel(recLabel, "Rectification", recKnob, SHAPE_COLOUR);
    setupLabel(biasLabel, "Bias", biasKnob, SHAPE_COLOUR);
    setupLabel(CompRatioLabel, "Ratio", compRatioKnob, COMP_COLOUR);
    setupLabel(CompThreshLabel, "Threshold", compThreshKnob, COMP_COLOUR);
    setupLabel(widthLabel, "Width", widthKnob, WIDTH_COLOUR);

    auto setupPanelLabel = [this](juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withHeight(KNOB_FONT_SIZE)
                .withStyle("Plain") });
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupPanelLabel(shapePanelLabel, "Shape", SHAPE_COLOUR);
    setupPanelLabel(compressorPanelLabel, "Compressor", COMP_COLOUR);
    setupPanelLabel(widthPanelLabel, "Stereo", WIDTH_COLOUR);

    // Initialize panel-switching toggles
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

    // Group components for visibility management
    mainControls = { &driveKnob, &outputKnob, &mixKnob, &recKnob, &biasKnob, &linkedButton, &safeButton, &extremeButton };
    compressorKnobs = { &compThreshKnob, &compRatioKnob };
    widthKnobs = { &widthKnob };

    shapeComponents = { &recKnob, &biasKnob, &shapePanelLabel, &recLabel, &biasLabel };
    compressorComponents = { &compThreshKnob, &compRatioKnob, &compressorBypassButton, &compressorPanelLabel, &CompRatioLabel, &CompThreshLabel };
    widthComponents = { &widthKnob, &widthBypassButton, &widthPanelLabel, &widthLabel };

    allControls = { &driveKnob, &outputKnob, &mixKnob, &recKnob, &biasKnob, &compRatioKnob, &compThreshKnob, &widthKnob, &linkedButton, &safeButton, &extremeButton, &compressorBypassButton, &widthBypassButton };

    setVisibility(shapeComponents, false);
    setVisibility(compressorComponents, false);
    setVisibility(widthComponents, false);

    // Load initial bypass states from the processor's state tree
    for (int i = 0; i < 4; ++i)
    {
        compBypassTemp[i] = *processor.treeState.getRawParameterValue(getParamID(COMP_BYPASS_ID, i));
        widthBypassTemp[i] = *processor.treeState.getRawParameterValue(getParamID(WIDTH_BYPASS_ID, i));
    }

    // Set initial attachments for band 0 and update knob enabled states
    setFocusBandNum(0, true);

    for (int i = 0; i < 4; ++i)
    {
        bool bandEnabled = *processor.treeState.getRawParameterValue(getParamID(BAND_ENABLE_ID, i));
        setBandKnobsStates(bandEnabled, false);
    }
}

BandPanel::~BandPanel()
{
    // Remove all parameter listeners that were added in the constructor.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(getParamID(DRIVE_ID, i), this);
        processor.treeState.removeParameterListener(getParamID(LINKED_ID, i), this);
    }

    oscSwitch.removeListener(this);
    shapeSwitch.removeListener(this);
    compressorSwitch.removeListener(this);
    widthSwitch.removeListener(this);
}

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
        driveKnob.setBounds(bigDriveArea);
    }
    else
    {
        driveKnob.setBounds(driveKnobArea.reduced(0, bandKnobArea.getHeight() / 5));
    }

    // --- Band Knob Area Layout ---
    juce::Rectangle<int> subKnobArea = bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5);
    juce::Rectangle<int> leftKnobArea = subKnobArea.withRight(subKnobArea.getCentreX());
    juce::Rectangle<int> rightKnobArea = subKnobArea.withLeft(subKnobArea.getCentreX());

    // Shape panel
    recKnob.setBounds(leftKnobArea);
    biasKnob.setBounds(rightKnobArea);

    // Compressor panel
    compThreshKnob.setBounds(leftKnobArea);
    compRatioKnob.setBounds(rightKnobArea);

    // Width panel
    widthKnob.setBounds(subKnobArea);

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
    // 1. First, define and set the bounds for the button column on the left.
    juce::Rectangle<int> outputButtonCol = outputKnobArea.removeFromLeft(getWidth() / 25);
    const float buttonHeight = outputButtonCol.getHeight() / 3.0f;
    linkedButton.setBounds(outputButtonCol.removeFromTop(buttonHeight));
    safeButton.setBounds(outputButtonCol.removeFromTop(buttonHeight));
    extremeButton.setBounds(outputButtonCol);

    // 2. Now, the 'outputKnobArea' has been correctly reduced.
    //    We can proceed with laying out the knobs in the remaining space.
    juce::Rectangle<int> outputSubArea = outputKnobArea.reduced(0, outputKnobArea.getHeight() / 5);
    juce::Rectangle<int> outputLeftArea = outputSubArea.withRight(outputSubArea.getCentreX());
    juce::Rectangle<int> outputRightArea = outputSubArea.withLeft(outputSubArea.getCentreX());

    outputKnob.setBounds(outputLeftArea);
    mixKnob.setBounds(outputRightArea);
}

void BandPanel::updateLinkedValue()
{
    // This is called on the message thread after a parameter change.
    // Check the "linked" state for the currently focused band.
    if (linkedButton.getToggleState())
    {
        float newOutputValue = -driveKnob.getValue() * 0.1f;

        if (std::abs(outputKnob.getValue() - newOutputValue) > 0.001)
        {
            // Use dontSendNotification to prevent a feedback loop.
            outputKnob.setValue(newOutputValue, juce::dontSendNotification);
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
    // This handles the main panel switching logic
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

    resized(); // Recalculate layout, especially for the drive knob size
    repaint();
}

void BandPanel::configureModulatableSlider(ModulatableSlider& slider, const juce::String& paramIDBase)
{
    slider.parameterID = getParamID(paramIDBase, focusBandNum);

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

void BandPanel::updateAttachments()
{
    // Detach old parameters by resetting unique_ptrs
    driveAttachment.reset();
    outputAttachment.reset();
    mixAttachment.reset();
    recAttachment.reset();
    biasAttachment.reset();
    compRatioAttachment.reset();
    compThreshAttachment.reset();
    widthAttachment.reset();
    linkedAttachment.reset();
    safeAttachment.reset();
    extremeAttachment.reset();
    compressorBypassAttachment.reset();
    widthBypassAttachment.reset();

    // Attach UI components to the new band's parameters
    driveAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(DRIVE_ID, focusBandNum), driveKnob);
    outputAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(OUTPUT_ID, focusBandNum), outputKnob);
    mixAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(MIX_ID, focusBandNum), mixKnob);
    recAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(REC_ID, focusBandNum), recKnob);
    biasAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(BIAS_ID, focusBandNum), biasKnob);
    compRatioAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(COMP_RATIO_ID, focusBandNum), compRatioKnob);
    compThreshAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(COMP_THRESH_ID, focusBandNum), compThreshKnob);
    widthAttachment = std::make_unique<SliderAttachment>(processor.treeState, getParamID(WIDTH_ID, focusBandNum), widthKnob);

    linkedAttachment = std::make_unique<ButtonAttachment>(processor.treeState, getParamID(LINKED_ID, focusBandNum), linkedButton);
    safeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, getParamID(SAFE_ID, focusBandNum), safeButton);
    extremeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, getParamID(EXTREME_ID, focusBandNum), extremeButton);

    compressorBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, getParamID(COMP_BYPASS_ID, focusBandNum), compressorBypassButton);
    widthBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, getParamID(WIDTH_BYPASS_ID, focusBandNum), widthBypassButton);

    // Update the visual state of the bypass buttons to match the newly focused band
    compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
    widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);

    // Apply the correct enabled/disabled states for the new band
    bool bandEnabled = *processor.treeState.getRawParameterValue(getParamID(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(bandEnabled, false);

    // After attaching, configure the modulatable properties for the current focus band.

    for (auto& knobPair : modulatableKnobs)
    {
        // knobPair.first is a pointer to ModulatableSlider
        // knobPair.second is its parameter ID base (e.g. REC_ID)
        configureModulatableSlider(*knobPair.first, knobPair.second);
    }
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
    // bypassButton.addListener(this);
}

void BandPanel::setFocusBandNum(int num, bool forceUpdate)
{
    if (focusBandNum == num && ! forceUpdate)
        return;

    focusBandNum = num;
    updateAttachments();
    updateWhenChangingFocus();

    // After switching, immediately sync the UI with the processor's state for the new band
    bool isBandEnabled = *processor.treeState.getRawParameterValue(getParamID(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(isBandEnabled, false);
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
    if ((&component == &compThreshKnob || &component == &compRatioKnob) && ! compressorBypassButton.getToggleState())
        return true;
    if (&component == &widthKnob && ! widthBypassButton.getToggleState())
        return true;
    return false;
}

// Method for the editor to command a state save
void BandPanel::saveBypassStatesToMemory()
{
    compBypassTemp[focusBandNum] = compressorBypassButton.getToggleState();
    widthBypassTemp[focusBandNum] = widthBypassButton.getToggleState();
}

// The CORE LOGIC, a direct translation of your original function
void BandPanel::setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass)
{
    bool widthBypassState, compBypassState;
    if (! isBandEnabled)
    {
        widthBypassState = false;
        compBypassState = false;
    }
    else
    {
        // Restore state from "memory"
        widthBypassState = widthBypassTemp[focusBandNum];
        compBypassState = compBypassTemp[focusBandNum];
    }

    // Update the visual toggle state of bypass buttons
    if (! callFromSubBypass)
    {
        widthBypassButton.setToggleState(widthBypassState, juce::dontSendNotification);
        compressorBypassButton.setToggleState(compBypassState, juce::dontSendNotification);
    }

    // Enable/disable all controls based on the logic
    if (isBandEnabled)
    {
        for (auto* component : allControls)
        {
            if (canEnableSubKnob(*component))
            {
                component->setEnabled(false); // Explicitly disable if sub-bypass is off
                continue;
            }
            component->setEnabled(true);
        }
    }
    else
    {
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
    buttonClicked(&oscSwitch);
    buttonClicked(&shapeSwitch);
    buttonClicked(&compressorSwitch);
    buttonClicked(&widthSwitch);
    repaint();
}

void BandPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    // When a relevant parameter changes (on any thread), trigger an async update.
    // This is lightweight and thread-safe.
    triggerAsyncUpdate();
}

void BandPanel::handleAsyncUpdate()
{
    // This is called safely on the message thread.
    // We can now update all our linked values.
    updateLinkedValue();
}

juce::String BandPanel::getParamID(const juce::String& base, int bandIndex)
{
    // Parameter IDs are 1-based, so add 1 to the index.
    return base + juce::String(bandIndex + 1);
}
