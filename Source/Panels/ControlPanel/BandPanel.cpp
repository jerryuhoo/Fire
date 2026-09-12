/*
  ==============================================================================

    BandPanel.cpp
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "BandPanel.h"
#include "../../GUI/FireTheme.h"
#include "../../Utility/AudioHelpers.h"
#include <algorithm>
#include <cmath>

namespace
{
void drawMinimalSurface(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;

    bounds = bounds.reduced(0.5f);
    juce::ColourGradient fill(fire::ui::colours::surface1.withAlpha(0.72f),
                              bounds.getX(), bounds.getY(),
                              fire::ui::colours::surface0.withAlpha(0.88f),
                              bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, fire::ui::Metrics::radius);
}

void drawMinimalTitle(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      const juce::String& text)
{
    g.setFont(fire::ui::labelFont(juce::jlimit(10.0f, 20.0f, bounds.getHeight() * 0.46f)));
    g.setColour(fire::ui::colours::textSecondary.withAlpha(0.82f));
    g.drawText(text.toUpperCase(), bounds, juce::Justification::centredLeft);
}

template <typename PanelType>
bool dismissInteractionBeforeComponentStateChange(
    juce::Component* component,
    bool stateWillChange,
    const juce::Component::SafePointer<PanelType>& safePanel)
{
    if (component == nullptr || ! stateWillChange)
        return safePanel != nullptr;

    if (auto* slider = dynamic_cast<ModulatableSlider*>(component))
        slider->dismissTransientInteraction();
    else if (auto* comboBox = dynamic_cast<ContextAwareComboBox*>(component))
        comboBox->dismissTransientInteraction();
    else if (auto* textButton = dynamic_cast<PrimaryTextButton*>(component))
        textButton->dismissPointerGesture();
    else if (auto* toggleButton = dynamic_cast<PrimaryToggleButton*>(component))
        toggleButton->dismissPointerGesture();

    return safePanel != nullptr;
}
} // namespace

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

    std::array<ModulatableSlider*, fire::effects::controlCount> insertKnobs {};
    for (size_t i = 0; i < insertKnobs.size(); ++i)
    {
        const auto key = "InsertControl" + juce::String(static_cast<int>(i));
        createAndConfigureSlider(key, "", fire::ui::colours::chorus);
        insertKnobs[i] = modulatableSliderComponents.at(key).get();
        insertKnobs[i]->setInteractionOnlyReadout(true);
    }
    insertControls.setControls(insertKnobs);
    insertControls.bind(1, 0);
    addChildComponent(insertControls);
    addAndMakeVisible(effectNavigation);
    effectNavigation.setBuiltins({{&oscSwitch, &driveBypassButton}, {&shapeSwitch, &shapeBypassButton}, {&compressorSwitch, &compressorBypassButton}, {&widthSwitch, &widthBypassButton}, {&ottSwitch, &ottBypassButton}});
    effectNavigation.onSelectEffect = [this](int slot) { selectInsertEffect(slot); };

    // Assign callbacks to all modulatable sliders in this panel
    for (auto& sliderPair : modulatableSliderComponents)
    {
        auto* slider = sliderPair.second.get();
        slider->onModDragStart = onModDragStart;
        slider->onModDragMove = onModDragMove;
        slider->onModDragEnd = onModDragEnd;
        slider->onHoverStart = onHoverStart;
        slider->onHoverEnd = onHoverEnd;
        slider->onInteractionEnded = [this]
        {
            applyPendingFocusChange();
        };
    }

    // Add graph components and make them visible
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(distortionGraph);
    addAndMakeVisible(vuPanel);
    addAndMakeVisible(widthGraph);
    addAndMakeVisible(ottGraph);
    configureGraphInteractions();

    // Group components for visibility management after they've been created
    setupComponentGroups();

    // Listen directly to the parameters that affect the transfer graph.
    constexpr std::array<const char*, distortionGraphParameterCount> graphParameterBases {
        REC_ID, MIX_ID, SHAPE_MIX_ID, BIAS_ID, MODE_ID, SAFE_ID,
        DRIVE_BYPASS_ID, SHAPE_BYPASS_ID, EXTREME_ID
    };

    for (int i = 0; i < 4; ++i)
    {
        driveParameterIds[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(DRIVE_ID, i);
        processor.treeState.addParameterListener(driveParameterIds[static_cast<size_t>(i)], this);

        for (size_t parameterIndex = 0; parameterIndex < graphParameterBases.size(); ++parameterIndex)
        {
            auto& parameterId = distortionGraphParameterIds[parameterIndex][static_cast<size_t>(i)];
            parameterId = ParameterIDAndName::getIDString(graphParameterBases[parameterIndex], i);
            processor.treeState.addParameterListener(parameterId, this);
        }
    }

    // Set initial attachments for band 0 and update knob enabled states
    setFocusBandNum(0, true);

    // Set initial visibility
    moduleSelectionPosition.snapTo(0.0f);
    buttonClicked(&oscSwitch);
    startTimerHz(30);
}

BandPanel::~BandPanel()
{
    effectNavigation.onSelectEffect = nullptr;
    for (auto& sliderPair : modulatableSliderComponents)
    {
        sliderPair.second->onInteractionEnded = nullptr;
        sliderPair.second->valueConstraint = nullptr;
    }

    dismissTransientInteraction();

    // Remove all parameter listeners that were added in the constructor.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(driveParameterIds[static_cast<size_t>(i)], this);

        for (const auto& parameterIds : distortionGraphParameterIds)
            processor.treeState.removeParameterListener(parameterIds[static_cast<size_t>(i)], this);
    }

    // Remove listeners that were added in the setupSwitch lambda
    oscSwitch.removeListener(this);
    shapeSwitch.removeListener(this);
    compressorSwitch.removeListener(this);
    widthSwitch.removeListener(this);
    ottSwitch.removeListener(this);
    ottBypassButton.removeListener(this);

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
}

void BandPanel::createSliders()
{
    // Main Panel
    createAndConfigureSlider(DRIVE_NAME, "Drive", fire::ui::colours::drive);
    createAndConfigureSlider(OUTPUT_NAME, "Output", fire::ui::colours::flame, " dB");
    createAndConfigureSlider(MIX_NAME, "Mix", fire::ui::colours::gold);

    // Shape Panel
    createAndConfigureSlider(REC_NAME, "Rectification", fire::ui::colours::shape);
    createAndConfigureSlider(BIAS_NAME, "Bias", fire::ui::colours::shape);
    createAndConfigureSlider(SHAPE_MIX_NAME, "Mix", fire::ui::colours::shape);

    // Compressor Panel
    createAndConfigureSlider(COMP_THRESH_NAME, "Threshold", fire::ui::colours::compressor, " dB");
    createAndConfigureSlider(COMP_RATIO_NAME, "Ratio", fire::ui::colours::compressor, ":1");
    createAndConfigureSlider(COMP_ATTACK_NAME, "Attack", fire::ui::colours::compressor, " ms");
    createAndConfigureSlider(COMP_RELEASE_NAME, "Release", fire::ui::colours::compressor, " ms");
    createAndConfigureSlider(COMP_MIX_NAME, "Mix", fire::ui::colours::compressor);

    // Width Panel
    createAndConfigureSlider(WIDTH_NAME, "Width", fire::ui::colours::stereo);
    createAndConfigureSlider(PAN_NAME, "Pan", fire::ui::colours::stereo);
    createAndConfigureSlider(WIDTH_MIX_NAME, "Mix", fire::ui::colours::stereo);

    const std::array<const char*, 6> labels { "Depth", "Time", "Up Thresh", "Down Thresh", "Gain", "Mix" };
    const std::array<const char*, 6> units { "", " %", " dB", " dB", " dB", "" };
    for (size_t i = 0; i < labels.size(); ++i)
        createAndConfigureSlider(ParameterIDAndName::ottControlNames[i], labels[i], fire::ui::colours::ott, units[i]);
    for (auto* name : ParameterIDAndName::ottControlNames)
        modulatableSliderComponents.at(name)->setInteractionOnlyReadout(true);
    modulatableSliderComponents.at(OTT_DEPTH_NAME)->setTooltip("OTT strength: raises quiet detail and compresses loud peaks");
    modulatableSliderComponents.at(OTT_TIME_NAME)->setTooltip("Scales OTT attack and release times. 100% uses 5 ms / 100 ms");
    modulatableSliderComponents.at(OTT_UPWARD_NAME)->setTooltip("Below this threshold, OTT raises quiet signals. Kept at least 6 dB below Down");
    modulatableSliderComponents.at(OTT_DOWNWARD_NAME)->setTooltip("Above this threshold, OTT compresses loud signals");

    modulatableSliderComponents.at(OTT_UPWARD_NAME)->valueConstraint = [this](double value)
    {
        auto* other = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_DOWNWARD_ID, focusBandNum));
        return other ? juce::jmin(value, static_cast<double>(other->load()) - 6.0) : value;
    };
    modulatableSliderComponents.at(OTT_DOWNWARD_NAME)->valueConstraint = [this](double value)
    {
        return juce::jmax(value, modulatableSliderComponents.at(OTT_UPWARD_NAME)->getValue() + 6.0);
    };

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
        label.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
        juce::ignoreUnused(colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupPanelLabel(dcFilterLabel, "DC", fire::ui::colours::shape);
}

void BandPanel::createButtons()
{
    initFlatButton(linkedButton, "Link");
    initFlatButton(safeButton, "Safe");
    initFlatButton(extremeButton, "Extreme");

    // Add the new drive bypass button
    initBypassButton(driveBypassButton, fire::ui::colours::drive);
    initBypassButton(shapeBypassButton, fire::ui::colours::shape);
    initBypassButton(compressorBypassButton, fire::ui::colours::compressor);
    initBypassButton(widthBypassButton, fire::ui::colours::stereo);
    initBypassButton(ottBypassButton, fire::ui::colours::ott);

    initBypassButton(dcFilterButton, fire::ui::colours::shape);

    auto setupSwitch = [this](juce::TextButton& btn, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(switchButtons);
        btn.getProperties().set("fireAnimatedSelection", true);
        btn.getProperties().set("fireModuleRail", true);

        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
        juce::ignoreUnused(colour);

        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, fire::ui::colours::textPrimary);

        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

        btn.addListener(this);
    };

    setupSwitch(oscSwitch, "Drive", fire::ui::colours::drive);
    setupSwitch(shapeSwitch, "Shape", fire::ui::colours::shape);
    setupSwitch(compressorSwitch, "Compressor", fire::ui::colours::compressor);
    setupSwitch(widthSwitch, "Stereo", fire::ui::colours::stereo);
    setupSwitch(ottSwitch, "OTT", fire::ui::colours::ott);
    ottSwitch.setTooltip("Upward/downward compression. Drag the lower or upper spectrum line to set thresholds.");
    oscSwitch.setToggleState(true, juce::dontSendNotification);

    driveBypassButton.toFront(false);
    shapeBypassButton.toFront(false);
    compressorBypassButton.toFront(false);
    widthBypassButton.toFront(false);
}

void BandPanel::updateIconButtonSemantics()
{
    const auto bandNumber = juce::String(focusBandNum + 1);
    const auto setSemantics = [&bandNumber](juce::Button& button,
                                            const juce::String& function,
                                            const juce::String& help)
    {
        button.setTitle("Band " + bandNumber + " " + function);
        button.setTooltip(help + " for band " + bandNumber);
    };

    setSemantics(driveBypassButton,
                 "Drive power",
                 "Enable or bypass Drive processing");
    setSemantics(shapeBypassButton,
                 "Shape power",
                 "Enable or bypass Shape processing");
    setSemantics(compressorBypassButton,
                 "Compressor power",
                 "Enable or bypass Compressor processing");
    setSemantics(widthBypassButton,
                 "Stereo power",
                 "Enable or bypass Stereo processing");
    setSemantics(ottBypassButton, "OTT power", "Enable or bypass OTT processing");
    setSemantics(dcFilterButton,
                 "DC filter",
                 "Enable or disable the DC filter");
}

void BandPanel::createComboBoxes()
{
    for (size_t i = 0; i < distortionModes.size(); ++i)
    {
        const auto parameterID = ParameterIDAndName::getIDString(
            MODE_ID, static_cast<int>(i));
        const auto bandNumber = juce::String(static_cast<int>(i) + 1);
        auto* const parameter = processor.treeState.getParameter(parameterID);
        jassert(parameter != nullptr);

        distortionModes[i].setTitle("Band " + bandNumber
                                    + " distortion mode");
        distortionModes[i].setTooltip("Select the distortion mode for band "
                                      + bandNumber);

        distortionModes[i].configurePopupSession(
            [this]
            {
                return distortionModeInteractionGeneration;
            },
            [this, i]
            {
                return canOpenDistortionModePopup(i);
            },
            parameter);
        setMenu(&distortionModes[i]);
        modeAttachments[i] = std::make_unique<ComboBoxAttachment>(
            processor.treeState,
            parameterID,
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

    for (auto* name : ParameterIDAndName::ottControlNames)
        ottComponents.add(modulatableSliderComponents.at(name).get());

    // A single master list of all components for disabling the entire band
    allControls.addArray(driveComponents);
    allControls.add(modulatableSliderComponents.at(OUTPUT_NAME).get());
    allControls.add(modulatableSliderComponents.at(MIX_NAME).get());
    allControls.add(&linkedButton, &safeButton, &extremeButton);
    allControls.addArray(shapeComponents);
    allControls.addArray(compressorComponents);
    allControls.addArray(widthComponents);
    allControls.addArray(ottComponents);
    allControls.add(&insertControls);
    for (auto& modeBox : distortionModes)
        allControls.add(&modeBox);
}

void BandPanel::paint(juce::Graphics& g)
{
    const auto displayScale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (chromeCacheDirty
        || ! juce::approximatelyEqual(chromeCacheDisplayScale, displayScale)
        || chromeCache.isNull())
        rebuildChromeCache(displayScale);

    if (! chromeCache.isNull())
        g.drawImage(chromeCache, getLocalBounds().toFloat());

}

void BandPanel::resized()
{
    const float uiScale = scale;
    const int scaledKnobSize = juce::roundToInt(KNOB_SIZE * uiScale);
    const int outerPadding = juce::roundToInt(10.0f * uiScale);
    const int gap = juce::roundToInt(juce::jlimit(7.0f * uiScale,
                                                 14.0f * uiScale,
                                                 static_cast<float>(getWidth()) * 0.01f));
    const int cardPadding = juce::roundToInt(8.0f * uiScale);
    const int titleHeight = juce::roundToInt(22.0f * uiScale);

    auto layoutArea = getLocalBounds().reduced(outerPadding);
    if (layoutArea.isEmpty())
    {
        invalidateChromeCache();
        return;
    }

    const int navWidth = juce::roundToInt(juce::jlimit(120.0f * uiScale,
                                                       165.0f * uiScale,
                                                       static_cast<float>(getWidth()) * 0.14f));
    const int outputWidth = juce::roundToInt(juce::jlimit(205.0f * uiScale,
                                                          240.0f * uiScale,
                                                          static_cast<float>(getWidth()) * 0.22f));
    const int graphWidth = juce::roundToInt(juce::jlimit(220.0f * uiScale,
                                                         320.0f * uiScale,
                                                         static_cast<float>(getWidth()) * 0.27f));

    tabAreaRect = layoutArea.removeFromLeft(juce::jmin(navWidth, layoutArea.getWidth()));
    layoutArea.removeFromLeft(juce::jmin(gap, layoutArea.getWidth()));
    outputAreaRect = layoutArea.removeFromRight(juce::jmin(outputWidth, layoutArea.getWidth()));
    layoutArea.removeFromRight(juce::jmin(gap, layoutArea.getWidth()));
    graphAreaRect = layoutArea.removeFromRight(juce::jmin(graphWidth, layoutArea.getWidth()));
    layoutArea.removeFromRight(juce::jmin(gap, layoutArea.getWidth()));
    knobsAreaRect = layoutArea;

    auto contentArea = [cardPadding, titleHeight](juce::Rectangle<int> card)
    {
        card.reduce(cardPadding, cardPadding);
        card.removeFromTop(juce::jmin(titleHeight, card.getHeight()));
        return card;
    };

    auto knobsColumnArea = contentArea(knobsAreaRect);
    auto graphColumnArea = contentArea(graphAreaRect);
    auto outputColumnArea = contentArea(outputAreaRect);

    // Every secondary rotary control shares one physical diameter.  Deriving
    // it from the strictest grid/output constraint keeps that invariant intact
    // even when the editor is resized down.
    const int controlGap = juce::roundToInt(8.0f * uiScale);
    const int modeHeight = juce::jmin(juce::roundToInt(30.0f * uiScale),
                                     knobsColumnArea.getHeight() / 4);
    const int dcReserve = juce::roundToInt(26.0f * uiScale);
    const int buttonAreaHeight = juce::jmin(juce::roundToInt(28.0f * uiScale),
                                            juce::jmax(1, outputColumnArea.getHeight() / 4));
    const int valueHeight = juce::roundToInt(fire::ui::Metrics::knobValueHeight * uiScale);
    const int secondaryKnobSize = juce::jmax(1, std::min({
        juce::roundToInt(fire::ui::Metrics::knobWidth * uiScale),
        (knobsColumnArea.getWidth() - controlGap * 2) / 3,
        (knobsColumnArea.getHeight() - controlGap) / 2 - valueHeight,
        knobsColumnArea.getHeight() - modeHeight - controlGap - dcReserve - valueHeight,
        (outputColumnArea.getWidth() - controlGap) / 2,
        outputColumnArea.getHeight() - buttonAreaHeight - controlGap - valueHeight
    }));

    const int secondaryKnobHeight = secondaryKnobSize + valueHeight;

    effectNavigation.setBounds(tabAreaRect);
    effectNavigation.setScale(uiScale);
    insertControls.setScale(uiScale);
    insertControls.setBounds(knobsColumnArea);

    // --- Active module controls ---
    if (oscSwitch.getToggleState())
    {
        const int driveSize = juce::jmax(1, std::min({ scaledKnobSize * 2,
                                                      knobsColumnArea.getWidth(),
                                                      knobsColumnArea.getHeight() }));
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(
            knobsColumnArea.withSizeKeepingCentre(driveSize, driveSize));
    }
    else if (shapeSwitch.getToggleState())
    {
        auto modeArea = knobsColumnArea.removeFromTop(modeHeight);
        const int modeWidth = juce::jmin(juce::roundToInt(180.0f * uiScale), modeArea.getWidth());
        for (auto& modeBox : distortionModes)
            modeBox.setBounds(modeArea.withSizeKeepingCentre(modeWidth, modeHeight).reduced(0, juce::roundToInt(2.0f * uiScale)));

        knobsColumnArea.removeFromTop(juce::jmin(controlGap, knobsColumnArea.getHeight()));
        auto knobRow = knobsColumnArea.withSizeKeepingCentre(secondaryKnobSize * 3 + controlGap * 2,
                                                             secondaryKnobHeight + dcReserve);
        knobRow = knobRow.removeFromTop(secondaryKnobHeight);
        auto tempKnobRow = knobRow;
        modulatableSliderComponents.at(REC_NAME)->setBounds(tempKnobRow.removeFromLeft(secondaryKnobSize));
        tempKnobRow.removeFromLeft(controlGap);
        auto biasKnobBounds = tempKnobRow.removeFromLeft(secondaryKnobSize);
        modulatableSliderComponents.at(BIAS_NAME)->setBounds(biasKnobBounds);
        tempKnobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(SHAPE_MIX_NAME)->setBounds(tempKnobRow);

        const int dcButtonSize = juce::roundToInt(juce::jlimit(14.0f * uiScale,
                                                              24.0f * uiScale,
                                                              secondaryKnobSize * 0.24f));
        const int dcLabelWidth = juce::roundToInt(30.0f * uiScale);
        juce::Rectangle<int> dcArea(0, 0, dcButtonSize + dcLabelWidth, dcButtonSize);
        dcArea.setCentre(biasKnobBounds.getCentreX(), biasKnobBounds.getBottom() + dcButtonSize / 2);
        dcFilterButton.setBounds(dcArea.removeFromLeft(dcButtonSize));
        dcFilterLabel.setBounds(dcArea);

    }
    else if (compressorSwitch.getToggleState())
    {
        auto centeredArea = knobsColumnArea.withSizeKeepingCentre(secondaryKnobSize * 3 + controlGap * 2,
                                                                  secondaryKnobHeight * 2 + controlGap);
        auto topRow = centeredArea.removeFromTop(secondaryKnobHeight);
        centeredArea.removeFromTop(controlGap);
        auto bottomRow = centeredArea.removeFromTop(secondaryKnobHeight);

        modulatableSliderComponents.at(COMP_THRESH_NAME)->setBounds(topRow.removeFromLeft(secondaryKnobSize));
        modulatableSliderComponents.at(COMP_RATIO_NAME)->setBounds(topRow.removeFromRight(secondaryKnobSize));

        modulatableSliderComponents.at(COMP_ATTACK_NAME)->setBounds(bottomRow.removeFromLeft(secondaryKnobSize));
        bottomRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(COMP_RELEASE_NAME)->setBounds(bottomRow.removeFromLeft(secondaryKnobSize));
        bottomRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(COMP_MIX_NAME)->setBounds(bottomRow);
    }
    else if (widthSwitch.getToggleState())
    {
        auto knobRow = knobsColumnArea.withSizeKeepingCentre(secondaryKnobSize * 3 + controlGap * 2,
                                                             secondaryKnobHeight);
        modulatableSliderComponents.at(WIDTH_NAME)->setBounds(knobRow.removeFromLeft(secondaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(PAN_NAME)->setBounds(knobRow.removeFromLeft(secondaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(WIDTH_MIX_NAME)->setBounds(knobRow);
    }

    if (ottSwitch.getToggleState())
    {
        auto rows = knobsColumnArea.withSizeKeepingCentre(secondaryKnobSize * 3 + controlGap * 2,
                                                          secondaryKnobHeight * 2 + controlGap);
        constexpr std::array<size_t, 6> order { 0, 1, 5, 2, 3, 4 };
        for (size_t row = 0; row < 2; ++row)
        {
            auto strip = rows.removeFromTop(secondaryKnobHeight);
            rows.removeFromTop(controlGap);
            for (size_t column = 0; column < 3; ++column)
            {
                modulatableSliderComponents.at(ParameterIDAndName::ottControlNames[order[row * 3 + column]])->setBounds(strip.removeFromLeft(secondaryKnobSize));
                strip.removeFromLeft(controlGap);
            }
        }
    }

    // --- Live visualiser card ---
    oscilloscope.setBounds(graphColumnArea);
    distortionGraph.setBounds(graphColumnArea);
    vuPanel.setBounds(graphColumnArea);
    widthGraph.setBounds(graphColumnArea);
    ottGraph.setBounds(graphColumnArea);

    if (zoomedGraph != nullptr && zoomedGraph->isVisible())
    {
        zoomedGraph->setBounds(knobsAreaRect.getUnion(graphAreaRect).reduced(2));
        zoomedGraph->toFront(false);
    }

    // --- Output card ---
    auto buttonArea = outputColumnArea.removeFromBottom(buttonAreaHeight);
    outputColumnArea.removeFromBottom(juce::jmin(controlGap, outputColumnArea.getHeight()));
    auto twoKnobsBounds = outputColumnArea.withSizeKeepingCentre(secondaryKnobSize * 2 + controlGap,
                                                                 secondaryKnobHeight);
    modulatableSliderComponents.at(OUTPUT_NAME)->setBounds(twoKnobsBounds.removeFromLeft(secondaryKnobSize));
    modulatableSliderComponents.at(MIX_NAME)->setBounds(twoKnobsBounds.removeFromRight(secondaryKnobSize));

    const int compactButtonGap = juce::jmax(3, juce::roundToInt(5.0f * uiScale));
    const int compactGroupWidth = juce::jmin(buttonArea.getWidth(), juce::roundToInt(184.0f * uiScale));
    auto compactButtonRow = buttonArea.withSizeKeepingCentre(compactGroupWidth, buttonAreaHeight);
    const int compactButtonWidth = juce::jmax(1, (compactButtonRow.getWidth() - compactButtonGap * 2) / 3);
    linkedButton.setBounds(compactButtonRow.removeFromLeft(compactButtonWidth));
    compactButtonRow.removeFromLeft(juce::jmin(compactButtonGap, compactButtonRow.getWidth()));
    safeButton.setBounds(compactButtonRow.removeFromLeft(compactButtonWidth));
    compactButtonRow.removeFromLeft(juce::jmin(compactButtonGap, compactButtonRow.getWidth()));
    extremeButton.setBounds(compactButtonRow);

    invalidateChromeCache();
}

void BandPanel::rebuildChromeCache(float displayScale)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    displayScale = juce::jmax(1.0f, displayScale);
    chromeCacheDisplayScale = displayScale;
    chromeCache = juce::Image(juce::Image::ARGB,
                              juce::jmax(1, juce::roundToInt(getWidth() * displayScale)),
                              juce::jmax(1, juce::roundToInt(getHeight() * displayScale)),
                              true);

    juce::Graphics cacheGraphics(chromeCache);
    cacheGraphics.addTransform(juce::AffineTransform::scale(displayScale));

    fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat());
    drawMinimalSurface(cacheGraphics, knobsAreaRect.getUnion(outputAreaRect).toFloat());

    const auto titleHeight = juce::roundToInt(22.0f * scale);
    const auto titleInset = juce::roundToInt(8.0f * scale);
    auto titleFor = [titleHeight, titleInset](juce::Rectangle<int> area)
    {
        area.reduce(titleInset, 0);
        return area.removeFromTop(juce::jmin(titleHeight, area.getHeight())).toFloat();
    };

    juce::String moduleTitle { "DRIVE" };
    juce::String graphTitle { "INPUT" };
    if (shapeSwitch.getToggleState())
    {
        moduleTitle = "SHAPE";
        graphTitle = "TRANSFER";
    }
    else if (compressorSwitch.getToggleState())
    {
        moduleTitle = "COMPRESSOR";
        graphTitle = "GAIN REDUCTION";
    }
    else if (widthSwitch.getToggleState())
    {
        moduleTitle = "STEREO";
        graphTitle = "WIDTH";
    }

    if (ottSwitch.getToggleState()) { moduleTitle = "OTT"; graphTitle = "DYNAMICS"; }

    if (selectedInsert >= 0) { moduleTitle = fire::effects::name(processor.getInsertEffectType(focusBandNum + 1, selectedInsert)); graphTitle = "OUTPUT"; }
    drawMinimalTitle(cacheGraphics, titleFor(tabAreaRect), "MODE");
    drawMinimalTitle(cacheGraphics, titleFor(knobsAreaRect), moduleTitle);
    drawMinimalTitle(cacheGraphics, titleFor(graphAreaRect), graphTitle);
    drawMinimalTitle(cacheGraphics,
                     titleFor(outputAreaRect),
                     "BAND " + juce::String(focusBandNum + 1));

    chromeCacheDirty = false;
}

void BandPanel::invalidateChromeCache()
{
    chromeCacheDirty = true;
    repaint();
}

void BandPanel::configureGraphInteractions()
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    const std::array<GraphTemplate*, 5> graphs {
        &oscilloscope, &distortionGraph, &vuPanel, &widthGraph, &ottGraph
    };

    for (auto* graph : graphs)
    {
        graph->setZoomRequestCallback([safeThis, graph]
        {
            if (safeThis != nullptr)
                safeThis->toggleGraphZoom(graph);
        });
    }
}

void BandPanel::toggleGraphZoom(GraphTemplate* graph)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    const std::array<GraphTemplate*, 5> graphs {
        &oscilloscope, &distortionGraph, &vuPanel, &widthGraph, &ottGraph
    };
    if (graph == nullptr
        || std::find(graphs.begin(), graphs.end(), graph) == graphs.end()
        || ! graph->isShowing())
        return;

    if (zoomedGraph == graph)
    {
        clearGraphZoom();
        if (safeThis != nullptr)
            safeThis->resized();
        return;
    }

    clearGraphZoom();
    if (safeThis == nullptr)
        return;

    zoomedGraph = graph;
    zoomedGraph->setZoomState(true);
    if (safeThis == nullptr)
        return;

    resized();
    if (safeThis != nullptr)
        safeThis->hideComponentsObscuredByZoom(*graph);
}

void BandPanel::clearGraphZoom() noexcept
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    const bool shouldRestoreDrivePreview =
        driveGraphPreviewPhase == DriveGraphPreviewPhase::restoreAfterZoom;
    auto* graph = zoomedGraph;
    zoomedGraph = nullptr;
    if (graph != nullptr)
        graph->setZoomState(false);

    if (safeThis != nullptr)
        safeThis->restoreComponentsObscuredByZoom();

    if (safeThis != nullptr && shouldRestoreDrivePreview
        && safeThis->driveGraphPreviewPhase
               == DriveGraphPreviewPhase::restoreAfterZoom)
        safeThis->restoreDriveGraphPreviewNow();
}

void BandPanel::hideComponentsObscuredByZoom(const GraphTemplate& graph)
{
    jassert(componentsHiddenForGraphZoom.empty());
    const auto cover = graph.getBounds();
    std::vector<juce::Component::SafePointer<juce::Component>> components;

    for (int index = 0; index < getNumChildComponents(); ++index)
    {
        auto* component = getChildComponent(index);
        if (component == &graph || component == nullptr
            || component->getBounds().isEmpty()
            || ! cover.contains(component->getBounds())
            || ! component->isVisible())
            continue;

        components.emplace_back(component);
    }

    componentsHiddenForGraphZoom = components;
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    for (auto& component : components)
    {
        if (component != nullptr)
            component->setVisible(false);
        if (safeThis == nullptr)
            return;
    }

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent(
            juce::AccessibilityEvent::structureChanged);
}

void BandPanel::restoreComponentsObscuredByZoom() noexcept
{
    if (componentsHiddenForGraphZoom.empty())
        return;

    auto components = std::move(componentsHiddenForGraphZoom);
    componentsHiddenForGraphZoom.clear();
    const juce::Component::SafePointer<BandPanel> safeThis(this);

    for (auto& component : components)
    {
        if (component != nullptr && component->getParentComponent() == this)
            component->setVisible(true);
        if (safeThis == nullptr)
            return;
    }

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent(
            juce::AccessibilityEvent::structureChanged);
}

GraphTemplate* BandPanel::getSelectedModuleGraph() noexcept
{
    if (selectedInsert >= 0) return &oscilloscope;
    if (ottSwitch.getToggleState()) return &ottGraph;
    if (shapeSwitch.getToggleState())
        return &distortionGraph;
    if (compressorSwitch.getToggleState())
        return &vuPanel;
    if (widthSwitch.getToggleState())
        return &widthGraph;
    return &oscilloscope;
}

void BandPanel::restoreDriveGraphPreviewNow() noexcept
{
    if (driveGraphPreviewPhase == DriveGraphPreviewPhase::idle)
        return;

    // Clear the session before changing visibility. Component listeners may
    // synchronously re-enter panel code, and a completed preview must be
    // observed as idle from that point onward.
    auto* const graphToRestore = graphBeforeDrivePreview;
    graphBeforeDrivePreview = nullptr;
    driveGraphPreviewPhase = DriveGraphPreviewPhase::idle;

    if (graphToRestore == nullptr || graphToRestore == &distortionGraph)
        return;

    const juce::Component::SafePointer<BandPanel> safeThis(this);
    distortionGraph.setVisible(false);
    if (safeThis != nullptr)
        graphToRestore->setVisible(true);
}

void BandPanel::setAnimatedModuleTarget(int moduleIndex)
{
    moduleIndex = juce::jlimit(0, 4, moduleIndex);
    const auto targetPosition = static_cast<float>(moduleIndex);
    if (juce::approximatelyEqual(moduleSelectionPosition.target, targetPosition))
        return;
    if (isShowing())
        moduleSelectionPosition.setTarget(targetPosition);
    else
        moduleSelectionPosition.snapTo(targetPosition);
    startContentTransition(knobsAreaRect.getUnion(graphAreaRect));
}

void BandPanel::selectInsertEffect(int slot)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    if (slot < 0 || processor.getInsertEffectType(focusBandNum + 1, slot) == fire::effects::Type::none)
    {
        selectedInsert = -1;
        oscSwitch.setToggleState(true, juce::sendNotificationSync);
        return;
    }
    dismissTransientInteraction();
    if (! safeThis) return;
    clearGraphZoom();
    if (! safeThis) return;
    selectedInsert = slot;
    effectNavigation.setSelectedSlot(slot);
    for (auto* group : {&driveComponents, &shapeComponents, &compressorComponents, &widthComponents, &ottComponents})
    {
        setVisibility(*group, false);
        if (! safeThis) return;
    }
    for (auto* graph : {static_cast<GraphTemplate*>(&distortionGraph), static_cast<GraphTemplate*>(&vuPanel),
                       static_cast<GraphTemplate*>(&widthGraph), static_cast<GraphTemplate*>(&ottGraph)})
    {
        graph->setVisible(false);
        if (! safeThis) return;
    }
    oscilloscope.setVisible(true);
    insertControls.bind(focusBandNum + 1, slot);
    if (! safeThis) return;
    insertControls.setActive(true);
    if (! safeThis) return;
    modulatableSliderComponents.at(OUTPUT_NAME)->setInteractionOnlyReadout(true);
    modulatableSliderComponents.at(MIX_NAME)->setInteractionOnlyReadout(true);
    startContentTransition(knobsAreaRect.getUnion(graphAreaRect));
    resized(); invalidateChromeCache();
    auto callback = onModuleChanged;
    if (callback) callback();
}

juce::Rectangle<float> BandPanel::getModuleSelectionBounds(float modulePosition) const
{
    const std::array<const juce::TextButton*, 5> switches {
        &oscSwitch, &shapeSwitch, &compressorSwitch, &widthSwitch, &ottSwitch
    };

    modulePosition = juce::jlimit(-0.2f, 4.2f, modulePosition);
    const auto lowerIndex = juce::jlimit(0, 3, static_cast<int>(std::floor(modulePosition)));
    const auto upperIndex = juce::jmin(4, lowerIndex + 1);
    const auto mix = modulePosition - static_cast<float>(lowerIndex);
    const auto* lowerButton = switches[static_cast<size_t>(lowerIndex)];
    const auto* upperButton = switches[static_cast<size_t>(upperIndex)];
    const auto lower = getLocalArea(lowerButton, lowerButton->getLocalBounds()).toFloat();
    const auto upper = getLocalArea(upperButton, upperButton->getLocalBounds()).toFloat();

    if (lower.isEmpty())
        return {};

    return { juce::jmap(mix, lower.getX(), upper.getX()),
             juce::jmap(mix, lower.getY(), upper.getY()),
             juce::jmap(mix, lower.getWidth(), upper.getWidth()),
             juce::jmap(mix, lower.getHeight(), upper.getHeight()) };
}

int BandPanel::getOttPreviewDirection() const
{
    if (! ottSwitch.getToggleState() || ! isShowing()) return 0;
    const bool up = modulatableSliderComponents.at(OTT_UPWARD_NAME)->isValueReadoutRequested();
    const bool down = modulatableSliderComponents.at(OTT_DOWNWARD_NAME)->isValueReadoutRequested();
    return (up ? 1 : 0) | (down ? 2 : 0);
}

void BandPanel::animationTick(float deltaSeconds)
{
    const juce::Component::SafePointer<BandPanel> safeOwner(this);
    effectNavigation.animationTick(deltaSeconds);
    if (! safeOwner) return;
    insertControls.refresh();
    if (! safeOwner) return;
    if (ottSwitch.getToggleState())
    {
        auto* rawUp = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_UPWARD_ID, focusBandNum));
        auto* rawDown = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_DOWNWARD_ID, focusBandNum));
        auto& up = *modulatableSliderComponents.at(OTT_UPWARD_NAME);
        auto& down = *modulatableSliderComponents.at(OTT_DOWNWARD_NAME);
        if (rawUp && rawDown && ! up.hasActiveInteraction() && ! down.hasActiveInteraction())
        {
            up.setValue(juce::jmin(rawUp->load(), rawDown->load() - 6.0f), juce::dontSendNotification);
            down.setValue(rawDown->load(), juce::dontSendNotification);
        }
        ottGraph.setThresholds(static_cast<float>(up.getValue()), static_cast<float>(down.getValue()));
    }
    if (lastOttMeterTimeMs >= 0.0 && juce::Time::getMillisecondCounterHiRes() - lastOttMeterTimeMs > 250.0)
        ottGraph.setLevels(-120.0f, 0.0f);
    if (isShowing() && ottSwitch.getToggleState())
    {
        bool reading = spectrumOttInteraction != 0;
        for (auto* name : ParameterIDAndName::ottControlNames)
            reading = reading || modulatableSliderComponents.at(name)->isValueReadoutRequested();
        ottGraph.advanceVisuals(deltaSeconds, reading, getOttPreviewDirection() | spectrumOttInteraction);
    }
    else ottGraph.resetVisuals();
    advanceContentTransition(deltaSeconds);
    if (! isShowing())
    {
        moduleSelectionPosition.snapTo(moduleSelectionPosition.target);
        return;
    }

    if (moduleSelectionPosition.isSettled())
        return;

    const auto oldBounds = getModuleSelectionBounds(moduleSelectionPosition.current);
    moduleSelectionPosition.advance(deltaSeconds);
    const auto newBounds = getModuleSelectionBounds(moduleSelectionPosition.current);

    repaint(oldBounds.getUnion(newBounds).expanded(3.0f * scale)
                .getSmallestIntegerContainer());
}

void BandPanel::setScale(float newScale)
{
    newScale = juce::jmax(0.25f, newScale);
    if (juce::approximatelyEqual(scale, newScale))
        return;

    scale = newScale;
    const std::array<GraphTemplate*, 5> graphs {
        &oscilloscope, &distortionGraph, &vuPanel, &widthGraph, &ottGraph
    };
    for (auto* graph : graphs)
        graph->setScale(newScale);

    resized();
}

void BandPanel::dismissButtonInteractions() noexcept
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    auto dismiss = [&safeThis](auto& button)
    {
        button.dismissPointerGesture();
        return safeThis != nullptr;
    };

    if (! dismiss(linkedButton))
        return;
    if (! dismiss(safeButton))
        return;
    if (! dismiss(extremeButton))
        return;
    if (! dismiss(oscSwitch))
        return;
    if (! dismiss(shapeSwitch))
        return;
    if (! dismiss(compressorSwitch))
        return;
    if (! dismiss(ottSwitch)) return;
    if (! dismiss(ottBypassButton)) return;
    if (! dismiss(widthSwitch))
        return;
    if (! dismiss(driveBypassButton))
        return;
    if (! dismiss(shapeBypassButton))
        return;
    if (! dismiss(compressorBypassButton))
        return;
    if (! dismiss(widthBypassButton))
        return;
    dismiss(dcFilterButton);
}

void BandPanel::dismissTransientInteraction() noexcept
{
    effectNavigation.dismiss();
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    for (auto* slider : modulatableSliders)
    {
        if (slider != nullptr)
            slider->dismissTransientInteraction();

        if (safeThis == nullptr)
            return;
    }

    dismissButtonInteractions();
    if (safeThis == nullptr)
        return;

    invalidateDistortionModeInteractions();
}

void BandPanel::dismissTransientInteractionForParameterRebind() noexcept
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    for (auto* slider : modulatableSliders)
    {
        if (slider != nullptr)
            slider->dismissTransientInteractionPreservingContextMenu();

        if (safeThis == nullptr)
            return;
    }

    dismissButtonInteractions();
    if (safeThis == nullptr)
        return;

    invalidateDistortionModeInteractions();
}

void BandPanel::invalidateDistortionModeInteractions() noexcept
{
    ++distortionModeInteractionGeneration;

    const juce::Component::SafePointer<BandPanel> safeThis(this);
    for (auto& modeBox : distortionModes)
    {
        modeBox.dismissTransientInteraction();
        if (safeThis == nullptr)
            return;
    }
}

bool BandPanel::canOpenDistortionModePopup(size_t modeIndex) const noexcept
{
    // Shape bypass skips rectification/bias/DC processing, but MODE still
    // selects the main waveshaper, so shapeBypassButton is intentionally not
    // part of this context predicate.
    return modeIndex < distortionModes.size()
           && focusBandNum == static_cast<int>(modeIndex)
           && shapeSwitch.getToggleState()
           && distortionModes[modeIndex].isVisible()
           && distortionModes[modeIndex].isEnabled()
           && isVisible();
}

void BandPanel::visibilityChanged()
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    juce::Component::visibilityChanged();

    if (safeThis != nullptr && ! isShowing())
    {
        dismissTransientInteraction();
        if (safeThis == nullptr)
            return;

        clearGraphZoom();
        if (safeThis != nullptr)
            safeThis->resized();
    }
}

void BandPanel::updateAttachments()
{
    updateIconButtonSemantics();

    for (const auto& paramInfo : ParameterIDAndName::getModulatableParameterInfo())
    {
        const auto found = modulatableSliderComponents.find(paramInfo.name);
        if (found == modulatableSliderComponents.end())
            continue;
        auto* slider = found->second.get();
        auto paramID = ParameterIDAndName::getIDString(paramInfo.idBase, focusBandNum);
        sliderAttachments[paramInfo.name].reset();
        slider->parameterID = paramID;
        auto* parameter = processor.treeState.getParameter(paramID);
        jassert(parameter != nullptr && "Parameter not found!");
        if (parameter)
        {
            sliderAttachments[paramInfo.name] = std::make_unique<SliderAttachment>(processor.treeState, paramID, *slider);
        }

        // The platform accessibility peer may cache a Slider value interface.
        // Recreate it only after the shared control has been fully rebound so
        // an old band's semantic target cannot leak into the new band.
        slider->invalidateAccessibilityHandler();
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
    ottAttachment.reset();

    linkedAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(LINKED_ID, focusBandNum), linkedButton);
    safeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SAFE_ID, focusBandNum), safeButton);
    extremeAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(EXTREME_ID, focusBandNum), extremeButton);

    driveBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, focusBandNum), driveBypassButton);
    shapeBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, focusBandNum), shapeBypassButton);
    compressorBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum), compressorBypassButton);
    widthBypassAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(WIDTH_BYPASS_ID, focusBandNum), widthBypassButton);

    ottAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(OTT_ENABLED_ID, focusBandNum), ottBypassButton);

    dcFilterAttachment = std::make_unique<ButtonAttachment>(processor.treeState, ParameterIDAndName::getIDString(DC_FILTER_ID, focusBandNum), dcFilterButton);

    const auto* bandEnabledParameter = processor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    const bool bandEnabled = bandEnabledParameter != nullptr && bandEnabledParameter->load() > 0.5f;
    setBandKnobsStates(bandEnabled, false);
}

void BandPanel::initFlatButton(juce::TextButton& button, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    button.setComponentID("rounded");
    button.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    button.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::raised);
    button.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    button.setColour(juce::TextButton::textColourOnId, fire::ui::colours::gold);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
    button.setButtonText(buttonName);
}

void BandPanel::initBypassButton(juce::ToggleButton& bypassButton, juce::Colour colour)
{
    addAndMakeVisible(bypassButton);
    bypassButton.setColour(juce::ToggleButton::tickColourId, colour);
    bypassButton.setColour(juce::ToggleButton::tickDisabledColourId, fire::ui::colours::disabled);
    bypassButton.addListener(this);
}

void BandPanel::buttonClicked(juce::Button* clickedButton)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    auto setGroupVisibility = [this, &safeThis](
                                  juce::Array<juce::Component*>& components,
                                  bool shouldBeVisible)
    {
        setVisibility(components, shouldBeVisible);
        return safeThis != nullptr;
    };
    auto setComponentVisibility = [&safeThis](juce::Component& component,
                                               bool shouldBeVisible)
    {
        component.setVisible(shouldBeVisible);
        return safeThis != nullptr;
    };

    if ((clickedButton == &oscSwitch && oscSwitch.getToggleState())
        || (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
        || (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
        || (clickedButton == &widthSwitch && widthSwitch.getToggleState())
        || (clickedButton == &ottSwitch && ottSwitch.getToggleState()))
    {
        clearGraphZoom();
        if (safeThis == nullptr)
            return;
    }

    bool isSwitch = false;
    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        setAnimatedModuleTarget(0);
        if (! setGroupVisibility(driveComponents, true))
            return;
        if (! setGroupVisibility(shapeComponents, false))
            return;
        if (! setGroupVisibility(compressorComponents, false))
            return;
        if (! setGroupVisibility(widthComponents, false))
            return;

        if (! setComponentVisibility(oscilloscope, true))
            return;
        if (! setComponentVisibility(distortionGraph, false))
            return;
        if (! setComponentVisibility(vuPanel, false))
            return;
        if (! setComponentVisibility(widthGraph, false))
            return;
        isSwitch = true;
    }
    else if (clickedButton == &shapeSwitch && shapeSwitch.getToggleState())
    {
        setAnimatedModuleTarget(1);
        if (! setGroupVisibility(driveComponents, false))
            return;
        if (! setGroupVisibility(shapeComponents, true))
            return;
        if (! setGroupVisibility(compressorComponents, false))
            return;
        if (! setGroupVisibility(widthComponents, false))
            return;

        if (! setComponentVisibility(oscilloscope, false))
            return;
        if (! setComponentVisibility(distortionGraph, true))
            return;
        if (! setComponentVisibility(vuPanel, false))
            return;
        if (! setComponentVisibility(widthGraph, false))
            return;
        isSwitch = true;
    }
    else if (clickedButton == &compressorSwitch && compressorSwitch.getToggleState())
    {
        setAnimatedModuleTarget(2);
        if (! setGroupVisibility(driveComponents, false))
            return;
        if (! setGroupVisibility(shapeComponents, false))
            return;
        if (! setGroupVisibility(compressorComponents, true))
            return;
        if (! setGroupVisibility(widthComponents, false))
            return;

        if (! setComponentVisibility(oscilloscope, false))
            return;
        if (! setComponentVisibility(distortionGraph, false))
            return;
        if (! setComponentVisibility(vuPanel, true))
            return;
        if (! setComponentVisibility(widthGraph, false))
            return;
        isSwitch = true;
    }
    else if (clickedButton == &widthSwitch && widthSwitch.getToggleState())
    {
        setAnimatedModuleTarget(3);
        if (! setGroupVisibility(driveComponents, false))
            return;
        if (! setGroupVisibility(shapeComponents, false))
            return;
        if (! setGroupVisibility(compressorComponents, false))
            return;
        if (! setGroupVisibility(widthComponents, true))
            return;

        if (! setComponentVisibility(oscilloscope, false))
            return;
        if (! setComponentVisibility(distortionGraph, false))
            return;
        if (! setComponentVisibility(vuPanel, false))
            return;
        if (! setComponentVisibility(widthGraph, true))
            return;
        isSwitch = true;
    }

    else if (clickedButton == &ottSwitch && ottSwitch.getToggleState())
    {
        setAnimatedModuleTarget(4);
        if (! setGroupVisibility(driveComponents, false)
            || ! setGroupVisibility(shapeComponents, false)
            || ! setGroupVisibility(compressorComponents, false)
            || ! setGroupVisibility(widthComponents, false)) return;
        if (! setComponentVisibility(oscilloscope, false)
            || ! setComponentVisibility(distortionGraph, false)
            || ! setComponentVisibility(vuPanel, false)
            || ! setComponentVisibility(widthGraph, false)) return;
        isSwitch = true;
    }
    if (isSwitch)
    {
        if (! setGroupVisibility(ottComponents, ottSwitch.getToggleState())
            || ! setComponentVisibility(ottGraph, ottSwitch.getToggleState())) return;
    }

    updateDistortionModeVisibility();
    if (safeThis == nullptr)
        return;

    // Handle clicks from any of the bypass buttons.
    if (clickedButton == &driveBypassButton || clickedButton == &shapeBypassButton || clickedButton == &compressorBypassButton || clickedButton == &widthBypassButton || clickedButton == &ottBypassButton)
    {
        const auto* bandEnabledParameter = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
        const bool isBandEnabled = bandEnabledParameter != nullptr
                                   && bandEnabledParameter->load() > 0.5f;
        setBandKnobsStates(isBandEnabled, true);
        return;
    }

    if (isSwitch)
    {
        selectedInsert = -1;
        effectNavigation.setSelectedSlot(-1);
        insertControls.setActive(false);
        if (! safeThis) return;
        modulatableSliderComponents.at(OUTPUT_NAME)->setInteractionOnlyReadout(ottSwitch.getToggleState());
        modulatableSliderComponents.at(MIX_NAME)->setInteractionOnlyReadout(ottSwitch.getToggleState());
        resized();
        if (safeThis == nullptr)
            return;

        invalidateChromeCache();
        auto callback = onModuleChanged;
        if (callback) callback();
    }
}

void BandPanel::setFocusBandNum(int num, bool forceUpdate)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);

    if (! juce::isPositiveAndBelow(num, 4))
    {
        jassertfalse;
        return;
    }

    // Every shared knob is backed by a SliderAttachment. Rebinding one while
    // it is down destroys the listener that owns the old begin gesture; the
    // replacement then receives an unmatched end gesture for another band.
    // A modulation-handle drag has the same target-switch problem because its
    // callback reads the knob's current parameter ID on every move.
    if (hasActiveSliderInteraction())
    {
        if (focusBandNum == num && ! forceUpdate)
        {
            pendingFocusBandNum = -1;
            pendingFocusForceUpdate = false;
        }
        else
        {
            pendingFocusBandNum = num;
            pendingFocusForceUpdate = pendingFocusForceUpdate || forceUpdate;
        }
        return;
    }

    pendingFocusBandNum = -1;
    pendingFocusForceUpdate = false;

    vuPanel.setFocusBandNum(num);
    if (safeThis == nullptr)
        return;

    if (focusBandNum == num && ! forceUpdate)
        return;

    // Shared controls must not carry a pointer, text-entry, or hover session
    // across attachment targets. A late release or value-editor commit would
    // otherwise reach the replacement attachment for the newly focused band.
    // An already chosen context-menu command is different: its handler froze
    // the old parameter ID when the menu opened, so preserve that one session.
    dismissTransientInteractionForParameterRebind();

    if (safeThis == nullptr)
        return;

    processor.setUiFocusBand(num);
    if (safeThis == nullptr)
        return;

    focusBandNum = num;
    effectNavigation.setScope(num + 1);
    if (! safeThis) return;
    if (selectedInsert >= 0) selectInsertEffect(selectedInsert);
    else insertControls.bind(num + 1, 0);
    if (! safeThis) return;
    lastGraphTelemetryTimeMs = -1.0;
    ottGraph.setLevels(-120.0f, 0.0f);
    ottGraph.resetVisuals();
    lastOttMeterTimeMs = -1.0;
    updateAttachments();
    if (safeThis == nullptr)
        return;

    updateWhenChangingFocus();
    if (safeThis == nullptr)
        return;

    const auto* bandEnabledParameter = processor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    const bool isBandEnabled = bandEnabledParameter != nullptr
                               && bandEnabledParameter->load() > 0.5f;
    setBandKnobsStates(isBandEnabled, false);
    if (safeThis == nullptr)
        return;

    updateDistortionModeVisibility();
    if (safeThis == nullptr)
        return;

    updateDistortionGraphFromParameters();
    if (safeThis == nullptr)
        return;

    invalidateChromeCache();
}

bool BandPanel::hasActiveSliderInteraction() const noexcept
{
    return std::any_of(modulatableSliders.begin(),
                       modulatableSliders.end(),
                       [](const auto* slider)
                       {
                           return slider != nullptr && slider->hasActiveInteraction();
                       });
}

void BandPanel::applyPendingFocusChange()
{
    if (pendingFocusBandNum < 0 || hasActiveSliderInteraction())
        return;

    const auto requestedBand = pendingFocusBandNum;
    const auto forceUpdate = pendingFocusForceUpdate;
    pendingFocusBandNum = -1;
    pendingFocusForceUpdate = false;
    setFocusBandNum(requestedBand, forceUpdate);
}

void BandPanel::updateDistortionGraphFromParameters()
{
    const auto readBandParameter = [this](const juce::String& baseId, float fallback)
    {
        if (const auto* value = processor.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(baseId, focusBandNum)))
            return value->load();

        return fallback;
    };

    DistortionGraphValues values;
    const bool shapeEnabled = readBandParameter(SHAPE_BYPASS_ID, 0.0f) > 0.5f;
    values.rec = shapeEnabled ? readBandParameter(REC_ID, 0.0f) : 0.0f;
    values.mix = readBandParameter(MIX_ID, 1.0f)
                 * (shapeEnabled ? readBandParameter(SHAPE_MIX_ID, 1.0f) : 1.0f);
    values.bias = shapeEnabled ? readBandParameter(BIAS_ID, 0.0f) : 0.0f;
    values.mode = juce::roundToInt(readBandParameter(MODE_ID, 0.0f));

    const bool driveEnabled = readBandParameter(DRIVE_BYPASS_ID, 1.0f) > 0.5f;
    float driveAmount = driveEnabled ? readBandParameter(DRIVE_ID, 0.0f) : 0.0f;
    if (driveEnabled && readBandParameter(EXTREME_ID, 0.0f) > 0.5f)
        driveAmount *= std::log2(10.0f);
    const float driveForCalc = driveAmount * 6.5f / 100.0f;
    const float powerDrive = std::pow(2.0f, driveForCalc);
    const float sampleMaxValue = processor.getSampleMaxValue(focusBandNum);
    const bool safeMode = readBandParameter(SAFE_ID, 1.0f) > 0.5f;
    values.drive = driveEnabled && safeMode
                           && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f
                       ? 2.0f / sampleMaxValue + 0.1f * driveForCalc
                       : powerDrive;

    const auto* downsample = processor.treeState.getRawParameterValue(DOWNSAMPLE_ID);
    const auto* downsampleBypass = processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID);
    const bool downsampleEnabled = downsampleBypass != nullptr
                                && downsampleBypass->load() > 0.5f;
    values.rateDivide = downsampleEnabled && downsample != nullptr
                            ? downsample->load()
                            : 1.0f;

    distortionGraph.setState(values.mode,
                             values.rec,
                             values.mix,
                             values.bias,
                             values.drive,
                             values.rateDivide);
}

void BandPanel::updateDriveMeter()
{
    if (! isShowing())
        return;

    if (auto* lnf = dynamic_cast<FireLookAndFeel*>(&getLookAndFeel()))
    {
        auto sampleMax = processor.getSampleMaxValue(focusBandNum);
        auto reduction = safeButton.getToggleState()
                             ? processor.getReductionPrecent(focusBandNum)
                             : 1.0f;
        sampleMax = std::isfinite(sampleMax) ? juce::jmax(0.0f, sampleMax) : 0.0f;
        reduction = std::isfinite(reduction) ? juce::jlimit(0.0f, 1.0f, reduction) : 1.0f;

        const bool changed = std::abs(lnf->sampleMaxValue - sampleMax) > 0.0005f
                             || std::abs(lnf->reductionPercent - reduction) > 0.0005f;
        lnf->sampleMaxValue = sampleMax;
        lnf->reductionPercent = reduction;

        if (changed)
            if (auto* driveKnob = getDriveKnob(); driveKnob != nullptr && driveKnob->isShowing())
                driveKnob->repaint();
    }
}

void BandPanel::setVisibility(juce::Array<juce::Component*>& components, bool isVisible)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    for (auto* component : components)
    {
        if (component == nullptr)
            continue;

        if (! dismissInteractionBeforeComponentStateChange(
                component, component->isVisible() != isVisible, safeThis))
            return;

        component->setVisible(isVisible);

        if (safeThis == nullptr)
            return;
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

    return ottComponents.contains(&component);
}

void BandPanel::setBandKnobsStates(bool isBandEnabled, bool /*callFromSubBypass*/)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    auto setComponentEnabled = [&safeThis](juce::Component* component,
                                            bool shouldBeEnabled)
    {
        if (component == nullptr)
            return safeThis != nullptr;

        if (! dismissInteractionBeforeComponentStateChange(
                component,
                component->isEnabled() != shouldBeEnabled,
                safeThis))
            return false;

        component->setEnabled(shouldBeEnabled);
        return safeThis != nullptr;
    };

    if (! isBandEnabled)
    {
        invalidateDistortionModeInteractions();
        if (safeThis == nullptr)
            return;
    }

    for (auto* component : allControls)
    {
        if (! setComponentEnabled(component, isBandEnabled))
            return;
    }

    if (isBandEnabled)
    {
        bool driveIsEnabled = driveBypassButton.getToggleState();
        for (auto* component : driveComponents)
        {
            if (! setComponentEnabled(component, driveIsEnabled))
                return;
        }

        bool shapeIsEnabled = shapeBypassButton.getToggleState();
        for (auto* component : shapeComponents)
        {
            if (! setComponentEnabled(component, shapeIsEnabled))
                return;
        }

        bool compIsEnabled = compressorBypassButton.getToggleState();
        for (auto* component : compressorComponents)
        {
            if (! setComponentEnabled(component, compIsEnabled))
                return;
        }

        bool widthIsEnabled = widthBypassButton.getToggleState();
        for (auto* component : widthComponents)
        {
            if (! setComponentEnabled(component, widthIsEnabled))
                return;
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
    else if (index == 4)
        ottSwitch.setToggleState(state, juce::sendNotificationSync);
}

void BandPanel::updateWhenChangingFocus()
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    updateDriveMeter();
    if (safeThis == nullptr)
        return;

    buttonClicked(&oscSwitch);
    if (safeThis == nullptr)
        return;

    buttonClicked(&shapeSwitch);
    if (safeThis == nullptr)
        return;

    buttonClicked(&compressorSwitch);
    if (safeThis == nullptr)
        return;

    buttonClicked(&widthSwitch);
    if (safeThis == nullptr)
        return;

    buttonClicked(&ottSwitch);
    if (safeThis == nullptr) return;

    updateDistortionModeVisibility();
    if (safeThis == nullptr)
        return;

    invalidateChromeCache();
}

