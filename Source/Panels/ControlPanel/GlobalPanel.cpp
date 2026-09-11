/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GlobalPanel.h"
#include "../../GUI/FireTheme.h"
#include "../../Utility/AudioHelpers.h"
#include <algorithm>
#include <array>

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
    configureGraphInteractions();

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

    // Set initial switch state and trigger visibility update using buttonClicked
    filterSwitch.setToggleState(true, juce::dontSendNotification);
    buttonClicked(&filterSwitch);
}

GlobalPanel::~GlobalPanel()
{
    dismissTransientInteraction();

    filterSwitch.removeListener(this);
    downsampleSwitch.removeListener(this);
    graphSwitch.removeListener(this);
    filterLowCutButton.removeListener(this);
    filterPeakButton.removeListener(this);
    filterHighCutButton.removeListener(this);
}

void GlobalPanel::dismissTransientInteraction() noexcept
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    for (auto* slider : modulatableSliders)
    {
        if (slider != nullptr)
            slider->dismissTransientInteraction();

        if (safeThis == nullptr)
            return;
    }

    auto dismiss = [&safeThis](auto& button)
    {
        button.dismissPointerGesture();
        return safeThis != nullptr;
    };

    if (! dismiss(filterLowCutButton))
        return;
    if (! dismiss(filterPeakButton))
        return;
    if (! dismiss(filterHighCutButton))
        return;
    if (! dismiss(filterSwitch))
        return;
    if (! dismiss(downsampleSwitch))
        return;
    if (! dismiss(graphSwitch))
        return;
    if (filterBypassButton != nullptr)
    {
        filterBypassButton->dismissPointerGesture();
        if (safeThis == nullptr)
            return;
    }
    if (downsampleBypassButton != nullptr)
    {
        downsampleBypassButton->dismissPointerGesture();
        if (safeThis == nullptr)
            return;
    }

    invalidateSlopeInteractions();
}

void GlobalPanel::invalidateSlopeInteractions() noexcept
{
    ++slopeInteractionGeneration;
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    lowcutSlopeMode.dismissTransientInteraction();
    if (safeThis == nullptr)
        return;

    highcutSlopeMode.dismissTransientInteraction();
}

bool GlobalPanel::canOpenSlopePopup(bool lowCut) const noexcept
{
    const auto& slopeMode = lowCut ? lowcutSlopeMode : highcutSlopeMode;
    const bool correctFilterType = lowCut
                                       ? filterLowCutButton.getToggleState()
                                             && ! filterPeakButton.getToggleState()
                                             && ! filterHighCutButton.getToggleState()
                                       : filterHighCutButton.getToggleState()
                                             && ! filterLowCutButton.getToggleState()
                                             && ! filterPeakButton.getToggleState();

    return filterBypassButton != nullptr
           && filterBypassButton->getToggleState()
           && filterSwitch.getToggleState()
           && ! downsampleSwitch.getToggleState()
           && ! graphSwitch.getToggleState()
           && correctFilterType
           && slopeMode.isShowing()
           && slopeMode.isEnabled();
}

void GlobalPanel::visibilityChanged()
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
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

void GlobalPanel::enablementChanged()
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    juce::Component::enablementChanged();
    if (safeThis == nullptr)
        return;

    if (! isEnabled())
        invalidateSlopeInteractions();
}

void GlobalPanel::presentMeterValues(const MeterValues& values,
                                     std::uint64_t generation)
{
    vuPanel.presentMeterValues(values, generation);
}

