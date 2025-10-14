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

    auto setupSwitch = [this](juce::TextButton& btn, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setClickingTogglesState(true); // Make it behave like a toggle
        btn.setRadioGroupId(switchButtons);

        // Set colors according to the new design
        btn.setColour(juce::TextButton::buttonColourId, COLOUR6); // Standard background
        btn.setColour(juce::TextButton::buttonOnColourId, COLOUR7); // "On" background
        btn.setColour(juce::ComboBox::outlineColourId, COLOUR1.withAlpha(0.0f)); // No outline

        // When ON, text is bright.
        btn.setColour(juce::TextButton::textColourOnId, colour);
        // When OFF, text is a darker version of the main color.
        btn.setColour(juce::TextButton::textColourOffId, colour.darker());
        btn.addListener(this);
    };

    setupSwitch(oscSwitch, "Drive", DRIVE_COLOUR);
    setupSwitch(shapeSwitch, "Shape", SHAPE_COLOUR);
    setupSwitch(compressorSwitch, "Compressor", COMP_COLOUR);
    setupSwitch(widthSwitch, "Stereo", WIDTH_COLOUR);
    oscSwitch.setToggleState(true, juce::dontSendNotification);

    shapeBypassButton.toFront(false);
    compressorBypassButton.toFront(false);
    widthBypassButton.toFront(false);
    activeTabColour = DRIVE_COLOUR;
}