void BandPanel::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);

    for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
    {
        const auto index = static_cast<size_t>(bandIndex);
        const auto bandMask = 1u << static_cast<unsigned int>(bandIndex);
        const bool isDriveParameter = parameterID == driveParameterIds[index];

        bool affectsGraph = isDriveParameter;
        for (const auto& parameterIds : distortionGraphParameterIds)
            affectsGraph = affectsGraph || parameterID == parameterIds[index];

        if (affectsGraph)
        {
            distortionGraphDirtyMask.fetch_or(bandMask, std::memory_order_release);
            return;
        }
    }
}

void BandPanel::timerCallback()
{
    // The transfer curve is presentation-only. Keep its dirty bits pending
    // while hidden and rebuild just that graph once it can actually be seen.
    if (! isShowing() || ! distortionGraph.isShowing())
        return;

    // A stopped DAW can still process audio and run free LFOs. While DSP
    // packets are arriving, never interleave their modulated transfer curve
    // with an unmodulated APVTS preview. Keep dirty edits pending for when
    // audio processing actually stops, rather than using transport state.
    if (lastGraphTelemetryTimeMs >= 0.0
        && juce::Time::getMillisecondCounterHiRes() - lastGraphTelemetryTimeMs
               < graphTelemetryTimeoutMs)
        return;

    const auto graphDirtyMask = distortionGraphDirtyMask.exchange(0, std::memory_order_acq_rel);
    if (juce::isPositiveAndBelow(focusBandNum, 4)
        && (graphDirtyMask & (1u << static_cast<unsigned int>(focusBandNum))) != 0)
        updateDistortionGraphFromParameters();
}