void GlobalPanel::createSliders()
{
    // Global Knobs
    createAndConfigureSlider(GLOBAL_OUTPUT_NAME, "Output", fire::ui::colours::flame, " dB");
    createAndConfigureSlider(GLOBAL_MIX_NAME, "Mix", fire::ui::colours::gold);

    // Lo-fi section sliders
    const auto lofiColour = fire::ui::colours::loFi;
    createAndConfigureSlider(DOWNSAMPLE_NAME, "Rate", lofiColour);
    createAndConfigureSlider(BIT_DEPTH_NAME, "Bits", lofiColour, " bit");
    createAndConfigureSlider(JITTER_NAME, "Jitter", lofiColour);
    createAndConfigureSlider(DOWNSAMPLE_MIX_NAME, "Mix", lofiColour);

    // Filter Knobs
    const auto filterColour = fire::ui::colours::filter;
    createAndConfigureSlider(LOWCUT_FREQ_NAME, "Frequency", filterColour, " Hz");
    createAndConfigureSlider(LOWCUT_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(LOWCUT_GAIN_NAME, "Gain", filterColour, " dB");
    createAndConfigureSlider(HIGHCUT_FREQ_NAME, "Frequency", filterColour, " Hz");
    createAndConfigureSlider(HIGHCUT_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(HIGHCUT_GAIN_NAME, "Gain", filterColour, " dB");
    createAndConfigureSlider(PEAK_FREQ_NAME, "Frequency", filterColour, " Hz");
    createAndConfigureSlider(PEAK_Q_NAME, "Q", filterColour);
    createAndConfigureSlider(PEAK_GAIN_NAME, "Gain", filterColour, " dB");
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
        label.setColour(juce::Label::textColourId, fire::ui::colours::textSecondary);
        juce::ignoreUnused(colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupLabel(filterTypeLabel, "Type", fire::ui::colours::filter);
    setupLabel(lowcutSlopeLabel, "Slope", fire::ui::colours::filter);
    setupLabel(highcutSlopeLabel, "Slope", fire::ui::colours::filter);
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

        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textSecondary);
        juce::ignoreUnused(colour);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, fire::ui::colours::textPrimary);
        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        btn.getProperties().set("fireAnimatedSelection", true);
        btn.getProperties().set("fireModuleRail", true);

        btn.addListener(this);
    };

    setupSwitch(filterSwitch, "Filter", fire::ui::colours::filter);
    setupSwitch(downsampleSwitch, "Lo-Fi", fire::ui::colours::loFi);
    setupSwitch(graphSwitch, "Analysis", fire::ui::colours::signalCool);

    filterBypassButton = std::make_unique<PrimaryToggleButton>();
    initBypassButton(*filterBypassButton, fire::ui::colours::filter);
    filterBypassButton->onClick = [this]
    { setBypassState(0, filterBypassButton->getToggleState()); };

    downsampleBypassButton = std::make_unique<PrimaryToggleButton>();
    initBypassButton(*downsampleBypassButton, fire::ui::colours::loFi);
    downsampleBypassButton->onClick = [this]
    { setBypassState(1, downsampleBypassButton->getToggleState()); };

    setRoundButton(filterLowCutButton, LOW_ID, "");
    setRoundButton(filterPeakButton, BAND_ID, "");
    setRoundButton(filterHighCutButton, HIGH_ID, "");

    filterLowCutButton.setComponentID("low_cut");
    filterPeakButton.setComponentID("band_pass");
    filterHighCutButton.setComponentID("high_cut");

    const auto setSemantics = [](juce::Button& button,
                                 const juce::String& title,
                                 const juce::String& help)
    {
        button.setTitle(title);
        button.setTooltip(help);
    };

    setSemantics(*filterBypassButton,
                 "Global filter power",
                 "Enable or bypass the global filter");
    setSemantics(*downsampleBypassButton,
                 "Global Lo-Fi power",
                 "Enable or bypass global Lo-Fi processing");
    setSemantics(filterLowCutButton,
                 "Low-cut filter type",
                 "Select the low-cut filter type");
    setSemantics(filterPeakButton,
                 "Band-pass filter type",
                 "Select the band-pass filter type");
    setSemantics(filterHighCutButton,
                 "High-cut filter type",
                 "Select the high-cut filter type");

    filterLowCutButton.setRadioGroupId(filterModeButtons);
    filterPeakButton.setRadioGroupId(filterModeButtons);
    filterHighCutButton.setRadioGroupId(filterModeButtons);
}

