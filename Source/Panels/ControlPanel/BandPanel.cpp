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
BandPanel::BandPanel(FireAudioProcessor& p,
                     std::function<void(ModulatableSlider*)> onDragStart,
                     std::function<void(ModulatableSlider*)> onDragMove,
                     std::function<void(ModulatableSlider*)> onDragEnd,
                     std::function<void(ModulatableSlider*)> onHoverStart,
                     std::function<void(ModulatableSlider*)> onHoverEnd)
    : PanelBase(p), focusBandNum(0)
{
    // Create all UI components using helper methods
    createSliders();
    createLabels();
    createButtons();

    // Assign callbacks to all modulatable sliders in this panel
    for (auto& sliderPair : modulatableSliderComponents)
    {
        auto* slider = sliderPair.second.get();
        slider->onDragStart = onDragStart;
        slider->onDragMove = onDragMove;
        slider->onDragEnd = onDragEnd;
        slider->onHoverStart = onHoverStart;
        slider->onHoverEnd = onHoverEnd;
    }

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

    // Remove listeners that were added in the setupSwitch lambda
    oscSwitch.removeListener(this);
    shapeSwitch.removeListener(this);
    compressorSwitch.removeListener(this);
    widthSwitch.removeListener(this);

    // Also remove listeners from any other buttons if they were added in their init functions.
    // Assuming initFlatButton and initBypassButton also add 'this' as a listener.
    linkedButton.removeListener(this);
    safeButton.removeListener(this);
    extremeButton.removeListener(this);
    compressorBypassButton.removeListener(this);
    widthBypassButton.removeListener(this);
}

void BandPanel::createSliders()
{
    // [MODIFIED] This function is now much simpler.
    // It just calls the helper from the base class for each slider.

    // Main Panel
    createAndConfigureSlider(DRIVE_NAME, "Drive", DRIVE_COLOUR);
    createAndConfigureSlider(OUTPUT_NAME, "Output", COLOUR1, " dB");
    createAndConfigureSlider(MIX_NAME, "Mix", COLOUR1);

    // Shape Panel
    createAndConfigureSlider(REC_NAME, "Rectification", SHAPE_COLOUR);
    createAndConfigureSlider(BIAS_NAME, "Bias", SHAPE_COLOUR);
    createAndConfigureSlider(SHAPE_MIX_NAME, "Mix", SHAPE_COLOUR);

    // Compressor Panel
    createAndConfigureSlider(COMP_THRESH_NAME, "Threshold", COMP_COLOUR, " dB");
    createAndConfigureSlider(COMP_RATIO_NAME, "Ratio", COMP_COLOUR);
    createAndConfigureSlider(COMP_ATTACK_NAME, "Attack", COMP_COLOUR, " ms");
    createAndConfigureSlider(COMP_RELEASE_NAME, "Release", COMP_COLOUR, " ms");
    createAndConfigureSlider(COMP_MIX_NAME, "Mix", COMP_COLOUR);

    // Width Panel
    createAndConfigureSlider(WIDTH_NAME, "Width", WIDTH_COLOUR);
    createAndConfigureSlider(PAN_NAME, "Pan", WIDTH_COLOUR);
    createAndConfigureSlider(WIDTH_MIX_NAME, "Mix", WIDTH_COLOUR);

    // === Final specific configurations ===
    // Assign special component ID for the custom LookAndFeel.
    modulatableSliderComponents.at(DRIVE_NAME)->setComponentID("drive");
}

