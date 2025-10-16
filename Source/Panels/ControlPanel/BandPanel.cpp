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
                     std::function<void(ModulatableSlider*)> onModDragStart,
                     std::function<void(ModulatableSlider*)> onModDragMove,
                     std::function<void(ModulatableSlider*)> onModDragEnd,
                     std::function<void(ModulatableSlider*)> onHoverStart,
                     std::function<void(ModulatableSlider*)> onHoverEnd)
    : PanelBase(p), focusBandNum(0)
{
    // Create all UI components using helper methods
    createSliders();
    createLabels();
    createButtons();
    createComboBoxes(); // Create distortion mode dropdowns

    // Assign callbacks to all modulatable sliders in this panel
    for (auto& sliderPair : modulatableSliderComponents)
    {
        auto* slider = sliderPair.second.get();
        slider->onModDragStart = onModDragStart;
        slider->onModDragMove = onModDragMove;
        slider->onModDragEnd = onModDragEnd;
        slider->onHoverStart = onHoverStart;
        slider->onHoverEnd = onHoverEnd;
    }

    // Add graph components and make them visible
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(distortionGraph);
    addAndMakeVisible(vuPanel);
    addAndMakeVisible(widthGraph);

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
        driveBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, i));
        shapeBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, i));
        compBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(COMP_BYPASS_ID, i));
        widthBypassTemp[i] = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, i));
    }

    // Set initial attachments for band 0 and update knob enabled states
    setFocusBandNum(0, true);

    // Set initial visibility
    buttonClicked(&oscSwitch);
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
    driveBypassButton.removeListener(this);
    compressorBypassButton.removeListener(this);
    widthBypassButton.removeListener(this);
    shapeBypassButton.removeListener(this);
    dcFilterButton.removeListener(this);

    for (auto& modeBox : distortionModes)
        modeBox.removeListener(this);
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

    // Add the new drive bypass button
    initBypassButton(driveBypassButton, DRIVE_COLOUR);
    initBypassButton(shapeBypassButton, SHAPE_COLOUR);
    initBypassButton(compressorBypassButton, COMP_COLOUR);
    initBypassButton(widthBypassButton, WIDTH_COLOUR);

    initBypassButton(dcFilterButton, SHAPE_COLOUR);

    auto setupSwitch = [this](juce::TextButton& btn, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(switchButtons);

        btn.setColour(juce::TextButton::buttonColourId, COLOUR8);
        btn.setColour(juce::TextButton::textColourOffId, colour);

        btn.setColour(juce::TextButton::buttonOnColourId, colour.darker().darker());
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

        btn.addListener(this);
    };

    setupSwitch(oscSwitch, "Drive", DRIVE_COLOUR);
    setupSwitch(shapeSwitch, "Shape", SHAPE_COLOUR);
    setupSwitch(compressorSwitch, "Compressor", COMP_COLOUR);
    setupSwitch(widthSwitch, "Stereo", WIDTH_COLOUR);
    oscSwitch.setToggleState(true, juce::dontSendNotification);

    driveBypassButton.toFront(false);
    shapeBypassButton.toFront(false);
    compressorBypassButton.toFront(false);
    widthBypassButton.toFront(false);
    activeTabColour = DRIVE_COLOUR;
}

void BandPanel::createComboBoxes()
{
    for (int i = 0; i < 4; ++i)
    {
        setMenu(&distortionModes[i]);
        modeAttachments[i] = std::make_unique<ComboBoxAttachment>(
            processor.treeState,
            ParameterIDAndName::getIDString(MODE_ID, i),
            distortionModes[i]);
    }
}

void BandPanel::setupComponentGroups()
{
    driveComponents = {
        modulatableSliderComponents.at(DRIVE_NAME).get()
    };

    // Groups for managing VISIBILITY when switching panels
    shapeComponents = {
        modulatableSliderComponents.at(REC_NAME).get(),
        modulatableSliderComponents.at(BIAS_NAME).get(),
        modulatableSliderComponents.at(SHAPE_MIX_NAME).get(),
        &shapePanelLabel,
        &dcFilterButton,
        &dcFilterLabel
    };

    // for (auto& modeBox : distortionModes)
    //     shapeComponents.add(&modeBox);

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
    allControls.addArray(driveComponents);
    allControls.add(modulatableSliderComponents.at(OUTPUT_NAME).get());
    allControls.add(modulatableSliderComponents.at(MIX_NAME).get());
    allControls.add(&linkedButton, &safeButton, &extremeButton);
    allControls.addArray(shapeComponents);
    allControls.addArray(compressorComponents);
    allControls.addArray(widthComponents);
    for (auto& modeBox : distortionModes)
        allControls.add(&modeBox);
}