void GlobalPanel::createComboBoxes()
{
    addAndMakeVisible(lowcutSlopeMode);
    lowcutSlopeMode.setTitle("Low-cut filter slope");
    lowcutSlopeMode.setTooltip("Select the low-cut filter slope");
    lowcutSlopeMode.addItem("12 db", 1);
    lowcutSlopeMode.addItem("24 db", 2);
    lowcutSlopeMode.addItem("36 db", 3);
    lowcutSlopeMode.addItem("48 db", 4);

    addAndMakeVisible(highcutSlopeMode);
    highcutSlopeMode.setTitle("High-cut filter slope");
    highcutSlopeMode.setTooltip("Select the high-cut filter slope");
    highcutSlopeMode.addItem("12 db", 1);
    highcutSlopeMode.addItem("24 db", 2);
    highcutSlopeMode.addItem("36 db", 3);
    highcutSlopeMode.addItem("48 db", 4);

    for (auto* menu : { &lowcutSlopeMode, &highcutSlopeMode })
    {
        menu->setColour(juce::ComboBox::backgroundColourId, fire::ui::colours::surface1);
        menu->setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
        menu->setColour(juce::ComboBox::textColourId, fire::ui::colours::textPrimary);
        menu->setColour(juce::ComboBox::arrowColourId, fire::ui::colours::filter);
    }

    auto* const lowcutSlopeParameter =
        processor.treeState.getParameter(LOWCUT_SLOPE_ID);
    auto* const highcutSlopeParameter =
        processor.treeState.getParameter(HIGHCUT_SLOPE_ID);
    jassert(lowcutSlopeParameter != nullptr);
    jassert(highcutSlopeParameter != nullptr);

    lowcutSlopeMode.configurePopupSession(
        [this]
        {
            return slopeInteractionGeneration;
        },
        [this]
        {
            return canOpenSlopePopup(true);
        },
        lowcutSlopeParameter);
    highcutSlopeMode.configurePopupSession(
        [this]
        {
            return slopeInteractionGeneration;
        },
        [this]
        {
            return canOpenSlopePopup(false);
        },
        highcutSlopeParameter);
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
    bypassButton.setColour(juce::ToggleButton::tickDisabledColourId, fire::ui::colours::disabled);
}

void GlobalPanel::setRoundButton(juce::TextButton& button, juce::String, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, fire::ui::colours::surface1);
    button.setColour(juce::TextButton::buttonOnColourId, fire::ui::colours::raised);
    button.setColour(juce::ComboBox::outlineColourId, fire::ui::colours::hairline);
    button.setColour(juce::TextButton::textColourOnId, fire::ui::colours::filter);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::colours::textMuted);
    if (button.getComponentID().isEmpty())
    {
        button.setComponentID("rounded");
        button.setButtonText(buttonName);
    }
    button.addListener(this);
}

void GlobalPanel::paint(juce::Graphics& g)
{
    const auto displayScale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (chromeCacheDirty
        || ! juce::approximatelyEqual(chromeCacheDisplayScale, displayScale)
        || chromeCache.isNull())
        rebuildChromeCache(displayScale);

    if (! chromeCache.isNull())
        g.drawImage(chromeCache, getLocalBounds().toFloat());

    if (selectionAnimationInitialised)
    {
        if (auto* selectedSwitch = getSelectedSwitch())
        {
            auto selectionBounds = selectedSwitch->getBounds().toFloat().reduced(
                juce::jmax(0.5f, 1.0f * scale));
            selectionBounds.setY(selectionY.current);

            const auto radius = juce::jmin(selectionBounds.getHeight() * 0.5f,
                                           fire::ui::Metrics::radius * scale);

            g.setColour(fire::ui::colours::raised);
            g.fillRoundedRectangle(selectionBounds, radius);

        }
    }
}