void BandPanel::presentDistortionGraphValues(const DistortionGraphValues& values)
{
    // The processor has already rejected stale source generations. This
    // additional band check covers a deferred UI attachment rebind.
    if (static_cast<int>(values.sourceToken & 0x3u) != focusBandNum)
        return;

    lastGraphTelemetryTimeMs = juce::Time::getMillisecondCounterHiRes();
    distortionGraph.setState(values.mode, values.rec, values.mix, values.bias,
                             values.drive, values.rateDivide);
}

void BandPanel::setMenu(juce::ComboBox* combobox)
{
    addAndMakeVisible(combobox);
    combobox->setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface1);
    combobox->setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    combobox->setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);
    combobox->setColour(juce::ComboBox::arrowColourId, fire::ui::colours::shape);
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
}

void BandPanel::updateDistortionModeVisibility()
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    invalidateDistortionModeInteractions();
    if (safeThis == nullptr)
        return;

    const bool shouldShowAny = shapeSwitch.getToggleState();

    for (size_t i = 0; i < distortionModes.size(); ++i)
    {
        distortionModes[i].setVisible(shouldShowAny
                                      && focusBandNum == static_cast<int>(i));
        if (safeThis == nullptr)
            return;
    }
}

void BandPanel::updateRealtimeThreshold(float newThreshold)
{
    vuPanel.updateRealtimeThreshold(newThreshold);
}