void BandPanel::createLabels()
{
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
    initFlatButton(linkedButton, "Link");
    initFlatButton(safeButton, "Safe");
    initFlatButton(extremeButton, "Extreme");
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
    // Groups for managing VISIBILITY when switching panels
    shapeComponents = {
        modulatableSliderComponents.at(REC_NAME).get(),
        modulatableSliderComponents.at(BIAS_NAME).get(),
        modulatableSliderComponents.at(SHAPE_MIX_NAME).get(),
        &shapePanelLabel,
    };

    compressorComponents = {
        modulatableSliderComponents.at(COMP_THRESH_NAME).get(),
        modulatableSliderComponents.at(COMP_RATIO_NAME).get(),
        modulatableSliderComponents.at(COMP_ATTACK_NAME).get(),
        modulatableSliderComponents.at(COMP_RELEASE_NAME).get(),
        modulatableSliderComponents.at(COMP_MIX_NAME).get(),
        &compressorBypassButton,
        &compressorPanelLabel,
    };

    widthComponents = {
        modulatableSliderComponents.at(WIDTH_NAME).get(),
        modulatableSliderComponents.at(PAN_NAME).get(),
        modulatableSliderComponents.at(WIDTH_MIX_NAME).get(),
        &widthBypassButton,
        &widthPanelLabel,
    };

    // Groups for managing ENABLED/DISABLED state logic
    compressorSubControls = {
        modulatableSliderComponents.at(COMP_THRESH_NAME).get(),
        modulatableSliderComponents.at(COMP_RATIO_NAME).get(),
        modulatableSliderComponents.at(COMP_ATTACK_NAME).get(),
        modulatableSliderComponents.at(COMP_RELEASE_NAME).get(),
        modulatableSliderComponents.at(COMP_MIX_NAME).get(),
        &compressorPanelLabel
    };

    widthSubControls = {
        modulatableSliderComponents.at(WIDTH_NAME).get(),
        modulatableSliderComponents.at(PAN_NAME).get(),
        modulatableSliderComponents.at(WIDTH_MIX_NAME).get(),
        &widthPanelLabel
    };

    // A single master list of all components for disabling the entire band
    allControls.add(modulatableSliderComponents.at(DRIVE_NAME).get());
    allControls.add(modulatableSliderComponents.at(OUTPUT_NAME).get());
    allControls.add(modulatableSliderComponents.at(MIX_NAME).get());
    allControls.add(&linkedButton, &safeButton, &extremeButton);
    allControls.addArray(shapeComponents);
    allControls.addArray(compressorComponents);
    allControls.addArray(widthComponents);

    // Set initial visibility
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

    // Conditionally define the layout areas based on which switch is active.
    if (! oscSwitch.getToggleState())
    {
        // If Shape, Comp, or Width is active:
        // The main knob area takes up the full left 3/5ths of the panel.
        bandKnobArea = controlArea.removeFromLeft(getWidth() / 5 * 3);
        driveKnobArea = {}; // Drive area is not used, so we create an empty rectangle.
    }
    else
    {
        // If OSC (Drive) is active, use the original layout definition.
        bandKnobArea = controlArea.removeFromLeft(getWidth() / 5 * 2);
        driveKnobArea = controlArea.removeFromLeft(getWidth() / 5);
    }

    // The rest of the area is for the Output section, this remains unchanged.
    outputKnobArea = controlArea;
    const int verticalMargin = getHeight() / 6;

    outputKnobArea.reduce(0, verticalMargin);
    driveKnobArea.reduce(0, verticalMargin); // Reduces an empty rect, which is fine.
    bandKnobArea.reduce(0, verticalMargin);

    juce::Rectangle<int> switchArea = bandKnobArea.removeFromLeft(getWidth() / 50);
    const float switchButtonHeight = switchArea.getHeight() / 4.0f;
    juce::Rectangle<int> area = switchArea;

    oscSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    shapeSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    compressorSwitch.setBounds(area.removeFromTop(juce::roundToInt(switchButtonHeight)));
    widthSwitch.setBounds(area);

    // Set bounds only for the big Drive knob when OSC is active.
    if (oscSwitch.getToggleState())
    {
        juce::Rectangle<int> bigDriveArea = getLocalBounds().removeFromLeft(getWidth() / 5 * 3).reduced(getHeight() / 10);
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(bigDriveArea);
    }

    // === MODIFIED BLOCK START ===
    // --- Band Knob Area Layout ---
    // This section is now replaced with a flexible grid layout to accommodate all new knobs.
    juce::Rectangle<int> subKnobArea = bandKnobArea.reduced(0, bandKnobArea.getHeight() / 5);

    // --- Shape Panel Layout (3 knobs in 1 row) ---
    auto shapeArea = subKnobArea;
    int shapeKnobWidth = shapeArea.getWidth() / 3;
    modulatableSliderComponents.at(REC_NAME)->setBounds(shapeArea.removeFromLeft(shapeKnobWidth));
    modulatableSliderComponents.at(BIAS_NAME)->setBounds(shapeArea.removeFromLeft(shapeKnobWidth));
    modulatableSliderComponents.at(SHAPE_MIX_NAME)->setBounds(shapeArea);

    // --- Compressor Panel Layout (5 knobs in 2 rows) ---
    auto compArea = subKnobArea;
    auto compTopRow = compArea.removeFromTop(compArea.getHeight() / 2);
    auto compBottomRow = compArea;
    int compTopKnobWidth = compTopRow.getWidth() / 3;
    // Center the 2 knobs on the bottom row to align with the 3 on top
    auto centeredBottomRow = compBottomRow.withSizeKeepingCentre(compTopKnobWidth * 2, compBottomRow.getHeight());
    modulatableSliderComponents.at(COMP_THRESH_NAME)->setBounds(compTopRow.removeFromLeft(compTopKnobWidth));
    modulatableSliderComponents.at(COMP_RATIO_NAME)->setBounds(compTopRow.removeFromLeft(compTopKnobWidth));
    modulatableSliderComponents.at(COMP_ATTACK_NAME)->setBounds(compTopRow);
    modulatableSliderComponents.at(COMP_RELEASE_NAME)->setBounds(centeredBottomRow.removeFromLeft(compTopKnobWidth));
    modulatableSliderComponents.at(COMP_MIX_NAME)->setBounds(centeredBottomRow);

    // --- Width Panel Layout (3 knobs in 1 row) ---
    auto widthArea = subKnobArea;
    int widthKnobWidth = widthArea.getWidth() / 3;
    modulatableSliderComponents.at(WIDTH_NAME)->setBounds(widthArea.removeFromLeft(widthKnobWidth));
    modulatableSliderComponents.at(PAN_NAME)->setBounds(widthArea.removeFromLeft(widthKnobWidth));
    modulatableSliderComponents.at(WIDTH_MIX_NAME)->setBounds(widthArea);
    // === MODIFIED BLOCK END ===

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

    // --- Output Area Layout --- (This part remains unchanged and correct)
    const int bottomAreaHeight = outputKnobArea.getHeight() / 5;

    juce::Rectangle<int> outputBottomArea(outputKnobArea.getX(),
                                          outputKnobArea.getBottom() - bottomAreaHeight,
                                          outputKnobArea.getWidth(),
                                          bottomAreaHeight);

    juce::Rectangle<int> tempButtonArea = outputBottomArea;
    const int buttonWidth = tempButtonArea.getWidth() / 3;
    linkedButton.setBounds(tempButtonArea.removeFromLeft(buttonWidth));
    safeButton.setBounds(tempButtonArea.removeFromLeft(buttonWidth));
    extremeButton.setBounds(tempButtonArea);

    juce::Rectangle<int> outputSubArea = outputKnobArea.reduced(0, bottomAreaHeight);
    juce::Rectangle<int> outputLeftArea = outputSubArea.withRight(outputSubArea.getCentreX());
    juce::Rectangle<int> outputRightArea = outputSubArea.withLeft(outputSubArea.getCentreX());

    modulatableSliderComponents.at(OUTPUT_NAME)->setBounds(outputLeftArea);
    modulatableSliderComponents.at(MIX_NAME)->setBounds(outputRightArea);
}