void BandPanel::setupComponentGroups()
{
    // Groups for managing VISIBILITY when switching panels
    shapeComponents = {
        modulatableSliderComponents.at(REC_NAME).get(),
        modulatableSliderComponents.at(BIAS_NAME).get(),
        modulatableSliderComponents.at(SHAPE_MIX_NAME).get(),
        &shapePanelLabel,
        &dcFilterButton,
        &dcFilterLabel
    };

    compressorComponents = {
        modulatableSliderComponents.at(COMP_THRESH_NAME).get(),
        modulatableSliderComponents.at(COMP_RATIO_NAME).get(),
        modulatableSliderComponents.at(COMP_ATTACK_NAME).get(),
        modulatableSliderComponents.at(COMP_RELEASE_NAME).get(),
        modulatableSliderComponents.at(COMP_MIX_NAME).get(),
        &compressorPanelLabel
    };

    widthComponents = {
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
    // 1. Draw the standard border for the output area.
    g.setColour(COLOUR6);
    g.drawRect(outputAreaRect);

    // 2. Draw the special "Tab" border.
    if (oscSwitch.getToggleState())
    {
        // When Drive is selected, it's not a "tab", so just draw a standard
        // border around its content area.
        g.setColour(COLOUR6);
        g.drawRect(knobsAreaRect);
    }
    else
    {
        // When any other switch is selected, draw a themed border around
        // the entire tab area (switches + knobs).
        g.setColour(activeTabColour);
        g.drawRect(tabAreaRect, 1.5f); // Use a slightly thicker line for emphasis.
    }
}

void BandPanel::resized()
{
    // A small margin for the entire panel content.
    auto mainArea = getLocalBounds().reduced(10);

    // 1. Define the three main columns
    auto switchColumnArea = mainArea.removeFromLeft(juce::roundToInt(getWidth() * 0.22f));
    auto outputColumnArea = mainArea.removeFromRight(juce::roundToInt(getWidth() * 0.3f));
    auto knobsColumnArea = mainArea;

    // Store bounds for painting borders later.
    knobsAreaRect = knobsColumnArea;
    outputAreaRect = outputColumnArea;

    // NEW: Calculate the full tab area by combining the switch and knob columns.
    // 新增：通过组合开关列和旋钮列来计算完整的标签页区域。
    tabAreaRect = switchColumnArea.getUnion(knobsColumnArea);

    // Add a margin between the columns.
    switchColumnArea.removeFromRight(10);
    outputColumnArea.removeFromLeft(10);

    // 2. Layout the Switch Column
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    switchColumnBox.items.add(juce::FlexItem(oscSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(shapeSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(compressorSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(widthSwitch).withFlex(1.0f));
    switchColumnBox.performLayout(switchColumnArea);

    // 3. UPDATED LAYOUT: Position bypass buttons ON TOP of the switches
    auto layoutBypassButton = [](juce::ToggleButton& bypass, const juce::TextButton& parentSwitch)
    {
        auto parentBounds = parentSwitch.getBounds();

        // Make the bypass button slightly larger.
        // 将旁通按钮稍微调大。
        const int bypassSize = juce::jmin(24, (int) (parentBounds.getHeight() * 0.6f));
        const int margin = 5;

        // Position it in the left-center of the parent switch.
        // 将其放置在父开关的左侧中央。
        bypass.setBounds(parentBounds.getX() + margin,
                         parentBounds.getCentreY() - (bypassSize / 2),
                         bypassSize,
                         bypassSize);
        bypass.toFront(false);
    };

    layoutBypassButton(shapeBypassButton, shapeSwitch);
    layoutBypassButton(compressorBypassButton, compressorSwitch);
    layoutBypassButton(widthBypassButton, widthSwitch);

    // 4. Layout the Output Column (remains the same)
    juce::FlexBox outputColumnBox;
    outputColumnBox.flexDirection = juce::FlexBox::Direction::column;

    juce::FlexBox outputKnobsBox;
    outputKnobsBox.items.add(juce::FlexItem(*modulatableSliderComponents.at(OUTPUT_NAME)).withFlex(1.0f));
    outputKnobsBox.items.add(juce::FlexItem(*modulatableSliderComponents.at(MIX_NAME)).withFlex(1.0f));
    outputColumnBox.items.add(juce::FlexItem(outputKnobsBox).withFlex(4.0f));

    juce::FlexBox outputButtonsBox;
    outputButtonsBox.items.add(juce::FlexItem(linkedButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(safeButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(extremeButton).withFlex(1.0f));
    outputColumnBox.items.add(juce::FlexItem(outputButtonsBox).withFlex(1.0f));
    outputColumnBox.performLayout(outputColumnArea);

    // 5. Layout the Knobs Column using Grid (remains the same)
    if (oscSwitch.getToggleState())
    {
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(knobsColumnArea);
    }
    else
    {
        juce::Grid knobGrid;
        using Track = juce::Grid::TrackInfo;
        knobGrid.templateColumns = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        knobGrid.templateRows = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        knobGrid.setGap(juce::Grid::Px(10));

        if (shapeSwitch.getToggleState())
        {
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(REC_NAME).get()));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(BIAS_NAME).get()));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(SHAPE_MIX_NAME).get()));
            knobGrid.performLayout(knobsColumnArea);

            auto middleKnobBounds = modulatableSliderComponents.at(BIAS_NAME)->getBounds();
            const int dcButtonSize = (int) (middleKnobBounds.getHeight() * 0.3f);
            const int dcLabelWidth = 35;
            const int totalWidth = dcButtonSize + dcLabelWidth;
            juce::Rectangle<int> dcArea;
            dcArea.setSize(totalWidth, dcButtonSize);
            dcArea.setCentre(middleKnobBounds.getCentreX(), middleKnobBounds.getBottom() - (int) (dcButtonSize * 0.5f));
            dcFilterButton.setBounds(dcArea.removeFromLeft(dcButtonSize));
            dcFilterLabel.setBounds(dcArea);
        }
        else if (compressorSwitch.getToggleState())
        {
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_THRESH_NAME).get()).withArea(1, 1));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_RATIO_NAME).get()).withArea(1, 3));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_ATTACK_NAME).get()).withArea(2, 1));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_RELEASE_NAME).get()).withArea(2, 2));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_MIX_NAME).get()).withArea(2, 3));
            knobGrid.performLayout(knobsColumnArea);
        }
        else if (widthSwitch.getToggleState())
        {
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(WIDTH_NAME).get()));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(PAN_NAME).get()));
            knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(WIDTH_MIX_NAME).get()));
            knobGrid.performLayout(knobsColumnArea);
        }
    }
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

    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        activeTabColour = DRIVE_COLOUR;
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        activeTabColour = SHAPE_COLOUR;
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        activeTabColour = COMP_COLOUR;
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        activeTabColour = WIDTH_COLOUR;
    }

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
    if (shapeComponents.contains(&component) && shapeBypassButton.getToggleState())
        return true;

    if (compressorComponents.contains(&component) && compressorBypassButton.getToggleState())
        return true;

    if (widthComponents.contains(&component) && widthBypassButton.getToggleState())
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
    for (auto* component : shapeComponents)
    {
        component->setEnabled(shapeIsEnabled);
    }

    // 4. Compressor sub-controls depend on the compressor bypass button state.
    bool compIsEnabled = compressorBypassButton.getToggleState();
    for (auto* component : compressorComponents)
    {
        component->setEnabled(compIsEnabled);
    }

    // 5. Width sub-controls depend on the width bypass button state.
    bool widthIsEnabled = widthBypassButton.getToggleState();
    for (auto* component : widthComponents)
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
