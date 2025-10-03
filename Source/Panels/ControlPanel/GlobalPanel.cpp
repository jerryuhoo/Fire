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
GlobalPanel::GlobalPanel(FireAudioProcessor& p) : processor(p)
{
    createSliders();
    createLabels();
    createButtons();
    createComboBoxes();

    setupComponentGroups();

    updateAttachments();

    // init state
    setBypassState(0, filterBypassButton->getToggleState());
    setBypassState(1, downsampleBypassButton->getToggleState());

    // init visibility
    setVisibility(downsampleComponents, false);
    downsampleBypassButton->setVisible(false);

    // Set the default filter type to Low Cut before the panel is shown
    filterLowCutButton.setToggleState(true, juce::dontSendNotification);

    // Set initial switch state and trigger visibility update
    filterSwitch.setToggleState(true, juce::dontSendNotification);
    buttonClicked(&filterSwitch);
}

GlobalPanel::~GlobalPanel()
{
}

void GlobalPanel::createSliders()
{
    // === Create Modulatable Sliders in a loop ===
    for (const auto& paramInfo : ParameterIDAndName::getGlobalParameterInfo())
    {
        modulatableSliderComponents[paramInfo.name] = std::make_unique<ModulatableSlider>();
        auto* slider = modulatableSliderComponents.at(paramInfo.name).get();

        initRotarySlider(*slider, juce::Colours::white); // Color will be set below
        modulatableSliders.push_back(slider);
    }

    // === Configure specific slider properties ===
    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setColour(juce::Slider::rotarySliderFillColourId, COLOUR1);
    modulatableSliderComponents.at(DOWNSAMPLE_NAME)->setColour(juce::Slider::rotarySliderFillColourId, DOWNSAMPLE_COLOUR.withBrightness(0.8f));

    auto setFilterKnobColour = [&](const juce::String& name)
    {
        modulatableSliderComponents.at(name)->setColour(juce::Slider::rotarySliderFillColourId, FILTER_COLOUR.withBrightness(0.8f));
    };

    setFilterKnobColour(LOWCUT_FREQ_NAME);
    setFilterKnobColour(LOWCUT_Q_NAME);
    setFilterKnobColour(LOWCUT_GAIN_NAME);
    setFilterKnobColour(HIGHCUT_FREQ_NAME);
    setFilterKnobColour(HIGHCUT_Q_NAME);
    setFilterKnobColour(HIGHCUT_GAIN_NAME);
    setFilterKnobColour(PEAK_FREQ_NAME);
    setFilterKnobColour(PEAK_Q_NAME);
    setFilterKnobColour(PEAK_GAIN_NAME);

    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setTextValueSuffix(" dB");
}

