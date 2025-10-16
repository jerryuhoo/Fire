/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GlobalPanel.h"
#include "../../Utility/AudioHelpers.h"

//==============================================================================
GlobalPanel::GlobalPanel(FireAudioProcessor& p,
                         std::function<void(ModulatableSlider*)> onModDragStart,
                         std::function<void(ModulatableSlider*)> onModDragMove,
                         std::function<void(ModulatableSlider*)> onModDragEnd,
                         std::function<void(ModulatableSlider*)> onHoverStart,
                         std::function<void(ModulatableSlider*)> onHoverEnd)
    : PanelBase(p)
{
    createSliders();
    createLabels();
    createButtons();
    createComboBoxes();

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(vuPanel);
    addAndMakeVisible(widthGraph);

    vuPanel.setFocusBandNum(-1);

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

    setupComponentGroups();

    updateAttachments();

    // init state
    setBypassState(0, filterBypassButton->getToggleState());
    setBypassState(1, downsampleBypassButton->getToggleState());

    // init visibility
    setVisibility(downsampleComponents, false);
    setVisibility(graphComponents, false);

    // Set the default filter type to Low Cut before the panel is shown
    filterLowCutButton.setToggleState(true, juce::dontSendNotification);

    // Set initial switch state and trigger visibility update using buttonClicked
    filterSwitch.setToggleState(true, juce::dontSendNotification);
    buttonClicked(&filterSwitch);
}

GlobalPanel::~GlobalPanel()
{
    filterSwitch.removeListener(this);
    downsampleSwitch.removeListener(this);
    graphSwitch.removeListener(this);
}