void BandPanel::updateAttachments()
{
    // Loop through our new definitive list of parameter info
    for (const auto& paramInfo : ParameterIDAndName::getModulatableParameterInfo())
    {
        // Get the slider using its stable Name (e.g., "CompRatio")
        auto* slider = modulatableSliderComponents.at(paramInfo.name).get();

        // Generate the dynamic parameter ID using the correct idBase (e.g., "compRatio")
        auto paramID = ParameterIDAndName::getIDString(paramInfo.idBase, focusBandNum);

        slider->parameterID = paramID;

        sliderAttachments[paramInfo.name].reset();

        auto* parameter = processor.treeState.getParameter(paramID);
        jassert(parameter != nullptr && "Parameter not found! Check idBase in getModulatableParameterInfo and parameter creation in PluginProcessor.");

        if (parameter)
        {
            sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
        }
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

    processor.setUiFocusBand(num);

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
        float newOutputValue = -modulatableSliderComponents.at(DRIVE_NAME)->getValue() * 0.1f;
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
    // Get pointers to the Drive knob and its label
    auto* driveComponent = modulatableSliderComponents.at(DRIVE_NAME).get();
    // auto* driveLabel = labels.at(DRIVE_NAME).get();

    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        // If the OSC switch is activated, show the Drive components.
        driveComponent->setVisible(true);
        // driveLabel->setVisible(true);

        // Hide all other component groups.
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        // If any other switch is activated, hide the Drive components.
        driveComponent->setVisible(false);
        // driveLabel->setVisible(false);

        // Show only the Shape components.
        setVisibility(shapeComponents, true);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        // Hide the Drive components.
        driveComponent->setVisible(false);
        // driveLabel->setVisible(false);

        // Show only the Compressor components.
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, true);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        // Hide the Drive components.
        driveComponent->setVisible(false);
        // driveLabel->setVisible(false);

        // Show only the Width components.
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, true);
    }

    // Call resized() and repaint() to apply layout and visibility changes.
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
    // A sub-knob can be enabled if its corresponding bypass button is on.
    if (compressorSubControls.contains(&component) && compressorBypassButton.getToggleState())
        return true;

    if (widthSubControls.contains(&component) && widthBypassButton.getToggleState())
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
    if (! isBandEnabled)
    {
        // If the entire band is disabled, just disable every single control and we're done.
        for (auto* component : allControls)
        {
            component->setEnabled(false);
        }
        return;
    }

    // --- If the band IS enabled ---

    // 1. Set the state of bypass buttons first (unless the call came from one of them).
    // The state depends on our memory, as the actual parameter might be for another band.
    if (! callFromSubBypass)
    {
        compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
        widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);
    }

    // 2. Main controls (Drive, Output, etc.) and the bypass buttons themselves are always enabled.
    modulatableSliderComponents.at(DRIVE_NAME)->setEnabled(true);
    modulatableSliderComponents.at(OUTPUT_NAME)->setEnabled(true);
    modulatableSliderComponents.at(MIX_NAME)->setEnabled(true);
    linkedButton.setEnabled(true);
    safeButton.setEnabled(true);
    extremeButton.setEnabled(true);
    compressorBypassButton.setEnabled(true);
    widthBypassButton.setEnabled(true);

    // 3. Shape panel controls are always enabled when the band is on.
    for (auto* component : shapeComponents)
    {
        component->setEnabled(true);
    }

    // 4. Compressor sub-controls depend on the compressor bypass button state.
    bool compIsEnabled = compressorBypassButton.getToggleState();
    for (auto* component : compressorSubControls)
    {
        component->setEnabled(compIsEnabled);
    }

    // 5. Width sub-controls depend on the width bypass button state.
    bool widthIsEnabled = widthBypassButton.getToggleState();
    for (auto* component : widthSubControls)
    {
        component->setEnabled(widthIsEnabled);
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