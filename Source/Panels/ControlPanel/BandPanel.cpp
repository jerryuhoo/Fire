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
    createComboBoxes(); // Create distortion mode dropdowns

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

    // Draw a themed border for the active tab
    g.setColour(activeTabColour);
    g.drawRect(tabAreaRect, 1.5f);
}

void BandPanel::resized()
{
    auto mainArea = getLocalBounds().reduced(10);

    // Define the four main columns for the new layout.
    auto switchColumnArea = mainArea.removeFromLeft(mainArea.getWidth() * 0.15f);
    auto knobsColumnArea = mainArea.removeFromLeft(mainArea.getWidth() * 0.40f); // Main controls are wider
    auto graphColumnArea = mainArea.removeFromLeft(mainArea.getWidth() * 0.55f); // Graph takes the next chunk
    auto outputColumnArea = mainArea; // Output takes the remaining space on the right

    // Store the bounds for painting borders later if needed.
    tabAreaRect = switchColumnArea.getUnion(knobsColumnArea); // The "tab" is the switches + knobs
    graphAreaRect = graphColumnArea;
    outputAreaRect = outputColumnArea;

    // Add some margin between the columns for visual separation.
    switchColumnArea.removeFromRight(10);
    knobsColumnArea.removeFromRight(10);
    // graphColumnArea.removeFromRight(10); // REMOVED THIS LINE TO FIX THE GAP

    // --- Column 1: Layout Switches ---
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // Use spaceAround for better vertical spacing
    switchColumnBox.items.add(juce::FlexItem(oscSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(shapeSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(compressorSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(widthSwitch).withFlex(1.0f));
    switchColumnBox.performLayout(switchColumnArea);

    // Layout the bypass buttons on top of their parent switches.
    auto layoutBypassButton = [](juce::ToggleButton& bypass, const juce::TextButton& parentSwitch)
    {
        auto parentBounds = parentSwitch.getBounds();
        const int bypassSize = juce::jmin(24, (int) (parentBounds.getHeight() * 0.6f));
        const int margin = 5;
        bypass.setBounds(parentBounds.getX() + margin, parentBounds.getCentreY() - (bypassSize / 2), bypassSize, bypassSize);
        bypass.toFront(false);
    };

    layoutBypassButton(driveBypassButton, oscSwitch);
    layoutBypassButton(shapeBypassButton, shapeSwitch);
    layoutBypassButton(compressorBypassButton, compressorSwitch);
    layoutBypassButton(widthBypassButton, widthSwitch);

    // --- Column 2: Layout the Main Knobs Area ---
    if (oscSwitch.getToggleState())
    {
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(knobsColumnArea);
    }
    else if (shapeSwitch.getToggleState())
    {
        auto distortionModeArea = knobsColumnArea.removeFromTop(knobsColumnArea.getHeight() / 5);
        for (auto& modeBox : distortionModes)
            modeBox.setBounds(distortionModeArea.reduced(0, distortionModeArea.getHeight() / 4));

        juce::Grid knobGrid;
        knobGrid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.templateRows = { juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.setGap(juce::Grid::Px(10));
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
        juce::Grid knobGrid;
        knobGrid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.templateRows = { juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.setGap(juce::Grid::Px(10));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_THRESH_NAME).get()).withArea(1, 1));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_RATIO_NAME).get()).withArea(1, 3));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_ATTACK_NAME).get()).withArea(2, 1));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_RELEASE_NAME).get()).withArea(2, 2));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(COMP_MIX_NAME).get()).withArea(2, 3));
        knobGrid.performLayout(knobsColumnArea);
    }
    else if (widthSwitch.getToggleState())
    {
        juce::Grid knobGrid;
        knobGrid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)), juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.templateRows = { juce::Grid::TrackInfo(juce::Grid::Fr(1)) };
        knobGrid.setGap(juce::Grid::Px(10));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(WIDTH_NAME).get()));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(PAN_NAME).get()));
        knobGrid.items.add(juce::GridItem(modulatableSliderComponents.at(WIDTH_MIX_NAME).get()));
        knobGrid.performLayout(knobsColumnArea);
    }

    // --- Column 3: Layout Graph Area ---
    // Set bounds for all graphs; only one will be visible at a time.
    oscilloscope.setBounds(graphColumnArea);
    distortionGraph.setBounds(graphColumnArea);
    vuPanel.setBounds(graphColumnArea);
    widthGraph.setBounds(graphColumnArea);

    // --- Column 4: Layout Output Section ---
    juce::FlexBox outputColumnBox;
    outputColumnBox.flexDirection = juce::FlexBox::Direction::column;

    // The knobs at the top of this column
    juce::FlexBox outputKnobsBox;
    outputKnobsBox.items.add(juce::FlexItem(*modulatableSliderComponents.at(OUTPUT_NAME)).withFlex(1.0f));
    outputKnobsBox.items.add(juce::FlexItem(*modulatableSliderComponents.at(MIX_NAME)).withFlex(1.0f));
    outputColumnBox.items.add(juce::FlexItem(outputKnobsBox).withFlex(3.0f)); // Give knobs more space

    // The buttons at the bottom of this column
    juce::FlexBox outputButtonsBox;
    outputButtonsBox.flexDirection = juce::FlexBox::Direction::column; // Stack them vertically
    outputButtonsBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    outputButtonsBox.items.add(juce::FlexItem(linkedButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(safeButton).withFlex(1.0f));
    outputButtonsBox.items.add(juce::FlexItem(extremeButton).withFlex(1.0f));
    outputColumnBox.items.add(juce::FlexItem(outputButtonsBox).withFlex(2.0f).withMargin(juce::FlexItem::Margin(10, 0, 0, 0))); // Add top margin

    outputColumnBox.performLayout(outputColumnArea);
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