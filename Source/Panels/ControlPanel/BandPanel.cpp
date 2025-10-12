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
        shapeBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, i));
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
    shapeBypassButton.removeListener(this);
    dcFilterButton.removeListener(this);
}

void BandPanel::createSliders()
{
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
    setupPanelLabel(dcFilterLabel, "DC", SHAPE_COLOUR.withBrightness(0.8f));
}

void BandPanel::createButtons()
{
    initFlatButton(linkedButton, "Link");
    initFlatButton(safeButton, "Safe");
    initFlatButton(extremeButton, "Extreme");

    initBypassButton(shapeBypassButton, SHAPE_COLOUR);
    initBypassButton(compressorBypassButton, COMP_COLOUR);
    initBypassButton(widthBypassButton, WIDTH_COLOUR);

    initBypassButton(dcFilterButton, SHAPE_COLOUR);

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
        &shapeBypassButton,
        &dcFilterButton,
        &dcFilterLabel
    };

    compressorComponents = {
        modulatableSliderComponents.at(COMP_THRESH_NAME).get(),
        modulatableSliderComponents.at(COMP_RATIO_NAME).get(),
        modulatableSliderComponents.at(COMP_ATTACK_NAME).get(),
        modulatableSliderComponents.at(COMP_RELEASE_NAME).get(),
        modulatableSliderComponents.at(COMP_MIX_NAME).get(),
        &compressorBypassButton,
        &compressorPanelLabel
    };

    widthComponents = {
        modulatableSliderComponents.at(WIDTH_NAME).get(),
        modulatableSliderComponents.at(PAN_NAME).get(),
        modulatableSliderComponents.at(WIDTH_MIX_NAME).get(),
        &widthBypassButton,
        &widthPanelLabel
    };

    // Groups for managing ENABLED/DISABLED state logic
    shapeSubControls = {
        modulatableSliderComponents.at(REC_NAME).get(),
        modulatableSliderComponents.at(BIAS_NAME).get(),
        modulatableSliderComponents.at(SHAPE_MIX_NAME).get(),
        &shapePanelLabel,
        &dcFilterButton,
        &dcFilterLabel
    };

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

    // This is the main area for the 3 panels (Shape, Comp, Width)
    juce::Rectangle<int> subKnobArea = bandKnobArea;

    // 1. Calculate a single, unified knob size based on the Compressor panel's layout
    //    (which is the most dense with 3 columns and 2 rows).
    const int fixedKnobWidth = subKnobArea.getWidth() / 3;
    const int fixedKnobHeight = subKnobArea.getHeight() / 2;

    // --- Shape Panel Layout (3 knobs + 1 button) ---
    auto shapeContentArea = subKnobArea;
    auto shapeKnobArea = shapeContentArea.removeFromTop(shapeContentArea.getHeight() * 0.8);
    auto dcFilterArea = shapeContentArea;

    auto shapeRowArea = shapeKnobArea.withSizeKeepingCentre(shapeKnobArea.getWidth(), fixedKnobHeight);
    modulatableSliderComponents.at(REC_NAME)->setBounds(shapeRowArea.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(BIAS_NAME)->setBounds(shapeRowArea.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(SHAPE_MIX_NAME)->setBounds(shapeRowArea);

    // 1. Define the size of the button and label.
    const int dcButtonSize = dcFilterArea.getHeight() * 0.7; // Make the button a bit smaller than the row height
    const int dcLabelWidth = 35;
    const int dcGroupWidth = dcButtonSize + dcLabelWidth;

    // 2. Create a single area for the button-label group and center it horizontally
    //    under the middle knob.
    auto middleColumnX = subKnobArea.getX() + fixedKnobWidth;
    juce::Rectangle<int> dcGroupArea;
    dcGroupArea.setSize(dcGroupWidth, dcFilterArea.getHeight());
    dcGroupArea.setCentre(middleColumnX + (fixedKnobWidth / 2), dcFilterArea.getCentreY());
    dcGroupArea.translate(0, -30);

    // 3. Place the button on the left of the group area, and the label on the right.
    dcFilterButton.setBounds(dcGroupArea.removeFromLeft(dcButtonSize).withSizeKeepingCentre(dcButtonSize, dcButtonSize));
    dcFilterLabel.setBounds(dcGroupArea);
    dcFilterLabel.setJustificationType(juce::Justification::centredLeft); // Align text to the left

    // --- Compressor Panel Layout (2 knobs on top, 3 on bottom) ---
    auto compArea = subKnobArea;
    auto compTopRow = compArea.removeFromTop(fixedKnobHeight);
    auto compBottomRow = compArea;

    // Top Row: 2 knobs. Create a centered area for them.
    auto centeredTopRow = compTopRow.withSizeKeepingCentre(2 * fixedKnobWidth, fixedKnobHeight);
    modulatableSliderComponents.at(COMP_THRESH_NAME)->setBounds(centeredTopRow.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(COMP_RATIO_NAME)->setBounds(centeredTopRow);

    // Bottom Row: 3 knobs. They will fill the full width of their row.
    modulatableSliderComponents.at(COMP_ATTACK_NAME)->setBounds(compBottomRow.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(COMP_RELEASE_NAME)->setBounds(compBottomRow.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(COMP_MIX_NAME)->setBounds(compBottomRow);

    // --- Width Panel Layout (3 knobs in 1 row) ---
    // Create a single row area with the fixed height, and center it vertically.
    auto widthRowArea = subKnobArea.withSizeKeepingCentre(subKnobArea.getWidth(), fixedKnobHeight);
    modulatableSliderComponents.at(WIDTH_NAME)->setBounds(widthRowArea.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(PAN_NAME)->setBounds(widthRowArea.removeFromLeft(fixedKnobWidth));
    modulatableSliderComponents.at(WIDTH_MIX_NAME)->setBounds(widthRowArea);

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

    shapeBypassButton.setBounds(bypassButtonArea);
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
    for (const auto& paramInfo : ParameterIDAndName::getModulatableParameterInfo())
    {
        auto* slider = modulatableSliderComponents.at(paramInfo.name).get();
        auto paramID = ParameterIDAndName::getIDString(paramInfo.idBase, focusBandNum);
        slider->parameterID = paramID;
        sliderAttachments[paramInfo.name].reset();
        auto* parameter = processor.treeState.getParameter(paramID);
        jassert(parameter != nullptr && "Parameter not found!");
        if (parameter)
        {
            sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
        }
    }

    // === Button Attachment Logic ===
    linkedAttachment.reset();
    safeAttachment.reset();
    extremeAttachment.reset();
    shapeBypassAttachment.reset();
    compressorBypassAttachment.reset();
    widthBypassAttachment.reset();
    dcFilterAttachment.reset();

    linkedAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(LINKED_ID, focusBandNum), linkedButton);
    safeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SAFE_ID, focusBandNum), safeButton);
    extremeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(EXTREME_ID, focusBandNum), extremeButton);

    shapeBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, focusBandNum), shapeBypassButton);
    compressorBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum), compressorBypassButton);
    widthBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, focusBandNum), widthBypassButton);

    dcFilterAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(DC_FILTER_ID, focusBandNum), dcFilterButton);

    // Update visual states from memory
    shapeBypassButton.setToggleState(shapeBypassTemp[focusBandNum], juce::dontSendNotification);
    compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
    widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);
    dcFilterButton.setToggleState(dcFilterBypassTemp[focusBandNum], juce::dontSendNotification);

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

    // Add the panel as a listener to the button.
    // This is the crucial step to make sure we can react to clicks.
    bypassButton.addListener(this);
}