void GlobalPanel::createSliders()
{
    // Global Knobs
    createAndConfigureSlider(GLOBAL_OUTPUT_NAME, "Output", COLOUR1, " dB");
    createAndConfigureSlider(GLOBAL_MIX_NAME, "Mix", COLOUR1);

    // Lo-fi section sliders
    const auto lofiColour = DOWNSAMPLE_COLOUR.withBrightness(0.8f);
    createAndConfigureSlider(DOWNSAMPLE_NAME, "Rate", lofiColour);
    createAndConfigureSlider(BIT_DEPTH_NAME, "Bits", lofiColour);
    createAndConfigureSlider(JITTER_NAME, "Jitter", lofiColour);
    createAndConfigureSlider(DOWNSAMPLE_MIX_NAME, "Mix", lofiColour);

    // Filter Knobs
    const auto filterColour = FILTER_COLOUR.withBrightness(0.8f);
    createAndConfigureSlider(LOWCUT_FREQ_NAME, "Frequency", filterColour);
    createAndConfigureSlider(LOWCUT_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(LOWCUT_GAIN_NAME, "Gain", filterColour);
    createAndConfigureSlider(HIGHCUT_FREQ_NAME, "Frequency", filterColour);
    createAndConfigureSlider(HIGHCUT_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(HIGHCUT_GAIN_NAME, "Gain", filterColour);
    createAndConfigureSlider(PEAK_FREQ_NAME, "Frequency", filterColour);
    createAndConfigureSlider(PEAK_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(PEAK_GAIN_NAME, "Gain", filterColour);
}

void GlobalPanel::createLabels()
{
    // Removed the main panel labels as they are no longer needed in the new design.
    // The switch text ("Filter", "Lo-Fi") now serves as the title.

    auto setupLabel = [this](juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupLabel(filterTypeLabel, "Type", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(lowcutSlopeLabel, "Slope", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(highcutSlopeLabel, "Slope", FILTER_COLOUR.withBrightness(0.8f));

    filterTypeLabel.attachToComponent(&filterLowCutButton, false);
    lowcutSlopeLabel.attachToComponent(&lowcutSlopeMode, false);
    lowcutSlopeLabel.setJustificationType(juce::Justification::centred);
    highcutSlopeLabel.attachToComponent(&highcutSlopeMode, false);
    highcutSlopeLabel.setJustificationType(juce::Justification::centred);
}

void GlobalPanel::createButtons()
{
    // Using a setupSwitch function similar to BandPanel for a consistent look.
    auto setupSwitch = [this](juce::TextButton& btn, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(switchButtonsGlobal);

        btn.setColour(juce::TextButton::buttonColourId, COLOUR8);
        btn.setColour(juce::TextButton::textColourOffId, colour);
        btn.setColour(juce::TextButton::buttonOnColourId, colour.darker().darker());
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

        btn.addListener(this);
    };

    setupSwitch(filterSwitch, "Filter", FILTER_COLOUR);
    setupSwitch(downsampleSwitch, "Lo-Fi", DOWNSAMPLE_COLOUR);
    setupSwitch(graphSwitch, "Graphs", juce::Colours::blueviolet);

    filterBypassButton = std::make_unique<juce::ToggleButton>();
    initBypassButton(*filterBypassButton, FILTER_COLOUR);
    filterBypassButton->onClick = [this]
    { setBypassState(0, filterBypassButton->getToggleState()); };

    downsampleBypassButton = std::make_unique<juce::ToggleButton>();
    initBypassButton(*downsampleBypassButton, DOWNSAMPLE_COLOUR);
    downsampleBypassButton->onClick = [this]
    { setBypassState(1, downsampleBypassButton->getToggleState()); };

    setRoundButton(filterLowCutButton, LOW_ID, "");
    setRoundButton(filterPeakButton, BAND_ID, "");
    setRoundButton(filterHighCutButton, HIGH_ID, "");

    filterLowCutButton.setComponentID("low_cut");
    filterPeakButton.setComponentID("band_pass");
    filterHighCutButton.setComponentID("high_cut");

    filterLowCutButton.setRadioGroupId(filterModeButtons);
    filterPeakButton.setRadioGroupId(filterModeButtons);
    filterHighCutButton.setRadioGroupId(filterModeButtons);
}

void GlobalPanel::createComboBoxes()
{
    addAndMakeVisible(lowcutSlopeMode);
    lowcutSlopeMode.addItem("12 db", 1);
    lowcutSlopeMode.addItem("24 db", 2);
    lowcutSlopeMode.addItem("36 db", 3);
    lowcutSlopeMode.addItem("48 db", 4);

    addAndMakeVisible(highcutSlopeMode);
    highcutSlopeMode.addItem("12 db", 1);
    highcutSlopeMode.addItem("24 db", 2);
    highcutSlopeMode.addItem("36 db", 3);
    highcutSlopeMode.addItem("48 db", 4);
}

void GlobalPanel::setupComponentGroups()
{
    lowcutKnobs = {
        modulatableSliderComponents.at(LOWCUT_FREQ_NAME).get(),
        modulatableSliderComponents.at(LOWCUT_Q_NAME).get(),
        modulatableSliderComponents.at(LOWCUT_GAIN_NAME).get(),
        &lowcutSlopeMode,
        &lowcutSlopeLabel
    };

    peakKnobs = {
        modulatableSliderComponents.at(PEAK_FREQ_NAME).get(),
        modulatableSliderComponents.at(PEAK_Q_NAME).get(),
        modulatableSliderComponents.at(PEAK_GAIN_NAME).get(),
    };

    highcutKnobs = {
        modulatableSliderComponents.at(HIGHCUT_FREQ_NAME).get(),
        modulatableSliderComponents.at(HIGHCUT_Q_NAME).get(),
        modulatableSliderComponents.at(HIGHCUT_GAIN_NAME).get(),
        &highcutSlopeMode,
        &highcutSlopeLabel
    };

    graphComponents = {
        &oscilloscope,
        &vuPanel,
        &widthGraph
    };

    filterComponents = { &filterLowCutButton, &filterPeakButton, &filterHighCutButton, &filterTypeLabel };
    filterComponents.addArray(lowcutKnobs);
    filterComponents.addArray(peakKnobs);
    filterComponents.addArray(highcutKnobs);

    downsampleComponents = {
        modulatableSliderComponents.at(DOWNSAMPLE_NAME).get(),
        modulatableSliderComponents.at(BIT_DEPTH_NAME).get(),
        modulatableSliderComponents.at(JITTER_NAME).get(),
        modulatableSliderComponents.at(DOWNSAMPLE_MIX_NAME).get(),
    };

    allControls.addArray(filterComponents);
    allControls.addArray(downsampleComponents);
    allControls.addArray(graphComponents);
    allControls.add(modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME).get());
    allControls.add(modulatableSliderComponents.at(GLOBAL_MIX_NAME).get());
}

void GlobalPanel::updateAttachments()
{
    sliderAttachments.clear();
    for (const auto& paramInfo : ParameterIDAndName::getGlobalParameterInfo())
    {
        if (modulatableSliderComponents.count(paramInfo.name))
        {
            auto* slider = modulatableSliderComponents.at(paramInfo.name).get();
            auto paramID = paramInfo.idBase;
            slider->parameterID = paramID;
            auto* parameter = processor.treeState.getParameter(paramID);
            jassert(parameter != nullptr && "Global Parameter not found!");
            if (parameter)
            {
                sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
            }
        }
    }

    filterLowAttachment = std::make_unique<ButtonAttachment>(processor.treeState, LOW_ID, filterLowCutButton);
    filterBandAttachment = std::make_unique<ButtonAttachment>(processor.treeState, BAND_ID, filterPeakButton);
    filterHighAttachment = std::make_unique<ButtonAttachment>(processor.treeState, HIGH_ID, filterHighCutButton);
    filterBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, FILTER_BYPASS_ID, *filterBypassButton);
    downsampleBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, DOWNSAMPLE_BYPASS_ID, *downsampleBypassButton);
    lowcutModeAttachment = std::make_unique<ComboBoxAttachment>(processor.treeState, LOWCUT_SLOPE_ID, lowcutSlopeMode);
    highcutModeAttachment = std::make_unique<ComboBoxAttachment>(processor.treeState, HIGHCUT_SLOPE_ID, highcutSlopeMode);
}

void GlobalPanel::initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour)
{
    addAndMakeVisible(bypassButton);
    bypassButton.setColour(juce::ToggleButton::tickColourId, colour);
}

void GlobalPanel::setRoundButton(juce::TextButton& button, juce::String paramId, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::TextButton::buttonOnColourId, COLOUR6.withBrightness(0.1f));
    button.setColour(juce::ComboBox::outlineColourId, COLOUR6);
    button.setColour(juce::TextButton::textColourOnId, FILTER_COLOUR.withBrightness(0.8f));
    button.setColour(juce::TextButton::textColourOffId, COLOUR7.withBrightness(0.8f));
    if (button.getComponentID().isEmpty())
    {
        button.setComponentID("rounded");
        button.setButtonText(buttonName);
    }
    button.addListener(this);
}

void GlobalPanel::paint(juce::Graphics& g)
{
    // Draw borders for the new layout areas, just like in BandPanel
    g.setColour(COLOUR6);
    g.drawRect(outputAreaRect); // Border for output section
    g.drawRect(tabAreaRect); // Border for the main controls section

    // Optional: Draw a themed border for the active tab
    // g.setColour(activeTabColour.withAlpha(0.8f));
    // g.drawRect(tabAreaRect, 2.0f);
}

void GlobalPanel::resized()
{
    const float scale = this->scale;
    const int scaledKnobSize = static_cast<int>(KNOB_SIZE * scale);
    const int scaledSpacing = static_cast<int>(10 * scale);

    // The main area is reduced only once to create the outer margin.
    auto mainArea = getLocalBounds().reduced(scaledSpacing);

    // --- Define proportions ---
    const float switchColumnProportion = 0.15f;
    const float outputColumnProportion = 0.2f;

    // --- Sequentially lay out the columns ---
    auto layoutArea = mainArea;
    auto switchColumnArea = layoutArea.removeFromLeft(mainArea.getWidth() * switchColumnProportion);
    auto outputColumnArea = layoutArea.removeFromRight(mainArea.getWidth() * outputColumnProportion);
    auto knobsColumnArea = layoutArea; // Knobs area is what's left.

    tabAreaRect = switchColumnArea.getUnion(knobsColumnArea);
    outputAreaRect = outputColumnArea;

    // --- Column 1: Layout Switches ---
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    switchColumnBox.alignItems = juce::FlexBox::AlignItems::stretch;
    switchColumnBox.items.add(juce::FlexItem(filterSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(downsampleSwitch).withFlex(1.0f));
    switchColumnBox.items.add(juce::FlexItem(graphSwitch).withFlex(1.0f));
    switchColumnBox.performLayout(switchColumnArea);

    auto layoutBypassButton = [&](juce::ToggleButton& bypass, const juce::TextButton& parentSwitch)
    {
        auto parentBounds = parentSwitch.getBounds();
        const int bypassSize = (int) (KNOB_FONT_SIZE * 2.0f * scale);

        bypass.setBounds(parentBounds.getX(),
                         parentBounds.getCentreY() - (bypassSize / 2),
                         bypassSize,
                         bypassSize);
        bypass.toFront(false);
    };

    layoutBypassButton(*filterBypassButton, filterSwitch);
    layoutBypassButton(*downsampleBypassButton, downsampleSwitch);

    // --- Column 3: Layout for Output Section ---
    auto knobsArea = outputColumnArea.withSizeKeepingCentre(scaledKnobSize, scaledKnobSize * 2 + scaledSpacing);
    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setBounds(knobsArea.removeFromTop(scaledKnobSize));
    knobsArea.removeFromTop(scaledSpacing); // spacing
    modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setBounds(knobsArea.removeFromTop(scaledKnobSize));

    // --- Column 2: Layout for Main Knobs/Graphs Area ---
    if (filterSwitch.getToggleState())
    {
        auto controlArea = knobsColumnArea;
        auto leftHalf = controlArea.removeFromLeft(controlArea.getWidth() / 2);
        auto rightHalf = controlArea;

        rightHalf.removeFromLeft(scaledSpacing);

        {
            auto buttonColumn = leftHalf.removeFromLeft(leftHalf.getWidth() / 2);
            auto comboColumn = leftHalf;

            const int buttonHeight = static_cast<int>(30 * scale);
            const int buttonWidth = static_cast<int>(60 * scale);
            const int verticalPadding = static_cast<int>(5 * scale);

            const int totalButtonBlockHeight = (buttonHeight * 3) + (verticalPadding * 2);

            auto buttonBlockArea = buttonColumn.withSizeKeepingCentre(buttonWidth, totalButtonBlockHeight);

            filterLowCutButton.setBounds(buttonBlockArea.removeFromTop(buttonHeight));
            buttonBlockArea.removeFromTop(verticalPadding);
            filterPeakButton.setBounds(buttonBlockArea.removeFromTop(buttonHeight));
            buttonBlockArea.removeFromTop(verticalPadding);
            filterHighCutButton.setBounds(buttonBlockArea);

            const int comboBoxWidth = static_cast<int>(70 * scale);
            const int comboBoxHeight = static_cast<int>(30 * scale);
            auto comboBoxBounds = comboColumn.withSizeKeepingCentre(comboBoxWidth, comboBoxHeight);
            lowcutSlopeMode.setBounds(comboBoxBounds);
            highcutSlopeMode.setBounds(comboBoxBounds);
        }

        {
            const int numKnobs = 3;
            const int totalKnobsWidth = (numKnobs * scaledKnobSize) + ((numKnobs - 1) * scaledSpacing);
            auto knobsArea = rightHalf.withSizeKeepingCentre(totalKnobsWidth, scaledKnobSize);

            auto freqBounds = knobsArea.removeFromLeft(scaledKnobSize);
            knobsArea.removeFromLeft(scaledSpacing);
            auto gainBounds = knobsArea.removeFromLeft(scaledKnobSize);
            knobsArea.removeFromLeft(scaledSpacing);
            auto qBounds = knobsArea;

            modulatableSliderComponents.at(LOWCUT_FREQ_NAME)->setBounds(freqBounds);
            modulatableSliderComponents.at(PEAK_FREQ_NAME)->setBounds(freqBounds);
            modulatableSliderComponents.at(HIGHCUT_FREQ_NAME)->setBounds(freqBounds);

            modulatableSliderComponents.at(LOWCUT_GAIN_NAME)->setBounds(gainBounds);
            modulatableSliderComponents.at(PEAK_GAIN_NAME)->setBounds(gainBounds);
            modulatableSliderComponents.at(HIGHCUT_GAIN_NAME)->setBounds(gainBounds);

            modulatableSliderComponents.at(LOWCUT_Q_NAME)->setBounds(qBounds);
            modulatableSliderComponents.at(PEAK_Q_NAME)->setBounds(qBounds);
            modulatableSliderComponents.at(HIGHCUT_Q_NAME)->setBounds(qBounds);
        }
    }
    else if (downsampleSwitch.getToggleState())
    {
        auto centeredArea = knobsColumnArea.withSizeKeepingCentre(scaledKnobSize * 2 + scaledSpacing, scaledKnobSize * 2 + scaledSpacing);
        auto topRow = centeredArea.removeFromTop(scaledKnobSize);
        auto bottomRow = centeredArea.removeFromBottom(scaledKnobSize);
        modulatableSliderComponents.at(DOWNSAMPLE_NAME)->setBounds(topRow.removeFromLeft(scaledKnobSize));
        topRow.removeFromLeft(scaledSpacing);
        modulatableSliderComponents.at(BIT_DEPTH_NAME)->setBounds(topRow.removeFromLeft(scaledKnobSize));
        modulatableSliderComponents.at(JITTER_NAME)->setBounds(bottomRow.removeFromLeft(scaledKnobSize));
        bottomRow.removeFromLeft(scaledSpacing);
        modulatableSliderComponents.at(DOWNSAMPLE_MIX_NAME)->setBounds(bottomRow.removeFromLeft(scaledKnobSize));
    }
    else if (graphSwitch.getToggleState())
    {
        juce::FlexBox graphBox;
        graphBox.flexDirection = juce::FlexBox::Direction::row;
        graphBox.justifyContent = juce::FlexBox::JustifyContent::center;

        graphBox.items.add(juce::FlexItem(oscilloscope).withFlex(1.0f));
        graphBox.items.add(juce::FlexItem(vuPanel).withFlex(1.0f));
        graphBox.items.add(juce::FlexItem(widthGraph).withFlex(1.0f));

        graphBox.performLayout(knobsColumnArea);
    }
}

void GlobalPanel::buttonClicked(juce::Button* clickedButton)
{
    bool isSwitch = false;
    if (clickedButton == &filterSwitch && filterSwitch.getToggleState())
    {
        activeTabColour = FILTER_COLOUR;
        setVisibility(filterComponents, true);
        setVisibility(downsampleComponents, false);
        setVisibility(graphComponents, false);
        updateFilterKnobVisibility();
        isSwitch = true;
    }
    else if (clickedButton == &downsampleSwitch && downsampleSwitch.getToggleState())
    {
        activeTabColour = DOWNSAMPLE_COLOUR;
        setVisibility(downsampleComponents, true);
        setVisibility(filterComponents, false);
        setVisibility(graphComponents, false);
        isSwitch = true;
    }
    else if (clickedButton == &graphSwitch && graphSwitch.getToggleState())
    {
        activeTabColour = COLOUR1;
        setVisibility(graphComponents, true);
        setVisibility(filterComponents, false);
        setVisibility(downsampleComponents, false);
        isSwitch = true;
    }
    else if (clickedButton == &filterLowCutButton || clickedButton == &filterPeakButton || clickedButton == &filterHighCutButton)
    {
        updateFilterKnobVisibility();
    }

    if (isSwitch)
    {
        resized();
        repaint();
    }
}

void GlobalPanel::updateFilterKnobVisibility()
{
    // This function assumes the main filter panel is already visible.
    bool peakVisible = filterPeakButton.getToggleState();
    bool lowcutVisible = filterLowCutButton.getToggleState();
    bool highcutVisible = filterHighCutButton.getToggleState();

    setVisibility(peakKnobs, peakVisible);
    setVisibility(lowcutKnobs, lowcutVisible);
    setVisibility(highcutKnobs, highcutVisible);
}

void GlobalPanel::setVisibility(juce::Array<juce::Component*>& array, bool isVisible)
{
    for (auto* component : array)
    {
        component->setVisible(isVisible);
    }
}

ModulatableSlider& GlobalPanel::getLowcutFreqKnob() { return *modulatableSliderComponents.at(LOWCUT_FREQ_NAME); }
ModulatableSlider& GlobalPanel::getPeakFreqKnob() { return *modulatableSliderComponents.at(PEAK_FREQ_NAME); }
ModulatableSlider& GlobalPanel::getHighcutFreqKnob() { return *modulatableSliderComponents.at(HIGHCUT_FREQ_NAME); }
ModulatableSlider& GlobalPanel::getLowcutGainKnob() { return *modulatableSliderComponents.at(LOWCUT_GAIN_NAME); }
ModulatableSlider& GlobalPanel::getPeakGainKnob() { return *modulatableSliderComponents.at(PEAK_GAIN_NAME); }
ModulatableSlider& GlobalPanel::getHighcutGainKnob() { return *modulatableSliderComponents.at(HIGHCUT_GAIN_NAME); }

void GlobalPanel::setToggleButtonState(juce::String toggleButton)
{
    if (toggleButton == "lowcut")
        filterLowCutButton.setToggleState(true, juce::NotificationType::sendNotification);
    if (toggleButton == "peak")
        filterPeakButton.setToggleState(true, juce::NotificationType::sendNotification);
    if (toggleButton == "highcut")
        filterHighCutButton.setToggleState(true, juce::NotificationType::sendNotification);
}

void GlobalPanel::setBypassState(int index, bool state)
{
    // Simplified logic as the bypass buttons are now separate from the main component groups
    if (index == 0) // Filter
    {
        for (auto* component : filterComponents)
            component->setEnabled(state);
    }
    else if (index == 1) // Lo-Fi (Downsample)
    {
        for (auto* component : downsampleComponents)
            component->setEnabled(state);
    }
}