void GlobalPanel::createLabels()
{
    auto setupLabel = [this](const juce::String& paramName, const juce::String& text, juce::Colour colour, bool attachToLeft = false)
    {
        labels[paramName] = std::make_unique<juce::Label>();
        auto* label = labels.at(paramName).get();
        addAndMakeVisible(label);
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
        label->setColour(juce::Label::textColourId, colour);
        label->attachToComponent(modulatableSliderComponents.at(paramName).get(), attachToLeft);
        label->setJustificationType(juce::Justification::centred);
    };

    setupLabel(GLOBAL_OUTPUT_NAME, "Output", KNOB_FONT_COLOUR);
    setupLabel(GLOBAL_MIX_NAME, "Mix", KNOB_FONT_COLOUR);
    setupLabel(DOWNSAMPLE_NAME, "Downsample", DOWNSAMPLE_COLOUR.withBrightness(0.8f));
    setupLabel(LOWCUT_FREQ_NAME, "Frequency", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(LOWCUT_Q_NAME, "Q", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(LOWCUT_GAIN_NAME, "Gain", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(HIGHCUT_FREQ_NAME, "Frequency", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(HIGHCUT_Q_NAME, "Q", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(HIGHCUT_GAIN_NAME, "Gain", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(PEAK_FREQ_NAME, "Frequency", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(PEAK_Q_NAME, "Q", FILTER_COLOUR.withBrightness(0.8f));
    setupLabel(PEAK_GAIN_NAME, "Gain", FILTER_COLOUR.withBrightness(0.8f));

    auto setupPanelLabel = [this](juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(KNOB_FONT_SIZE).withStyle("Plain") });
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupPanelLabel(postFilterPanelLabel, "Post Filter", FILTER_COLOUR);
    setupPanelLabel(downSamplePanelLabel, "DownSample", DOWNSAMPLE_COLOUR);
    setupPanelLabel(filterTypeLabel, "Type", FILTER_COLOUR.withBrightness(0.8f));
    setupPanelLabel(lowcutSlopeLabel, "Slope", FILTER_COLOUR.withBrightness(0.8f));
    setupPanelLabel(highcutSlopeLabel, "Slope", FILTER_COLOUR.withBrightness(0.8f));

    filterTypeLabel.attachToComponent(&filterLowCutButton, false);
    lowcutSlopeLabel.attachToComponent(&lowcutSlopeMode, false);
    lowcutSlopeLabel.setJustificationType(juce::Justification::centred);
    highcutSlopeLabel.attachToComponent(&highcutSlopeMode, false);
    highcutSlopeLabel.setJustificationType(juce::Justification::centred);
}

void GlobalPanel::createButtons()
{
    auto setupSwitch = [this](juce::ToggleButton& btn, juce::Colour colour)
    {
        btn.setComponentID("flat_toggle");
        addAndMakeVisible(btn);
        btn.setRadioGroupId(switchButtonsGlobal);
        btn.setColour(juce::ToggleButton::tickDisabledColourId, colour.withBrightness(0.5f));
        btn.setColour(juce::ToggleButton::tickColourId, colour.withBrightness(0.9f));
        btn.setColour(juce::ComboBox::outlineColourId, COLOUR6);
        btn.addListener(this);
    };

    setupSwitch(filterSwitch, FILTER_COLOUR);
    setupSwitch(downsampleSwitch, DOWNSAMPLE_COLOUR);

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
    // Define knob groups for visibility switching
    lowcutKnobs = {
        modulatableSliderComponents.at(LOWCUT_FREQ_NAME).get(),
        modulatableSliderComponents.at(LOWCUT_Q_NAME).get(),
        modulatableSliderComponents.at(LOWCUT_GAIN_NAME).get(),
        labels.at(LOWCUT_FREQ_NAME).get(),
        labels.at(LOWCUT_Q_NAME).get(),
        labels.at(LOWCUT_GAIN_NAME).get(),
        &lowcutSlopeMode
    };

    peakKnobs = {
        modulatableSliderComponents.at(PEAK_FREQ_NAME).get(),
        modulatableSliderComponents.at(PEAK_Q_NAME).get(),
        modulatableSliderComponents.at(PEAK_GAIN_NAME).get(),
        labels.at(PEAK_FREQ_NAME).get(),
        labels.at(PEAK_Q_NAME).get(),
        labels.at(PEAK_GAIN_NAME).get()
    };

    highcutKnobs = {
        modulatableSliderComponents.at(HIGHCUT_FREQ_NAME).get(),
        modulatableSliderComponents.at(HIGHCUT_Q_NAME).get(),
        modulatableSliderComponents.at(HIGHCUT_GAIN_NAME).get(),
        labels.at(HIGHCUT_FREQ_NAME).get(),
        labels.at(HIGHCUT_Q_NAME).get(),
        labels.at(HIGHCUT_GAIN_NAME).get(),
        &highcutSlopeMode
    };

    filterComponents = { &filterLowCutButton, &filterPeakButton, &filterHighCutButton, &postFilterPanelLabel, &lowcutSlopeLabel, &highcutSlopeLabel };
    filterComponents.addArray(lowcutKnobs);
    filterComponents.addArray(peakKnobs);
    filterComponents.addArray(highcutKnobs);

    downsampleComponents = { modulatableSliderComponents.at(DOWNSAMPLE_NAME).get(), &downSamplePanelLabel, labels.at(DOWNSAMPLE_NAME).get() };

    allControls.addArray(filterComponents);
    allControls.addArray(downsampleComponents);
    allControls.add(modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME).get());
    allControls.add(modulatableSliderComponents.at(GLOBAL_MIX_NAME).get());
}

void GlobalPanel::updateAttachments()
{
    sliderAttachments.clear();
    for (const auto& paramInfo : ParameterIDAndName::getGlobalParameterInfo())
    {
        auto* slider = modulatableSliderComponents.at(paramInfo.name).get();
        auto paramID = paramInfo.idBase; // Global parameters don't have band index

        auto* parameter = processor.treeState.getParameter(paramID);
        jassert(parameter != nullptr && "Global Parameter not found!");

        if (parameter)
        {
            sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
        }

        configureModulatableSlider(*slider, paramID);
    }

    filterLowAttachment = std::make_unique<ButtonAttachment>(processor.treeState, LOW_ID, filterLowCutButton);
    filterBandAttachment = std::make_unique<ButtonAttachment>(processor.treeState, BAND_ID, filterPeakButton);
    filterHighAttachment = std::make_unique<ButtonAttachment>(processor.treeState, HIGH_ID, filterHighCutButton);

    filterBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, FILTER_BYPASS_ID, *filterBypassButton);
    downsampleBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, DOWNSAMPLE_BYPASS_ID, *downsampleBypassButton);

    lowcutModeAttachment = std::make_unique<ComboBoxAttachment>(processor.treeState, LOWCUT_SLOPE_ID, lowcutSlopeMode);
    highcutModeAttachment = std::make_unique<ComboBoxAttachment>(processor.treeState, HIGHCUT_SLOPE_ID, highcutSlopeMode);
}

void GlobalPanel::configureModulatableSlider(ModulatableSlider& slider, const juce::String& paramID)
{
    slider.parameterID = paramID;

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

    slider.onModulationCleared = [this, &slider]()
    {
        processor.clearModulationForParameter(slider.parameterID);
    };

    slider.onModulationInverted = [this, &slider]()
    {
        processor.invertModulationDepthForParameter(slider.parameterID);
    };
}

void GlobalPanel::initRotarySlider(juce::Slider& slider, juce::Colour colour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
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
    g.setColour(COLOUR6);
    g.drawRect(globalEffectArea);
    g.drawRect(outputKnobArea);

    // Fill the bottom area with a darker background color for visual separation
    g.setColour(COLOUR6.darker());
    g.fillRect(bottomArea);

    // Draw a border around the bottom area with the original border color
    g.setColour(COLOUR6);
    g.drawRect(bottomArea);
}

void GlobalPanel::resized()
{
    juce::Rectangle<int> controlArea = getLocalBounds();

    globalEffectArea = controlArea.removeFromLeft(getWidth() / 7 * 5);
    outputKnobArea = controlArea;

    juce::Rectangle<int> outputKnobAreaLeft = outputKnobArea;
    juce::Rectangle<int> outputKnobAreaRight = outputKnobAreaLeft.removeFromRight(outputKnobArea.getWidth() / 2);
    outputKnobAreaLeft = outputKnobAreaLeft.reduced(0, outputKnobAreaLeft.getHeight() / 5);
    outputKnobAreaRight = outputKnobAreaRight.reduced(0, outputKnobAreaRight.getHeight() / 5);
    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setBounds(outputKnobAreaLeft);
    modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setBounds(outputKnobAreaRight);

    juce::Rectangle<int> switchArea = globalEffectArea.removeFromLeft(getWidth() / 50);
    filterSwitch.setBounds(switchArea.removeFromTop(globalEffectArea.getHeight() / 2));
    downsampleSwitch.setBounds(switchArea.removeFromTop(globalEffectArea.getHeight() / 2));

    juce::Rectangle<int> fiveColumnArea = globalEffectArea;

    // 1. Define the geometry of our grid
    const int numColumns = 5;
    const int numGaps = numColumns - 1; // 5列之間有4個間隔
    const int gapWidth = 10; // 每個間隔的寬度

    // 2. Calculate the total width available for content (columns)
    //    總寬度減去所有間隔的總寬度
    const float totalContentWidth = fiveColumnArea.getWidth() - (numGaps * gapWidth);

    // 3. Calculate the width of each column based on proportions
    //    第一列佔 1/6，其餘四列佔 5/6，我們按這個比例分配剩餘的內容寬度
    const float narrowColumnProportion = 1.0f / 6.0f;
    const float wideColumnsTotalProportion = 5.0f / 6.0f;

    int col1Width = static_cast<int>(totalContentWidth * narrowColumnProportion);
    int otherColumnTotalWidth = static_cast<int>(totalContentWidth * wideColumnsTotalProportion);
    int otherColumnWidth = otherColumnTotalWidth / 4;

    // 4. Create the column rectangles by placing them sequentially with gaps
    juce::Array<juce::Rectangle<int>> columns;
    juce::Rectangle<int> placementArea = fiveColumnArea;

    columns.add(placementArea.removeFromLeft(col1Width));
    const int shiftRightAmount = 10;
    columns.getReference(0).setX(columns.getReference(0).getX() + shiftRightAmount);
    placementArea.removeFromLeft(gapWidth); // Add gap after column 1

    columns.add(placementArea.removeFromLeft(otherColumnWidth));
    placementArea.removeFromLeft(gapWidth); // Add gap after column 2

    columns.add(placementArea.removeFromLeft(otherColumnWidth));
    placementArea.removeFromLeft(gapWidth); // Add gap after column 3

    columns.add(placementArea.removeFromLeft(otherColumnWidth));
    placementArea.removeFromLeft(gapWidth); // Add gap after column 4

    columns.add(placementArea); // The last column fills the remaining space

    // Now the `columns` array holds perfectly spaced, clean rectangles.
    // The rest of the layout logic can proceed as before.

    // Define the vertical area for the knobs first
    juce::Rectangle<int> knob_yBasis = globalEffectArea.reduced(0, getHeight() / 5);

    // --- Button Column Layout ---
    auto buttonArea = columns[0];
    buttonArea.setY(knob_yBasis.getY());
    buttonArea.setHeight(knob_yBasis.getHeight());

    const int extraSideMargin = 10;
    buttonArea.reduce(extraSideMargin, 0);

    const int verticalPadding = 10;
    const int horizontalPadding = 0;
    auto layoutArea = buttonArea;
    const int totalPaddingHeight = verticalPadding * 2;
    const int singleButtonHeight = (layoutArea.getHeight() - totalPaddingHeight) / 3;

    filterLowCutButton.setBounds(layoutArea.removeFromTop(singleButtonHeight).reduced(horizontalPadding, 0));
    layoutArea.removeFromTop(verticalPadding);
    filterPeakButton.setBounds(layoutArea.removeFromTop(singleButtonHeight).reduced(horizontalPadding, 0));
    layoutArea.removeFromTop(verticalPadding);
    filterHighCutButton.setBounds(layoutArea.reduced(horizontalPadding, 0));

    // --- Knobs Layout ---
    modulatableSliderComponents.at(LOWCUT_FREQ_NAME)->setBounds(columns[2].getX(), knob_yBasis.getY(), columns[2].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(HIGHCUT_FREQ_NAME)->setBounds(columns[2].getX(), knob_yBasis.getY(), columns[2].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(PEAK_FREQ_NAME)->setBounds(columns[2].getX(), knob_yBasis.getY(), columns[2].getWidth(), knob_yBasis.getHeight());

    modulatableSliderComponents.at(LOWCUT_GAIN_NAME)->setBounds(columns[3].getX(), knob_yBasis.getY(), columns[3].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(HIGHCUT_GAIN_NAME)->setBounds(columns[3].getX(), knob_yBasis.getY(), columns[3].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(PEAK_GAIN_NAME)->setBounds(columns[3].getX(), knob_yBasis.getY(), columns[3].getWidth(), knob_yBasis.getHeight());

    modulatableSliderComponents.at(LOWCUT_Q_NAME)->setBounds(columns[4].getX(), knob_yBasis.getY(), columns[4].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(HIGHCUT_Q_NAME)->setBounds(columns[4].getX(), knob_yBasis.getY(), columns[4].getWidth(), knob_yBasis.getHeight());
    modulatableSliderComponents.at(PEAK_Q_NAME)->setBounds(columns[4].getX(), knob_yBasis.getY(), columns[4].getWidth(), knob_yBasis.getHeight());

    // --- ComboBox Column Layout ---
    const int comboBoxY = knob_yBasis.getY();
    const int comboBoxHeight = TEXTBOX_HEIGHT;
    const int comboBoxWidth = TEXTBOX_WIDTH;

    auto col2 = columns[1]; // Use the clean rectangle from our new calculation
    // Create a temporary, reduced area for placement to add horizontal margin
    auto comboBoxPlacementArea = col2.reduced(extraSideMargin, 5);
    // Center the ComboBox horizontally within the NEW reduced placement area
    const int comboBoxX = comboBoxPlacementArea.getCentreX() - (comboBoxWidth / 2);

    juce::Rectangle<int> comboBoxBounds(comboBoxX, comboBoxY, comboBoxWidth * 1.2f, comboBoxHeight);
    lowcutSlopeMode.setBounds(comboBoxBounds);
    highcutSlopeMode.setBounds(comboBoxBounds);
    // =====================================================================

    modulatableSliderComponents.at(DOWNSAMPLE_NAME)->setBounds(globalEffectArea.reduced(getHeight() / 15, getHeight() / 5));

    // 1. Define a shared bottom area for the labels and bypass buttons.
    const int sliceHeight = globalEffectArea.getHeight() / 5;
    bottomArea = juce::Rectangle<int>(globalEffectArea.getX(),
                                      globalEffectArea.getBottom() - sliceHeight,
                                      globalEffectArea.getWidth(),
                                      sliceHeight);

    bottomArea.reduce(globalEffectArea.getWidth() / 4, 0);

    // 2. Define sizing constants. The button will be a square.
    const int buttonSize = bottomArea.getHeight();
    const int labelWidth = 100; // A suitable width for "Post Filter" / "DownSample"
    const int gap = 4; // Gap between button and label

    // 3. Create the label area, centered horizontally within the bottom area.
    juce::Rectangle<int> labelArea = bottomArea.withSizeKeepingCentre(labelWidth, buttonSize);

    // 4. Create the bypass button area by translating the label area to the left.
    juce::Rectangle<int> bypassButtonArea = labelArea.translated(-buttonSize - gap, 0)
                                                .withSize(buttonSize, buttonSize);

    // 5. Set the bounds for both filter and downsample components, as they share the same space.
    postFilterPanelLabel.setBounds(labelArea);
    downSamplePanelLabel.setBounds(labelArea);

    filterBypassButton->setBounds(bypassButtonArea);
    downsampleBypassButton->setBounds(bypassButtonArea);
}

void GlobalPanel::buttonClicked(juce::Button* clickedButton)
{
    if (clickedButton == &filterSwitch && filterSwitch.getToggleState())
    {
        setVisibility(filterComponents, true);
        filterBypassButton->setVisible(true);
        setVisibility(downsampleComponents, false);
        downsampleBypassButton->setVisible(false);
        updateFilterKnobVisibility(); // Update knobs when switching to filter panel
    }
    else if (clickedButton == &downsampleSwitch && downsampleSwitch.getToggleState())
    {
        setVisibility(downsampleComponents, true);
        downsampleBypassButton->setVisible(true);
        setVisibility(filterComponents, false);
        filterBypassButton->setVisible(false);
    }
    else if (clickedButton == &filterLowCutButton || clickedButton == &filterPeakButton || clickedButton == &filterHighCutButton)
    {
        updateFilterKnobVisibility(); // Update knobs when changing filter type
    }

    resized();
    repaint();
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
    // BUG 2 FIX: Corrected the button toggling logic.
    if (toggleButton == "lowcut")
        filterLowCutButton.setToggleState(true, juce::NotificationType::sendNotification);
    if (toggleButton == "peak")
        filterPeakButton.setToggleState(true, juce::NotificationType::sendNotification);
    if (toggleButton == "highcut")
        filterHighCutButton.setToggleState(true, juce::NotificationType::sendNotification);
}

void GlobalPanel::setBypassState(int index, bool state)
{
    juce::Array<juce::Component*>* componentArray = nullptr;
    if (index == 0)
    {
        // For the filter section, enable/disable all of its child controls
        componentArray = &filterComponents;

        // Explicitly set the state for the filter type buttons as well
        filterLowCutButton.setEnabled(state);
        filterPeakButton.setEnabled(state);
        filterHighCutButton.setEnabled(state);
        filterTypeLabel.setEnabled(state);
    }
    else if (index == 1)
    {
        componentArray = &downsampleComponents;
    }

    if (componentArray)
    {
        for (auto* component : *componentArray)
        {
            component->setEnabled(state);
        }
    }
}