void BandPanel::buttonClicked(juce::Button* clickedButton)
{
    auto* driveComponent = modulatableSliderComponents.at(DRIVE_NAME).get();

    // --- Panel Visibility Switches ---
    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        driveComponent->setVisible(true);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        driveComponent->setVisible(false);
        setVisibility(shapeComponents, true);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        driveComponent->setVisible(false);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, true);
        setVisibility(widthComponents, false);
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        driveComponent->setVisible(false);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, true);
    }

    // Handle clicks from any of the bypass buttons.
    else if (clickedButton == &shapeBypassButton || clickedButton == &compressorBypassButton || clickedButton == &widthBypassButton)
    {
        // When a bypass button is clicked, we need to do two things:
        // 1. Save its new state to our temporary memory for the current band.
        saveBypassStatesToMemory();

        // 2. Re-run the logic to enable/disable all relevant knobs.
        // We get the master enable state for the whole band first.
        bool isBandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));

        // Then call our state function, passing 'true' to indicate the call
        // is from a sub-bypass button itself, preventing it from being reset.
        setBandKnobsStates(isBandEnabled, true);

        // Return early to avoid the unnecessary resized/repaint for these clicks.
        return;
    }

    // Only call resized/repaint if a panel switch was clicked
    resized();
    repaint();
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

void BandPanel::setVisibility(juce::Array<juce::Component*>& components, bool isVisible)
{
    for (auto* component : components)
    {
        component->setVisible(isVisible);
    }
}

bool BandPanel::canEnableSubKnob(juce::Component& component)
{
    if (shapeSubControls.contains(&component) && shapeBypassButton.getToggleState())
        return true;

    if (compressorSubControls.contains(&component) && compressorBypassButton.getToggleState())
        return true;

    if (widthSubControls.contains(&component) && widthBypassButton.getToggleState())
        return true;

    return false;
}

void BandPanel::saveBypassStatesToMemory()
{
    shapeBypassTemp[focusBandNum] = shapeBypassButton.getToggleState();
    compBypassTemp[focusBandNum] = compressorBypassButton.getToggleState();
    widthBypassTemp[focusBandNum] = widthBypassButton.getToggleState();
    dcFilterBypassTemp[focusBandNum] = dcFilterButton.getToggleState();
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
    if (! callFromSubBypass)
    {
        shapeBypassButton.setToggleState(shapeBypassTemp[focusBandNum], juce::dontSendNotification);
        compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
        widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);
    }

    // 2. Main controls and all bypass buttons themselves are always enabled.
    modulatableSliderComponents.at(DRIVE_NAME)->setEnabled(true);
    modulatableSliderComponents.at(OUTPUT_NAME)->setEnabled(true);
    modulatableSliderComponents.at(MIX_NAME)->setEnabled(true);
    linkedButton.setEnabled(true);
    safeButton.setEnabled(true);
    extremeButton.setEnabled(true);
    shapeBypassButton.setEnabled(true);
    compressorBypassButton.setEnabled(true);
    widthBypassButton.setEnabled(true);

    // 3. Shape sub-controls depend on the shape bypass button state.
    bool shapeIsEnabled = shapeBypassButton.getToggleState();
    for (auto* component : shapeSubControls)
    {
        component->setEnabled(shapeIsEnabled);
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