void BandPanel::presentMeterValues(const MeterValues& values,
                                   std::uint64_t generation)
{
    vuPanel.presentMeterValues(values, generation);
    const auto index = static_cast<size_t>(juce::jlimit(0, 3, focusBandNum));
    if (generation != lastOttMeterGeneration)
    {
        lastOttMeterGeneration = generation;
        lastOttMeterTimeMs = juce::Time::getMillisecondCounterHiRes();
        ottGraph.setLevels(values.bandLevelsAreFresh ? values.ottInputLevelDb[index] : -120.0f, values.bandLevelsAreFresh ? values.ottGainChangeDb[index] : 0.0f, values.bandLevelsAreFresh ? values.ottDynamicsActivityDb[index] : 0.0f);
    }
    ottGraph.setThresholds(static_cast<float>(modulatableSliderComponents.at(OTT_UPWARD_NAME)->getValue()), static_cast<float>(modulatableSliderComponents.at(OTT_DOWNWARD_NAME)->getValue()));
}

void BandPanel::setGraphVisibilityForDriveDrag(bool isDragging)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    if (isDragging)
    {
        // A Slider emits one main-drag start per accepted gesture. Ignore a
        // duplicate notification rather than replacing the graph owned by the
        // current preview session with its temporary transfer graph.
        if (driveGraphPreviewPhase != DriveGraphPreviewPhase::idle)
            return;

        graphBeforeDrivePreview = getSelectedModuleGraph();
        driveGraphPreviewPhase = DriveGraphPreviewPhase::previewing;

        if (graphBeforeDrivePreview != &distortionGraph)
        {
            if (graphBeforeDrivePreview != nullptr)
            {
                graphBeforeDrivePreview->setVisible(false);
                if (safeThis == nullptr)
                    return;
            }

            distortionGraph.setVisible(true);
            if (safeThis == nullptr)
                return;
        }
    }
    else
    {
        if (driveGraphPreviewPhase == DriveGraphPreviewPhase::idle
            || driveGraphPreviewPhase
                   == DriveGraphPreviewPhase::restoreAfterZoom)
            return;

        // Expanding the temporary transfer graph hides the Drive control. Its
        // visibility lifecycle synchronously ends the Slider gesture while the
        // graph is becoming zoomed. Keep the zoom owner visible and defer the
        // preview restoration until clearGraphZoom() has first restored the
        // controls hidden by that zoom session.
        if (zoomedGraph == &distortionGraph
            && distortionGraph.getZoomState())
        {
            driveGraphPreviewPhase = DriveGraphPreviewPhase::restoreAfterZoom;
            return;
        }

        restoreDriveGraphPreviewNow();
    }
}