void BandPanel::paint(juce::Graphics& g)
{
    // Draw borders or backgrounds for the new layout
    g.setColour(COLOUR6);
    g.drawRect(outputAreaRect); // Border for output section
    g.drawRect(tabAreaRect); // Border for the main controls section
    g.drawRect(graphAreaRect); // Border for the graph section

    if (shapeSwitch.getToggleState())
    {
        g.fillRect(shapeSeparatorLine);
    }

    // Draw a themed border for the active tab
    // g.setColour(activeTabColour.withAlpha(0.8f));
    // g.drawRect(tabAreaRect, 2.0f);
}

void BandPanel::resized()
{
    const float scale = this->scale; // Get the scale factor
    const int scaledKnobSize = static_cast<int>(KNOB_SIZE * scale);

    auto mainArea = getLocalBounds().reduced(10 * scale);

    const float switchColumnProportion = 0.15f;
    const float knobsColumnProportion = 0.35f;
    const float graphColumnProportion = 0.3f;

    // Define the four main columns
    auto layoutArea = mainArea;
    auto switchColumnArea = layoutArea.removeFromLeft(mainArea.getWidth() * switchColumnProportion);
    auto knobsColumnArea = layoutArea.removeFromLeft(mainArea.getWidth() * knobsColumnProportion);
    auto graphColumnArea = layoutArea.removeFromLeft(mainArea.getWidth() * graphColumnProportion);
    auto outputColumnArea = layoutArea;

    tabAreaRect = switchColumnArea.getUnion(knobsColumnArea);
    graphAreaRect = graphColumnArea;
    outputAreaRect = outputColumnArea;

    switchColumnArea.removeFromRight(10);
    knobsColumnArea.removeFromRight(10);

    // --- Column 1: Layout Switches (unchanged) ---
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    switchColumnBox.items.add(juce::FlexItem(oscSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(shapeSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(compressorSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(widthSwitch).withFlex(1.0f));
    switchColumnBox.performLayout(switchColumnArea);

    auto layoutBypassButton = [](juce::ToggleButton& bypass, const juce::TextButton& parentSwitch)
    {
        auto parentBounds = parentSwitch.getBounds();
        const int bypassSize = (int) (parentBounds.getHeight() * 0.6f);
        bypass.setBounds(parentBounds.getX(), parentBounds.getCentreY() - (bypassSize / 2), bypassSize, bypassSize);
        bypass.toFront(false);
    };

    layoutBypassButton(driveBypassButton, oscSwitch);
    layoutBypassButton(shapeBypassButton, shapeSwitch);
    layoutBypassButton(compressorBypassButton, compressorSwitch);
    layoutBypassButton(widthBypassButton, widthSwitch);

    // --- Column 2: Manual Layout for Main Knobs Area ---
    if (oscSwitch.getToggleState())
    {
        // Center the single Drive knob
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(knobsColumnArea.withSizeKeepingCentre(scaledKnobSize * 2, scaledKnobSize * 2));
    }
    else if (shapeSwitch.getToggleState())
    {
        // --- Step 1: Center the main row of three knobs vertically ---
        // Create a rectangle for the three knobs and their spacing,
        // and center it within the entire available `knobsColumnArea`.
        juce::Rectangle<int> knobRow = knobsColumnArea.withSizeKeepingCentre(scaledKnobSize * 3 + 20, scaledKnobSize);

        // Now, place the sliders within this perfectly centered `knobRow`.
        // We need to keep a reference to the middle knob's bounds for later.
        auto tempKnobRow = knobRow; // Use a temporary copy for manipulation
        modulatableSliderComponents.at(REC_NAME)->setBounds(tempKnobRow.removeFromLeft(scaledKnobSize));
        tempKnobRow.removeFromLeft(10);
        auto biasKnobBounds = tempKnobRow.removeFromLeft(scaledKnobSize);
        modulatableSliderComponents.at(BIAS_NAME)->setBounds(biasKnobBounds);
        tempKnobRow.removeFromLeft(10);
        modulatableSliderComponents.at(SHAPE_MIX_NAME)->setBounds(tempKnobRow);

        // --- Step 2: Place the other elements around the centered knobs ---

        // Place the distortion mode boxes at the top of the area.
        // We use getFromTop() so it doesn't affect `knobsColumnArea` for centering logic.
        juce::Rectangle<int> distortionModeArea(knobsColumnArea.getX(),
                                                knobsColumnArea.getY(),
                                                knobsColumnArea.getWidth(),
                                                knobsColumnArea.getHeight() / 5);

        auto smallerDistortionModeArea = distortionModeArea.withSizeKeepingCentre(distortionModeArea.getWidth() / 2, distortionModeArea.getHeight());
        for (auto& modeBox : distortionModes)
            modeBox.setBounds(smallerDistortionModeArea.reduced(0, distortionModeArea.getHeight() / 4));

        // Place the DC Filter button below the (now centered) Bias knob.
        // Its position is relative to `biasKnobBounds`, which we saved earlier.
        const int dcButtonSize = (int) (scaledKnobSize * 0.3f);
        const int dcLabelWidth = (int) (35 * scale);
        juce::Rectangle<int> dcArea(0, 0, dcButtonSize + dcLabelWidth, dcButtonSize);
        dcArea.setCentre(biasKnobBounds.getCentreX(), biasKnobBounds.getBottom() + dcButtonSize / 2 + 5);
        dcFilterButton.setBounds(dcArea.removeFromLeft(dcButtonSize));
        dcFilterLabel.setBounds(dcArea);

        // Calculate the separator line's position between the combo box and the knobs.
        const int spaceBetween = knobRow.getY() - distortionModeArea.getBottom();
        const int lineY = distortionModeArea.getBottom() + (spaceBetween / 2);
        const int lineHeight = 1; // Or 2 for a thicker line
        shapeSeparatorLine.setBounds(knobsColumnArea.getX(), lineY, knobsColumnArea.getWidth(), lineHeight);
    }
    else if (compressorSwitch.getToggleState())
    {
        // Place 5 knobs in a 2-row grid
        auto centeredArea = knobsColumnArea.withSizeKeepingCentre(scaledKnobSize * 3 + 20, scaledKnobSize * 2 + 10);
        auto topRow = centeredArea.removeFromTop(scaledKnobSize);
        auto bottomRow = centeredArea.removeFromBottom(scaledKnobSize);

        modulatableSliderComponents.at(COMP_THRESH_NAME)->setBounds(topRow.removeFromLeft(scaledKnobSize));
        modulatableSliderComponents.at(COMP_RATIO_NAME)->setBounds(topRow.removeFromRight(scaledKnobSize));

        modulatableSliderComponents.at(COMP_ATTACK_NAME)->setBounds(bottomRow.removeFromLeft(scaledKnobSize));
        bottomRow.removeFromLeft(10);
        modulatableSliderComponents.at(COMP_RELEASE_NAME)->setBounds(bottomRow.removeFromLeft(scaledKnobSize));
        bottomRow.removeFromLeft(10);
        modulatableSliderComponents.at(COMP_MIX_NAME)->setBounds(bottomRow);
    }
    else if (widthSwitch.getToggleState())
    {
        // Place 3 knobs in a row
        juce::Rectangle<int> knobRow = knobsColumnArea.withSizeKeepingCentre(scaledKnobSize * 3 + 20, scaledKnobSize);
        modulatableSliderComponents.at(WIDTH_NAME)->setBounds(knobRow.removeFromLeft(scaledKnobSize));
        knobRow.removeFromLeft(10);
        modulatableSliderComponents.at(PAN_NAME)->setBounds(knobRow.removeFromLeft(scaledKnobSize));
        knobRow.removeFromLeft(10);
        modulatableSliderComponents.at(WIDTH_MIX_NAME)->setBounds(knobRow);
    }

    // --- Column 3: Layout Graph Area (unchanged) ---
    oscilloscope.setBounds(graphColumnArea);
    distortionGraph.setBounds(graphColumnArea);
    vuPanel.setBounds(graphColumnArea);
    widthGraph.setBounds(graphColumnArea);

    // --- Column 4: CORRECTED Layout for Output Section ---

    // 1. Define an area for the knobs at the top half of the column.
    auto knobsArea = outputColumnArea.removeFromTop(outputColumnArea.getHeight() / 2);

    // 2. Center a rectangle within that area, wide enough for two knobs plus spacing.
    auto twoKnobsBounds = knobsArea.withSizeKeepingCentre(scaledKnobSize * 2 + 10, scaledKnobSize);

    // 3. Place the knobs with fixed size, left and right.
    modulatableSliderComponents.at(OUTPUT_NAME)->setBounds(twoKnobsBounds.removeFromLeft(scaledKnobSize));
    modulatableSliderComponents.at(MIX_NAME)->setBounds(twoKnobsBounds.removeFromRight(scaledKnobSize));

    // 4. Use the remaining bottom half for the buttons with FlexBox.
    auto buttonArea = outputColumnArea; // This is the remaining bottom half
    juce::FlexBox outputButtonsBox;
    outputButtonsBox.flexDirection = juce::FlexBox::Direction::column;
    outputButtonsBox.justifyContent = juce::FlexBox::JustifyContent::flexEnd;
    outputButtonsBox.items.add(juce::FlexItem(linkedButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(safeButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(extremeButton).withFlex(1.0f));

    // Perform layout for buttons in their designated area, with some vertical padding
    outputButtonsBox.performLayout(buttonArea.reduced(0, 0));
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
    driveBypassAttachment.reset();
    shapeBypassAttachment.reset();
    compressorBypassAttachment.reset();
    widthBypassAttachment.reset();
    dcFilterAttachment.reset();

    linkedAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(LINKED_ID, focusBandNum), linkedButton);
    safeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SAFE_ID, focusBandNum), safeButton);
    extremeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(EXTREME_ID, focusBandNum), extremeButton);

    driveBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, focusBandNum), driveBypassButton);
    shapeBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, focusBandNum), shapeBypassButton);
    compressorBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum), compressorBypassButton);
    widthBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, focusBandNum), widthBypassButton);

    dcFilterAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(DC_FILTER_ID, focusBandNum), dcFilterButton);

    // Update visual states from memory
    driveBypassButton.setToggleState(driveBypassTemp[focusBandNum], juce::dontSendNotification);
    shapeBypassButton.setToggleState(shapeBypassTemp[focusBandNum], juce::dontSendNotification);
    compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
    widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);
    dcFilterButton.setToggleState(dcFilterBypassTemp[focusBandNum], juce::dontSendNotification);

    bool bandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(bandEnabled, false);

    // Update attachments for distortion modes
    for (int i = 0; i < 4; ++i)
    {
        modeAttachments[i] = std::make_unique<ComboBoxAttachment>(
            processor.treeState,
            ParameterIDAndName::getIDString(MODE_ID, i),
            distortionModes[i]);
    }
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
    bypassButton.addListener(this);
}