void GlobalPanel::resized()
{
    const float uiScale = scale;
    // Drive is the sole hero control; every other rotary in the editor uses
    // this quieter common diameter.
    const int scaledKnobSize = juce::roundToInt(fire::ui::Metrics::knobWidth * uiScale);
    const int outerPadding = juce::roundToInt(10.0f * uiScale);
    const int gap = juce::roundToInt(juce::jlimit(7.0f * uiScale,
                                                 14.0f * uiScale,
                                                 static_cast<float>(getWidth()) * 0.01f));
    const int cardPadding = juce::roundToInt(8.0f * uiScale);
    const int titleHeight = juce::roundToInt(22.0f * uiScale);
    const int controlGap = juce::roundToInt(8.0f * uiScale);

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

    tabAreaRect = layoutArea.removeFromLeft(juce::jmin(navWidth, layoutArea.getWidth()));
    layoutArea.removeFromLeft(juce::jmin(gap, layoutArea.getWidth()));
    outputAreaRect = layoutArea.removeFromRight(juce::jmin(outputWidth, layoutArea.getWidth()));
    layoutArea.removeFromRight(juce::jmin(gap, layoutArea.getWidth()));
    controlsAreaRect = layoutArea;

    auto contentArea = [cardPadding, titleHeight](juce::Rectangle<int> card)
    {
        card.reduce(cardPadding, cardPadding);
        card.removeFromTop(juce::jmin(titleHeight, card.getHeight()));
        return card;
    };

    auto switchColumnArea = contentArea(tabAreaRect);
    auto knobsColumnArea = contentArea(controlsAreaRect);
    auto outputColumnArea = contentArea(outputAreaRect);

    const int filterUtilityWidth = juce::jlimit(juce::roundToInt(110.0f * uiScale),
                                                juce::roundToInt(155.0f * uiScale),
                                                juce::roundToInt(knobsColumnArea.getWidth() * 0.27f));
    const int filterKnobWidth = juce::jmax(1,
        knobsColumnArea.getWidth() - juce::jmin(filterUtilityWidth, knobsColumnArea.getWidth()) - controlGap);
    const int filterKnobLimit = juce::jmax(1, (filterKnobWidth - controlGap * 2) / 3);
    const int lofiKnobLimit = juce::jmax(1, (knobsColumnArea.getWidth() - controlGap * 3) / 4);
    const int valueHeight = juce::roundToInt(fire::ui::Metrics::knobValueHeight * uiScale);
    const int masterKnobLimit = juce::jmax(1,
        (outputColumnArea.getWidth() - controlGap) / 2);
    const int ordinaryKnobSize = juce::jmax(1, std::min({ scaledKnobSize,
                                                          filterKnobLimit,
                                                          lofiKnobLimit,
                                                          masterKnobLimit,
                                                          knobsColumnArea.getHeight() - valueHeight }));
    const int ordinaryKnobHeight = ordinaryKnobSize + valueHeight;

    // --- Process rail ---
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    switchColumnBox.alignItems = juce::FlexBox::AlignItems::stretch;
    const auto rowMargin = juce::FlexItem::Margin(juce::jmax(1.0f, 2.0f * uiScale));
    switchColumnBox.items.add(juce::FlexItem(filterSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.items.add(juce::FlexItem(downsampleSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.items.add(juce::FlexItem(graphSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.performLayout(switchColumnArea);

    auto layoutBypassButton = [&](juce::ToggleButton& bypass, const juce::TextButton& parentSwitch)
    {
        auto parentBounds = parentSwitch.getBounds();
        const int bypassSize = juce::roundToInt(juce::jlimit(16.0f * uiScale,
                                                            24.0f * uiScale,
                                                            parentBounds.getHeight() * 0.46f));

        bypass.setBounds(parentBounds.getX() + juce::roundToInt(7.0f * uiScale),
                         parentBounds.getCentreY() - (bypassSize / 2),
                         bypassSize,
                         bypassSize);
        bypass.toFront(false);
    };

    layoutBypassButton(*filterBypassButton, filterSwitch);
    layoutBypassButton(*downsampleBypassButton, downsampleSwitch);

    updateSelectionTarget(! selectionAnimationInitialised);

    // --- Master output card ---
    const int footerReserve = juce::roundToInt(36.0f * uiScale);
    outputColumnArea.removeFromBottom(juce::jmin(footerReserve, outputColumnArea.getHeight()));
    auto masterKnobs = outputColumnArea.withSizeKeepingCentre(ordinaryKnobSize * 2 + controlGap,
                                                              ordinaryKnobHeight);
    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setBounds(masterKnobs.removeFromLeft(ordinaryKnobSize));
    masterKnobs.removeFromLeft(controlGap);
    modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setBounds(masterKnobs);

    // --- Main reactor card ---
    if (filterSwitch.getToggleState())
    {
        auto controlArea = knobsColumnArea;
        auto utilityArea = controlArea.removeFromLeft(juce::jmin(filterUtilityWidth, controlArea.getWidth()));
        controlArea.removeFromLeft(juce::jmin(controlGap, controlArea.getWidth()));

        {
            // Type and Slope share one sizing system.  Explicit label bounds
            // avoid JUCE's attached-label positioning squeezing the Slope box
            // against the final Type button at smaller editor scales.
            const int controlHeight = juce::jmax(1, juce::jmin(
                juce::roundToInt(28.0f * uiScale),
                juce::roundToInt(utilityArea.getHeight() / 5.92f)));
            const int labelHeight = juce::jmax(1, juce::roundToInt(controlHeight * 0.50f));
            const int itemGap = juce::jmax(1, juce::roundToInt(controlHeight * 0.14f));
            const int sectionGap = juce::jmax(itemGap * 2,
                                              juce::roundToInt(controlHeight * 0.36f));
            const int controlWidth = juce::jmin(utilityArea.getWidth(), juce::roundToInt(
                juce::jlimit(66.0f * uiScale, 92.0f * uiScale,
                             utilityArea.getWidth() * 0.78f)));
            const int blockHeight = labelHeight * 2 + controlHeight * 4
                                  + itemGap * 4 + sectionGap;
            const auto utilityLabelFont = fire::ui::labelFont(
                juce::jmax(10.0f * uiScale, labelHeight * 0.78f));
            filterTypeLabel.setFont(utilityLabelFont);
            lowcutSlopeLabel.setFont(utilityLabelFont);
            highcutSlopeLabel.setFont(utilityLabelFont);
            auto utilityBlock = juce::Rectangle<int>(0, 0, controlWidth, blockHeight)
                                    .withCentre(utilityArea.getCentre());

            filterTypeLabel.setBounds(utilityBlock.removeFromTop(labelHeight));
            utilityBlock.removeFromTop(itemGap);
            filterLowCutButton.setBounds(utilityBlock.removeFromTop(controlHeight));
            utilityBlock.removeFromTop(itemGap);
            filterPeakButton.setBounds(utilityBlock.removeFromTop(controlHeight));
            utilityBlock.removeFromTop(itemGap);
            filterHighCutButton.setBounds(utilityBlock.removeFromTop(controlHeight));
            utilityBlock.removeFromTop(sectionGap);

            const auto slopeLabelBounds = utilityBlock.removeFromTop(labelHeight);
            lowcutSlopeLabel.setBounds(slopeLabelBounds);
            highcutSlopeLabel.setBounds(slopeLabelBounds);
            utilityBlock.removeFromTop(itemGap);

            const auto slopeBounds = utilityBlock.removeFromTop(controlHeight);
            lowcutSlopeMode.setBounds(slopeBounds);
            highcutSlopeMode.setBounds(slopeBounds);
        }

        {
            auto knobsArea = controlArea.withSizeKeepingCentre(ordinaryKnobSize * 3 + controlGap * 2,
                                                               ordinaryKnobHeight);

            auto freqBounds = knobsArea.removeFromLeft(ordinaryKnobSize);
            knobsArea.removeFromLeft(controlGap);
            auto gainBounds = knobsArea.removeFromLeft(ordinaryKnobSize);
            knobsArea.removeFromLeft(controlGap);
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
        auto knobRow = knobsColumnArea.withSizeKeepingCentre(ordinaryKnobSize * 4 + controlGap * 3,
                                                             ordinaryKnobHeight);
        modulatableSliderComponents.at(DOWNSAMPLE_NAME)->setBounds(knobRow.removeFromLeft(ordinaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(BIT_DEPTH_NAME)->setBounds(knobRow.removeFromLeft(ordinaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(JITTER_NAME)->setBounds(knobRow.removeFromLeft(ordinaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(DOWNSAMPLE_MIX_NAME)->setBounds(knobRow.removeFromLeft(ordinaryKnobSize));
    }
    else if (graphSwitch.getToggleState())
    {
        if (zoomedGraph != nullptr)
        {
            zoomedGraph->setBounds(
                knobsColumnArea.reduced(juce::roundToInt(2.0f * uiScale)));
            zoomedGraph->toFront(false);
        }
        else
        {
            juce::FlexBox graphBox;
            graphBox.flexDirection = juce::FlexBox::Direction::row;
            graphBox.justifyContent = juce::FlexBox::JustifyContent::center;

            graphBox.items.add(juce::FlexItem(oscilloscope).withFlex(1.0f));
            graphBox.items.add(juce::FlexItem(vuPanel).withFlex(1.0f));
            graphBox.items.add(juce::FlexItem(widthGraph).withFlex(1.0f));

            graphBox.performLayout(
                knobsColumnArea.reduced(juce::roundToInt(2.0f * uiScale)));
        }
    }

    invalidateChromeCache();
}

void GlobalPanel::animationTick(float deltaSeconds)
{
    advanceContentTransition(deltaSeconds);
    if (! selectionAnimationInitialised)
        return;

    if (! isShowing())
    {
        selectionY.snapTo(selectionY.target);
        return;
    }

    bool changed = selectionY.advance(deltaSeconds);

    if (changed)
        repaint(tabAreaRect.expanded(juce::jmax(2, juce::roundToInt(3.0f * scale))));
}

void GlobalPanel::setScale(float newScale)
{
    newScale = juce::jmax(0.25f, newScale);
    if (juce::approximatelyEqual(scale, newScale))
        return;

    scale = newScale;
    const std::array<GraphTemplate*, 3> graphs {
        &oscilloscope, &vuPanel, &widthGraph
    };
    for (auto* graph : graphs)
        graph->setScale(newScale);

    resized();
}

juce::TextButton* GlobalPanel::getSelectedSwitch() noexcept
{
    if (downsampleSwitch.getToggleState())
        return &downsampleSwitch;
    if (graphSwitch.getToggleState())
        return &graphSwitch;
    return &filterSwitch;
}

void GlobalPanel::updateSelectionTarget(bool snap)
{
    auto* selectedSwitch = getSelectedSwitch();
    if (selectedSwitch == nullptr || selectedSwitch->getBounds().isEmpty())
        return;

    const auto selectionInset = juce::jmax(0.5f, 1.0f * scale);
    const auto targetY = static_cast<float>(selectedSwitch->getY()) + selectionInset;
    if (snap || ! selectionAnimationInitialised || ! isShowing())
    {
        selectionY.snapTo(targetY);
        selectionAnimationInitialised = true;
        return;
    }

    selectionY.setTarget(targetY);
}

void GlobalPanel::rebuildChromeCache(float displayScale)
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
    drawMinimalSurface(cacheGraphics, controlsAreaRect.getUnion(outputAreaRect).toFloat());

    const int titleHeight = juce::roundToInt(22.0f * scale);
    const int titleInset = juce::roundToInt(8.0f * scale);
    auto titleFor = [titleHeight, titleInset](juce::Rectangle<int> area)
    {
        area.reduce(titleInset, 0);
        return area.removeFromTop(juce::jmin(titleHeight, area.getHeight())).toFloat();
    };

    juce::String sectionTitle { "FILTER" };
    if (downsampleSwitch.getToggleState())
        sectionTitle = "LO-FI";
    else if (graphSwitch.getToggleState())
        sectionTitle = "ANALYSIS";

    drawMinimalTitle(cacheGraphics, titleFor(tabAreaRect), "MODE");
    drawMinimalTitle(cacheGraphics, titleFor(controlsAreaRect), sectionTitle);
    drawMinimalTitle(cacheGraphics, titleFor(outputAreaRect), "MASTER");

    chromeCacheDirty = false;
}

void GlobalPanel::invalidateChromeCache()
{
    chromeCacheDirty = true;
    repaint();
}

void GlobalPanel::configureGraphInteractions()
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    const std::array<GraphTemplate*, 3> graphs {
        &oscilloscope, &vuPanel, &widthGraph
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

void GlobalPanel::toggleGraphZoom(GraphTemplate* graph)
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    const std::array<GraphTemplate*, 3> graphs {
        &oscilloscope, &vuPanel, &widthGraph
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

void GlobalPanel::clearGraphZoom() noexcept
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    auto* graph = zoomedGraph;
    zoomedGraph = nullptr;
    if (graph != nullptr)
        graph->setZoomState(false);

    if (safeThis != nullptr)
        safeThis->restoreComponentsObscuredByZoom();
}

void GlobalPanel::hideComponentsObscuredByZoom(const GraphTemplate& graph)
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
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
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

void GlobalPanel::restoreComponentsObscuredByZoom() noexcept
{
    if (componentsHiddenForGraphZoom.empty())
        return;

    auto components = std::move(componentsHiddenForGraphZoom);
    componentsHiddenForGraphZoom.clear();
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);

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

void GlobalPanel::buttonClicked(juce::Button* clickedButton)
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    auto setGroupVisibility = [this, &safeThis](
                                  juce::Array<juce::Component*>& components,
                                  bool shouldBeVisible)
    {
        setVisibility(components, shouldBeVisible);
        return safeThis != nullptr;
    };
    const bool changesSlopeContext = clickedButton == &filterSwitch
                                     || clickedButton == &downsampleSwitch
                                     || clickedButton == &graphSwitch
                                     || clickedButton == &filterLowCutButton
                                     || clickedButton == &filterPeakButton
                                     || clickedButton == &filterHighCutButton;

    if (changesSlopeContext)
    {
        invalidateSlopeInteractions();
        if (safeThis == nullptr)
            return;
    }

    if ((clickedButton == &filterSwitch && filterSwitch.getToggleState())
        || (clickedButton == &downsampleSwitch && downsampleSwitch.getToggleState())
        || (clickedButton == &graphSwitch && graphSwitch.getToggleState()))
    {
        clearGraphZoom();
        if (safeThis == nullptr)
            return;
    }

    bool isSwitch = false;
    if (clickedButton == &filterSwitch && filterSwitch.getToggleState())
    {
        if (! setGroupVisibility(filterComponents, true))
            return;
        if (! setGroupVisibility(downsampleComponents, false))
            return;
        if (! setGroupVisibility(graphComponents, false))
            return;
        updateFilterKnobVisibility();
        if (safeThis == nullptr)
            return;
        isSwitch = true;
    }
    else if (clickedButton == &downsampleSwitch && downsampleSwitch.getToggleState())
    {
        if (! setGroupVisibility(downsampleComponents, true))
            return;
        if (! setGroupVisibility(filterComponents, false))
            return;
        if (! setGroupVisibility(graphComponents, false))
            return;
        isSwitch = true;
    }
    else if (clickedButton == &graphSwitch && graphSwitch.getToggleState())
    {
        if (! setGroupVisibility(graphComponents, true))
            return;
        if (! setGroupVisibility(filterComponents, false))
            return;
        if (! setGroupVisibility(downsampleComponents, false))
            return;
        isSwitch = true;
    }
    else if (clickedButton == &filterLowCutButton || clickedButton == &filterPeakButton || clickedButton == &filterHighCutButton)
    {
        updateFilterKnobVisibility();
        if (safeThis == nullptr)
            return;
    }

    if (isSwitch)
    {
        startContentTransition(controlsAreaRect);
        resized();
        if (safeThis == nullptr)
            return;

        invalidateChromeCache();
    }
}

void GlobalPanel::updateFilterKnobVisibility()
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    // Filter type attachments may update from automation or state restore
    // while another global module is selected. Child visibility is independent
    // of its siblings, so never let such an update reveal a filter group over
    // Lo-Fi or Analysis.
    const bool filterModuleVisible = filterSwitch.getToggleState()
                                     && ! downsampleSwitch.getToggleState()
                                     && ! graphSwitch.getToggleState();
    const bool peakVisible = filterModuleVisible
                             && filterPeakButton.getToggleState();
    const bool lowcutVisible = filterModuleVisible
                               && filterLowCutButton.getToggleState();
    const bool highcutVisible = filterModuleVisible
                                && filterHighCutButton.getToggleState();

    setVisibility(peakKnobs, peakVisible);
    if (safeThis == nullptr)
        return;

    setVisibility(lowcutKnobs, lowcutVisible);
    if (safeThis == nullptr)
        return;

    setVisibility(highcutKnobs, highcutVisible);
}

void GlobalPanel::setVisibility(juce::Array<juce::Component*>& array, bool isVisible)
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    for (auto* component : array)
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
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
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

    // Simplified logic as the bypass buttons are now separate from the main component groups
    if (index == 0) // Filter
    {
        invalidateSlopeInteractions();
        if (safeThis == nullptr)
            return;

        for (auto* component : filterComponents)
        {
            if (! setComponentEnabled(component, state))
                return;
        }
    }
    else if (index == 1) // Lo-Fi (Downsample)
    {
        for (auto* component : downsampleComponents)
        {
            if (! setComponentEnabled(component, state))
                return;
        }
    }
}
