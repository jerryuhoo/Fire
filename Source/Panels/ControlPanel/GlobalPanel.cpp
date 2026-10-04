/*
  ==============================================================================

    GlobalPanel.cpp
    Created: 21 Sep 2021 8:53:20am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "GlobalPanel.h"
#include "../../GUI/FireTheme.h"
#include "../../GUI/Skin.h"
#include "../../Utility/AudioHelpers.h"
#include <algorithm>
#include <array>

namespace
{
void drawMinimalSurface(juce::Graphics& g, const juce::Component& owner, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty())
        return;

    bounds = bounds.reduced(0.5f);
    juce::ColourGradient fill(fire::ui::paletteFor(owner).surface1.withAlpha(fire::ui::isVintage(owner) ? 1.0f : 0.72f),
                              bounds.getX(), bounds.getY(),
                              fire::ui::paletteFor(owner).surface0.withAlpha(fire::ui::isVintage(owner) ? 1.0f : 0.88f),
                              bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, fire::ui::Metrics::radius);
    if (fire::ui::isVintage(owner))
    {
        g.setColour(juce::Colours::black.withAlpha(.48f)); g.drawRoundedRectangle(bounds,fire::ui::Metrics::radius,1);
        g.setColour(juce::Colour(0xffded3b7).withAlpha(.14f));
        g.drawRoundedRectangle(bounds.reduced(1.5f),fire::ui::Metrics::radius-1,1);
    }
}

void drawMinimalTitle(juce::Graphics& g, const juce::Component& owner,
                      juce::Rectangle<float> bounds,
                      const juce::String& text)
{
    g.setFont(fire::ui::labelFont(juce::jlimit(10.0f, 20.0f, bounds.getHeight() * 0.46f)));
    g.setColour(fire::ui::paletteFor(owner).textSecondary.withAlpha(0.82f));
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

    std::array<ModulatableSlider*, fire::effects::controlCount> insertKnobs {};
    for (size_t i = 0; i < insertKnobs.size(); ++i)
    {
        const auto key = "InsertControl" + juce::String(static_cast<int>(i));
        createAndConfigureSlider(key, "", fire::ui::colours::chorus);
        insertKnobs[i] = modulatableSliderComponents.at(key).get();
        insertKnobs[i]->setInteractionOnlyReadout(true);
    }
    insertControls.setControls(insertKnobs);
    createAndConfigureSlider("Analog Drive", "Drive", fire::ui::colours::drive);
    auto& analogDrive = *modulatableSliderComponents.at("Analog Drive");
    setupModulationCallbacks(analogDrive);
    insertControls.setAnalogDriveControl(analogDrive);
    std::array<ModulatableSlider*, InsertEffectControls::cloudsExtraCount> cloudsKnobs {};
    const std::array<const char*, InsertEffectControls::cloudsExtraCount> cloudsNames { "Spread", "Feedback", "Reverb" };
    for (size_t i = 0; i < cloudsKnobs.size(); ++i)
    {
        const auto key = "CloudsControl" + juce::String(static_cast<int>(i));
        createAndConfigureSlider(key, cloudsNames[i], fire::ui::colours::granular);
        cloudsKnobs[i] = modulatableSliderComponents.at(key).get();
        cloudsKnobs[i]->setInteractionOnlyReadout(true);
    }
    insertControls.setCloudsControls(cloudsKnobs);
    EqControlsPanel::Knobs insertEqKnobs {};
    for (int node = 0; node < fire::eq::maxNodes; ++node)
        for (int control = 0; control < 3; ++control)
        {
            const auto name = "Insert EQ " + juce::String(node) + " Control " + juce::String(control);
            createAndConfigureSlider(name, control == 0 ? "Frequency" : control == 1 ? "Gain" : "Q", fire::ui::colours::filter);
            auto* knob = modulatableSliderComponents.at(name).get();
            setupModulationCallbacks(*knob);
            insertEqKnobs[static_cast<size_t>(node)][static_cast<size_t>(control)] = knob;
        }
    insertControls.setEqControls(insertEqKnobs);
    insertControls.bind(0, 0);
    insertControls.onLayoutChanged = [safe = juce::Component::SafePointer<GlobalPanel>(this)]
    {
        if (safe) safe->refreshInsertLayout();
    };
    addChildComponent(insertControls);
    addAndMakeVisible(effectNavigation);
    effectNavigation.setBuiltins({{&filterSwitch, filterBypassButton.get()}, {&downsampleSwitch, downsampleBypassButton.get()}, {&graphSwitch, nullptr}});
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
    effectNavigation.refresh();
}

GlobalPanel::~GlobalPanel()
{
    effectNavigation.onSelectEffect = nullptr;
    insertControls.onLayoutChanged = nullptr;
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
    eqControls.dismiss();
    if (! safeThis) return;
    effectNavigation.dismiss();
    if (! safeThis) return;
    insertControls.dismissButtons();
    if (! safeThis) return;
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
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    eqControls.dismiss();
    if (! safeThis) return;
    ++slopeInteractionGeneration;
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
    for (size_t i = 0; i < fire::effects::tapeIDs.size(); ++i)
        createAndConfigureSlider(fire::effects::tapeNames[i], fire::effects::tapeNames[i], lofiColour);
    for (const auto& name : {juce::String(DOWNSAMPLE_NAME), juce::String(BIT_DEPTH_NAME), juce::String(JITTER_NAME),
                             juce::String(DOWNSAMPLE_MIX_NAME), juce::String("Tape"), juce::String("Wow"), juce::String("Flutter")})
        modulatableSliderComponents.at(name)->setInteractionOnlyReadout(true);

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

    EqControlsPanel::Knobs eqKnobs {};
    constexpr const char* legacyNames[3][3] {
        {LOWCUT_FREQ_NAME, LOWCUT_GAIN_NAME, LOWCUT_Q_NAME},
        {PEAK_FREQ_NAME, PEAK_GAIN_NAME, PEAK_Q_NAME},
        {HIGHCUT_FREQ_NAME, HIGHCUT_GAIN_NAME, HIGHCUT_Q_NAME}
    };
    const fire::eq::Field fields[] {fire::eq::Field::frequency, fire::eq::Field::gain, fire::eq::Field::q};
    const char* labels[] {"Frequency", "Gain", "Q"};
    const char* suffixes[] {" Hz", " dB", ""};
    for (int node = 0; node < fire::eq::maxNodes; ++node)
        for (int control = 0; control < 3; ++control)
        {
            const auto name = node < 3 ? juce::String(legacyNames[node][control])
                                      : fire::eq::parameterID(node, fields[control]);
            if (node >= 3) createAndConfigureSlider(name, labels[control], filterColour, suffixes[control]);
            eqKnobs[static_cast<size_t>(node)][static_cast<size_t>(control)] = modulatableSliderComponents.at(name).get();
        }
    addChildComponent(eqControls);
    eqControls.setKnobs(eqKnobs);
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
        label.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textSecondary);
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
        btn.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textSecondary);
        juce::ignoreUnused(colour);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, fire::ui::paletteFor(*this).textPrimary);
        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        btn.getProperties().set("fireAnimatedSelection", true);
        btn.getProperties().set("fireModuleRail", true);

        btn.addListener(this);
    };

    setupSwitch(filterSwitch, "EQ", fire::ui::colours::filter);
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
                 "Global EQ power",
                 "Enable or bypass the global EQ");
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
        menu->setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface1);
        menu->setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
        menu->setColour(juce::ComboBox::textColourId, fire::ui::paletteFor(*this).textPrimary);
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

    filterComponents = { &eqControls };
    for (auto* oldControl : std::array<juce::Component*, 8> {&filterLowCutButton, &filterPeakButton,
             &filterHighCutButton, &filterTypeLabel, &lowcutSlopeMode, &highcutSlopeMode,
             &lowcutSlopeLabel, &highcutSlopeLabel})
        oldControl->setVisible(false);

    downsampleComponents = {
        modulatableSliderComponents.at(DOWNSAMPLE_NAME).get(),
        modulatableSliderComponents.at(BIT_DEPTH_NAME).get(),
        modulatableSliderComponents.at(JITTER_NAME).get(),
        modulatableSliderComponents.at(DOWNSAMPLE_MIX_NAME).get(),
    };
    for (auto* name : fire::effects::tapeNames) downsampleComponents.add(modulatableSliderComponents.at(name).get());

    allControls.addArray(filterComponents);
    allControls.addArray(downsampleComponents);
    allControls.addArray(graphComponents);
    allControls.add(&insertControls);
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

    for (int node = 3; node < fire::eq::maxNodes; ++node)
        for (auto field : {fire::eq::Field::frequency, fire::eq::Field::gain, fire::eq::Field::q})
        {
            const auto id = fire::eq::parameterID(node, field);
            auto* slider = modulatableSliderComponents.at(id).get();
            slider->parameterID = id;
            slider->setComponentID(id);
            sliderAttachments[id] = std::make_unique<SliderAttachment>(processor.treeState, id, *slider);
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
    button.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface1);
    button.setColour(juce::TextButton::buttonOnColourId, fire::ui::paletteFor(*this).raised);
    button.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    button.setColour(juce::TextButton::textColourOnId, fire::ui::colours::filter);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textMuted);
    if (button.getComponentID().isEmpty())
    {
        button.setComponentID("rounded");
        button.setButtonText(buttonName);
    }
    button.addListener(this);
}

void GlobalPanel::lookAndFeelChanged()
{
    if (fire::ui::isVintage(*this))
        selectionY.snapTo(selectionY.target);
    // A skin change can arrive at the same size and display scale. Never
    // reuse the previous skin's rasterised panel chrome in that case.
    invalidateChromeCache();
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

}

void GlobalPanel::resized()
{
    const float uiScale = scale;
    // Drive is the sole hero control; every other rotary in the editor uses
    // this quieter common diameter.
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

    const int navWidth = fire::ui::moduleRailWidth(uiScale, getWidth());
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
    const int ordinaryKnobSize = fire::ui::ordinaryKnobWidth(uiScale, {
        filterKnobLimit,
        lofiKnobLimit,
        masterKnobLimit,
        (knobsColumnArea.getHeight() - controlGap) / 2 - valueHeight });
    const int ordinaryKnobHeight = fire::ui::ordinaryKnobHeight(ordinaryKnobSize, uiScale);

    effectNavigation.setBounds(tabAreaRect);
    effectNavigation.setScale(uiScale);
    insertControls.setScale(uiScale);
    insertControls.setKnobWidth(ordinaryKnobSize);
    if (selectedInsert >= 0)
    {
        auto insertArea = knobsColumnArea;
        if (! insertControls.usesFullWidthLayout())
        {
            auto graphArea = insertArea.removeFromRight(juce::jmin(insertArea.getWidth() / 2, juce::roundToInt(230.0f * uiScale)));
            insertArea.removeFromRight(controlGap);
            oscilloscope.setBounds(graphArea);
        }
        insertControls.setBounds(insertArea);
        if (zoomedGraph != nullptr) zoomedGraph->setBounds(knobsColumnArea.reduced(2));
    }
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
        eqControls.setScale(uiScale);
        eqControls.setKnobWidth(ordinaryKnobSize);
        eqControls.setBounds(knobsColumnArea);
    }
    else if (downsampleSwitch.getToggleState())
    {
        const auto size = ordinaryKnobSize;
        auto area = knobsColumnArea.withSizeKeepingCentre(size * 4 + controlGap * 3, ordinaryKnobHeight * 2 + controlGap);
        constexpr const char* names[] {DOWNSAMPLE_NAME, BIT_DEPTH_NAME, JITTER_NAME, DOWNSAMPLE_MIX_NAME, "Tape", "Wow", "Flutter"};
        for (int row = 0; row < 2; ++row)
        {
            auto strip = area.removeFromTop(ordinaryKnobHeight); area.removeFromTop(controlGap);
            if (row == 1) strip = strip.withSizeKeepingCentre(size * 3 + controlGap * 2, ordinaryKnobHeight);
            for (int column = 0; column < (row == 0 ? 4 : 3); ++column)
            {
                modulatableSliderComponents.at(names[row * 4 + column])->setBounds(strip.removeFromLeft(size));
                strip.removeFromLeft(controlGap);
            }
        }
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
    const juce::Component::SafePointer<GlobalPanel> safeOwner(this);
    eqControls.animationTick(deltaSeconds);
    if (! safeOwner) return;
    effectNavigation.animationTick(deltaSeconds);
    if (! safeOwner) return;
    insertControls.refresh();
    if (! safeOwner) return;
    if (! selectionAnimationInitialised)
        return;

    if (! isShowing())
    {
        selectionY.snapTo(selectionY.target);
        return;
    }

    bool changed = false;
    if (fire::ui::isVintage(*this))
    {
        changed = !juce::approximatelyEqual(selectionY.current, selectionY.target);
        selectionY.snapTo(selectionY.target);
    }
    else
        changed = selectionY.advance(deltaSeconds);

    if (changed)
        repaint(tabAreaRect.expanded(juce::jmax(2, juce::roundToInt(3.0f * scale))));
}

void GlobalPanel::setScale(float newScale)
{
    newScale = juce::jmax(0.25f, newScale);
    if (juce::approximatelyEqual(scale, newScale))
        return;

    scale = newScale;
    eqControls.setScale(newScale);
    const std::array<GraphTemplate*, 3> graphs {
        &oscilloscope, &vuPanel, &widthGraph
    };
    for (auto* graph : graphs)
        graph->setScale(newScale);

    resized();
}

juce::TextButton* GlobalPanel::getSelectedSwitch() noexcept
{
    if (selectedInsert >= 0) return nullptr;
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
    if (snap || fire::ui::isVintage(*this) || ! selectionAnimationInitialised || ! isShowing())
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

    fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat(), fire::ui::skinFor(*this));
    drawMinimalSurface(cacheGraphics, *this, controlsAreaRect.getUnion(outputAreaRect).toFloat());

    const int titleHeight = juce::roundToInt(22.0f * scale);
    const int titleInset = juce::roundToInt(8.0f * scale);
    auto titleFor = [titleHeight, titleInset](juce::Rectangle<int> area)
    {
        area.reduce(titleInset, 0);
        return area.removeFromTop(juce::jmin(titleHeight, area.getHeight())).toFloat();
    };

    juce::String sectionTitle { "EQ" };
    if (selectedInsert >= 0)
        sectionTitle = insertControls.usesExpandedLayout() ? "GRANULAR / CLOUDS"
            : fire::effects::name(processor.getInsertEffectType(0, selectedInsert));
    else if (downsampleSwitch.getToggleState())
        sectionTitle = "LO-FI";
    else if (graphSwitch.getToggleState())
        sectionTitle = "ANALYSIS";

    drawMinimalTitle(cacheGraphics, *this, titleFor(tabAreaRect), "CHAIN");
    drawMinimalTitle(cacheGraphics, *this, titleFor(controlsAreaRect), sectionTitle);
    drawMinimalTitle(cacheGraphics, *this, titleFor(outputAreaRect), "MASTER");

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
            || ! cover.intersects(component->getBounds())
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
        selectedInsert = -1;
        effectNavigation.setSelectedSlot(-1);
        insertControls.setActive(false);
        if (! safeThis) return;
        updateFilterKnobVisibility();
        if (! safeThis) return;
        modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setInteractionOnlyReadout(downsampleSwitch.getToggleState());
        modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setInteractionOnlyReadout(downsampleSwitch.getToggleState());
        repaint(controlsAreaRect);
        resized();
        if (safeThis == nullptr)
            return;

        invalidateChromeCache();
    }
}

void GlobalPanel::selectInsertEffect(int slot)
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    if (slot < 0 || processor.getInsertEffectType(0, slot) == fire::effects::Type::none)
    {
        clearGraphZoom();
        if (!safeThis) return;
        selectedInsert = -1;
        insertControls.setActive(false);
        if (!safeThis) return;
        for (auto* button : {&filterSwitch, &downsampleSwitch, &graphSwitch}) button->setToggleState(false, juce::dontSendNotification);
        for (auto* group : {&filterComponents, &downsampleComponents, &graphComponents})
        {setVisibility(*group, false); if (!safeThis) return;}
        eqControls.setVisible(false);
        if (safeThis) resized();
        return;
    }
    dismissTransientInteraction();
    if (! safeThis) return;
    clearGraphZoom();
    if (! safeThis) return;
    selectedInsert = slot;
    effectNavigation.setSelectedSlot(slot);
    for (auto* group : {&filterComponents, &downsampleComponents, &graphComponents})
    {
        setVisibility(*group, false);
        if (! safeThis) return;
    }
    insertControls.bind(0, slot);
    if (! safeThis) return;
    insertControls.setActive(true);
    if (! safeThis) return;
    oscilloscope.setVisible(! insertControls.usesFullWidthLayout());
    if (! safeThis) return;
    modulatableSliderComponents.at(GLOBAL_OUTPUT_NAME)->setInteractionOnlyReadout(true);
    modulatableSliderComponents.at(GLOBAL_MIX_NAME)->setInteractionOnlyReadout(true);
    repaint(controlsAreaRect);
    resized(); invalidateChromeCache();
}

void GlobalPanel::focusInsertEffect(int slot)
{
    if (! juce::isPositiveAndBelow(slot, fire::effects::slotCount)
        || processor.getInsertEffectType(0, slot) == fire::effects::Type::none)
        return;
    const juce::Component::SafePointer<GlobalPanel> safe(this);
    // State loading can add the requested row while this panel is hidden.
    // Refresh the rail before selecting it so its controls and scroll target
    // use the newly loaded slot, rather than the previous chain's geometry.
    effectNavigation.setSelectedSlot(-1);
    if (! safe) return;
    effectNavigation.refresh();
    if (safe) selectInsertEffect(slot);
}

void GlobalPanel::refreshInsertLayout()
{
    if (selectedInsert < 0) return;
    const auto expectedSlot = selectedInsert;
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    clearGraphZoom();
    if (! safeThis || selectedInsert != expectedSlot) return;
    oscilloscope.setVisible(! insertControls.usesFullWidthLayout());
    if (! safeThis) return;
    resized();
    if (! safeThis) return;
    invalidateChromeCache();
}

void GlobalPanel::updateFilterKnobVisibility()
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    const std::array<bool, 3> legacySelection {filterLowCutButton.getToggleState(),
                                              filterPeakButton.getToggleState(),
                                              filterHighCutButton.getToggleState()};
    if (legacySelection != lastLegacyFilterSelection)
    {
        lastLegacyFilterSelection = legacySelection;
        for (int node = 0; node < 3; ++node)
            if (legacySelection[static_cast<size_t>(node)])
            {
                eqControls.selectNode(node);
                if (! safeThis) return;
                break;
            }
    }
    eqControls.setVisible(filterSwitch.getToggleState()
                          && ! downsampleSwitch.getToggleState()
                          && ! graphSwitch.getToggleState()
                          && selectedInsert < 0);
}

void GlobalPanel::selectEqNode(int slot)
{
    const juce::Component::SafePointer<GlobalPanel> safeThis(this);
    if (! filterSwitch.getToggleState())
        filterSwitch.setToggleState(true, juce::sendNotificationSync);
    if (! safeThis) return;
    eqControls.selectNode(slot);
    if (! safeThis) return;
    updateFilterKnobVisibility();
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
    if (index == 0) // EQ can be edited while bypassed.
    {
        invalidateSlopeInteractions();
        if (! safeThis) return;
        eqControls.setEnabled(true);
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