void BandPanel::buttonClicked(juce::Button* clickedButton)
{
    bool isSwitch = false;
    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        activeTabColour = DRIVE_COLOUR;
        setVisibility(driveComponents, true);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);

        oscilloscope.setVisible(true);
        distortionGraph.setVisible(false);
        vuPanel.setVisible(false);
        widthGraph.setVisible(false);
        isSwitch = true;
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        activeTabColour = SHAPE_COLOUR;
        setVisibility(driveComponents, false);
        setVisibility(shapeComponents, true);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, false);

        oscilloscope.setVisible(false);
        distortionGraph.setVisible(true);
        vuPanel.setVisible(false);
        widthGraph.setVisible(false);
        isSwitch = true;
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        activeTabColour = COMP_COLOUR;
        setVisibility(driveComponents, false);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, true);
        setVisibility(widthComponents, false);

        oscilloscope.setVisible(false);
        distortionGraph.setVisible(false);
        vuPanel.setVisible(true);
        widthGraph.setVisible(false);
        isSwitch = true;
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        activeTabColour = WIDTH_COLOUR;
        setVisibility(driveComponents, false);
        setVisibility(shapeComponents, false);
        setVisibility(compressorComponents, false);
        setVisibility(widthComponents, true);

        oscilloscope.setVisible(false);
        distortionGraph.setVisible(false);
        vuPanel.setVisible(false);
        widthGraph.setVisible(true);
        isSwitch = true;
    }

    updateDistortionModeVisibility();

    // Handle clicks from any of the bypass buttons.
    if (clickedButton == &driveBypassButton || clickedButton == &shapeBypassButton || clickedButton == &compressorBypassButton || clickedButton == &widthBypassButton)
    {
        saveBypassStatesToMemory();
        bool isBandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
        setBandKnobsStates(isBandEnabled, true);
        return;
    }

    if (isSwitch)
    {
        resized();
        repaint();
    }
}

void BandPanel::setFocusBandNum(int num, bool forceUpdate)
{
    vuPanel.setFocusBandNum(num);
    if (focusBandNum == num && ! forceUpdate)
        return;

    processor.setUiFocusBand(num);

    focusBandNum = num;
    updateAttachments();
    updateWhenChangingFocus();

    bool isBandEnabled = *processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    setBandKnobsStates(isBandEnabled, false);

    updateDistortionModeVisibility();
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
    driveBypassTemp[focusBandNum] = driveBypassButton.getToggleState();
    shapeBypassTemp[focusBandNum] = shapeBypassButton.getToggleState();
    compBypassTemp[focusBandNum] = compressorBypassButton.getToggleState();
    widthBypassTemp[focusBandNum] = widthBypassButton.getToggleState();
    dcFilterBypassTemp[focusBandNum] = dcFilterButton.getToggleState();
}

void BandPanel::setBandKnobsStates(bool isBandEnabled, bool callFromSubBypass)
{
    for (auto* component : allControls)
    {
        component->setEnabled(isBandEnabled);
    }

    if (isBandEnabled)
    {
        if (! callFromSubBypass)
        {
            driveBypassButton.setToggleState(driveBypassTemp[focusBandNum], juce::dontSendNotification);
            shapeBypassButton.setToggleState(shapeBypassTemp[focusBandNum], juce::dontSendNotification);
            compressorBypassButton.setToggleState(compBypassTemp[focusBandNum], juce::dontSendNotification);
            widthBypassButton.setToggleState(widthBypassTemp[focusBandNum], juce::dontSendNotification);
        }

        bool driveIsEnabled = driveBypassButton.getToggleState();
        for (auto* component : driveComponents)
            component->setEnabled(driveIsEnabled);

        bool shapeIsEnabled = shapeBypassButton.getToggleState();
        for (auto* component : shapeComponents)
            component->setEnabled(shapeIsEnabled);

        bool compIsEnabled = compressorBypassButton.getToggleState();
        for (auto* component : compressorComponents)
            component->setEnabled(compIsEnabled);

        bool widthIsEnabled = widthBypassButton.getToggleState();
        for (auto* component : widthComponents)
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
    updateDistortionModeVisibility();
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

void BandPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    // Logic for combo box changes if any
}

void BandPanel::setMenu(juce::ComboBox* combobox)
{
    addAndMakeVisible(combobox);
    combobox->addSectionHeading("Soft Clipping");
    combobox->addItem("Arctan", 1);
    combobox->addItem("Exp", 2);
    combobox->addItem("Tanh", 3);
    combobox->addItem("Cubic", 4);
    combobox->addSeparator();
    combobox->addSectionHeading("Hard Clipping");
    combobox->addItem("Hard", 5);
    combobox->addItem("Sausage", 6);
    combobox->addSeparator();
    combobox->addSectionHeading("Foldback");
    combobox->addItem("Sin", 7);
    combobox->addItem("Linear", 8);
    combobox->addSeparator();
    combobox->addSectionHeading("Other");
    combobox->addItem("Limit", 9);
    combobox->addItem("Single Sin", 10);
    combobox->addItem("Logic", 11);
    combobox->addItem("Pit", 12);
    combobox->addSeparator();
    combobox->setJustificationType(juce::Justification::centred);
    combobox->addListener(this);
}

void BandPanel::updateDistortionModeVisibility()
{
    const bool shouldShowAny = shapeSwitch.getToggleState();

    for (int i = 0; i < distortionModes.size(); ++i)
    {
        distortionModes[i].setVisible(shouldShowAny && (focusBandNum == i));
    }
}

void BandPanel::updateRealtimeThreshold(float newThreshold)
{
    vuPanel.updateRealtimeThreshold(newThreshold);
}

void BandPanel::setGraphVisibilityForDriveDrag(bool isDragging)
{
    if (isDragging)
    {
        if (oscilloscope.isVisible())
            preDragVisibleGraph = &oscilloscope;
        else if (distortionGraph.isVisible())
            preDragVisibleGraph = &distortionGraph;
        else if (vuPanel.isVisible())
            preDragVisibleGraph = &vuPanel;
        else if (widthGraph.isVisible())
            preDragVisibleGraph = &widthGraph;
        else
            preDragVisibleGraph = nullptr;

        if (preDragVisibleGraph != &distortionGraph)
        {
            if (preDragVisibleGraph != nullptr)
                preDragVisibleGraph->setVisible(false);
            distortionGraph.setVisible(true);
        }
    }
    else
    {
        if (preDragVisibleGraph != nullptr && preDragVisibleGraph != &distortionGraph)
        {
            distortionGraph.setVisible(false);
            preDragVisibleGraph->setVisible(true);
        }
        preDragVisibleGraph = nullptr;
    }
}
