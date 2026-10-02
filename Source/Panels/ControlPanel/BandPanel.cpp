/*
  ==============================================================================

    BandPanel.cpp
    Created: 21 Sep 2021 8:52:29am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "BandPanel.h"
#include "../../GUI/FireTheme.h"
#include "../../GUI/Skin.h"
#include "../../Utility/AudioHelpers.h"
#include "../../Utility/DriveCompensationParameters.h"
#include <algorithm>
#include <cmath>

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
    insertControls.bind(1, 0);
    insertControls.onLayoutChanged = [safe = juce::Component::SafePointer<BandPanel>(this)]
    {
        if (safe) safe->refreshInsertLayout();
    };
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
    addChildComponent(hardwareColour);
    configureGraphInteractions();
    configureGraphViewMenu();

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
        processor.treeState.addParameterListener(fire::analog_params::bandID(i), this);
        driveCompParameters[static_cast<size_t>(i)] = {
            processor.treeState.getRawParameterValue(fire::drive_comp::parameterID(i)),
            processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(LINKED_ID, i)),
            processor.treeState.getRawParameterValue(driveParameterIds[static_cast<size_t>(i)]),
            processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(EXTREME_ID, i)),
            processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, i)),
            processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, i))
        };

        for (size_t parameterIndex = 0; parameterIndex < graphParameterBases.size(); ++parameterIndex)
        {
            auto& parameterId = distortionGraphParameterIds[parameterIndex][static_cast<size_t>(i)];
            parameterId = ParameterIDAndName::getIDString(graphParameterBases[parameterIndex], i);
            processor.treeState.addParameterListener(parameterId, this);
        }
    }

    // Set initial attachments for band 0 and update knob enabled states
    setFocusBandNum(0, true);
    effectNavigation.refresh();

    // Set initial visibility
    moduleSelectionPosition.snapTo(0.0f);
    buttonClicked(&oscSwitch);
    effectNavigation.refresh();
    startTimerHz(30);
}

BandPanel::~BandPanel()
{
    effectNavigation.onSelectEffect = nullptr;
    insertControls.onLayoutChanged = nullptr;
    upgradeDriveCompButton.onClick = nullptr;
    graphViewMenu.configurePopupSession({}, {}, ContextAwareComboBox::SelectionCallback {});
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
        processor.treeState.removeParameterListener(fire::analog_params::bandID(i), this);

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
        label.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textSecondary);
        juce::ignoreUnused(colour);
        label.setJustificationType(juce::Justification::centred);
    };

    setupPanelLabel(dcFilterLabel, "DC", fire::ui::colours::shape);
}

void BandPanel::createButtons()
{
    initFlatButton(linkedButton, "Gain Comp");
    addAndMakeVisible(driveCompReadout);
    driveCompReadout.setComponentID("driveCompensationReadout");
    driveCompReadout.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textSecondary);
    driveCompReadout.setJustificationType(juce::Justification::centred);
    driveCompReadout.setInterceptsMouseClicks(false, false);
    initFlatButton(upgradeDriveCompButton, "Use Drive Comp");
    upgradeDriveCompButton.setClickingTogglesState(false);
    upgradeDriveCompButton.setComponentID("driveCompUpgrade");
    upgradeDriveCompButton.setTitle("Use Drive Comp");
    upgradeDriveCompButton.setTooltip("Move compensation into the Drive stage and enable Comp for this band. "
                                      "This can change the sound: the stored manual Output setting becomes active again.");
    upgradeDriveCompButton.setHelpText(upgradeDriveCompButton.getTooltip());
    const juce::Component::SafePointer<BandPanel> safe(this);
    upgradeDriveCompButton.onClick = [safe]
    {
        if (! safe || ! safe->upgradeDriveCompButton.isShowing() || ! safe->upgradeDriveCompButton.isEnabled()
            || safe->selectedInsert >= 0 || ! safe->oscSwitch.getToggleState() || safe->usesModernDriveCompensation()) return;
        const auto band = safe->focusBandNum;
        auto& processor = safe->processor;
        processor.upgradeBandDriveCompensation(band);
        if (safe) safe->updateDriveCompensationPresentation();
    };
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
        btn.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textSecondary);
        juce::ignoreUnused(colour);

        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, fire::ui::paletteFor(*this).textPrimary);

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
    const juce::Component::SafePointer<BandPanel> safe(this);
    for (size_t i = 0; i < distortionModes.size(); ++i)
    {
        const auto legacyID = ParameterIDAndName::getIDString(MODE_ID, static_cast<int>(i));
        const auto modelID = fire::analog_params::bandID(static_cast<int>(i));
        auto& menu = distortionModes[i];
        menu.setTitle("Band " + juce::String(static_cast<int>(i) + 1) + " distortion mode");
        menu.setTooltip("Select the distortion mode for band " + juce::String(static_cast<int>(i) + 1));
        menu.setComponentID(legacyID);
        setMenu(&menu);
        menu.configurePopupSession([safe] {return safe ? safe->distortionModeInteractionGeneration : 0;},
            [safe, i] {return safe && safe->canOpenDistortionModePopup(i);},
            [safe, i](int item)
            {
                if (!safe || !safe->canOpenDistortionModePopup(i)) return;
                const auto epoch = safe->distortionModeInteractionGeneration;
                auto& selector = safe->distortionModes[i];
                const int before = safe->processor.getShapeMode(static_cast<int>(i) + 1) + 1;
                selector.setSelectedId(before, juce::dontSendNotification);
                if (!safe) return;
                selector.setSelectedId(item, juce::sendNotificationSync);
                if (!safe || safe->distortionModeInteractionGeneration != epoch || !safe->canOpenDistortionModePopup(i)) return;
                safe->processor.setShapeMode(static_cast<int>(i) + 1, -1, item - 1);
            });
        const auto update = [safe, i](float)
        {
            if (!safe) return;
            safe->distortionModes[i].setSelectedId(safe->processor.getShapeMode(static_cast<int>(i) + 1) + 1, juce::dontSendNotification);
            safe->updateDistortionGraphFromParameters();
            if (safe) safe->applySelectedGraphView();
            if (safe) safe->resized();
        };
        modeAttachments[i] = std::make_unique<juce::ParameterAttachment>(*processor.treeState.getParameter(legacyID), update, nullptr);
        shapeModelAttachments[i] = std::make_unique<juce::ParameterAttachment>(*processor.treeState.getParameter(modelID), update, nullptr);
        menu.setSelectedId(processor.getShapeMode(static_cast<int>(i) + 1) + 1, juce::dontSendNotification);
    }
}

void BandPanel::setupComponentGroups()
{
    driveComponents = {
        modulatableSliderComponents.at(DRIVE_NAME).get(),
        &linkedButton, &driveCompReadout, &upgradeDriveCompButton
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
    allControls.add(&safeButton, &extremeButton);
    allControls.addArray(shapeComponents);
    allControls.addArray(compressorComponents);
    allControls.addArray(widthComponents);
    allControls.addArray(ottComponents);
    allControls.add(&insertControls);
    for (auto& modeBox : distortionModes)
        allControls.add(&modeBox);
}

void BandPanel::lookAndFeelChanged()
{
    if (fire::ui::isVintage(*this))
        moduleSelectionPosition.snapTo(moduleSelectionPosition.target);
    // A skin change can arrive at the same size and display scale. Never
    // reuse the previous skin's rasterised panel chrome in that case.
    invalidateChromeCache();
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

    const int previousNavWidth = juce::roundToInt(juce::jlimit(120.0f * uiScale,
                                                       165.0f * uiScale,
                                                       static_cast<float>(getWidth()) * 0.14f));
    const int navWidth = fire::ui::moduleRailWidth(uiScale, getWidth());
    const int outputWidth = juce::roundToInt(juce::jlimit(205.0f * uiScale,
                                                          240.0f * uiScale,
                                                          static_cast<float>(getWidth()) * 0.22f));
    // Take the additional rail width from the visualiser so the dial grid
    // retains its established dimensions at the minimum editor size.
    const int graphWidth = juce::jmax(1, juce::roundToInt(juce::jlimit(220.0f * uiScale,
                                                         320.0f * uiScale,
                                                         static_cast<float>(getWidth()) * 0.27f)) - (navWidth - previousNavWidth));

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
    const int secondaryKnobSize = fire::ui::ordinaryKnobWidth(uiScale, {
        (knobsColumnArea.getWidth() - controlGap * 2) / 3,
        (knobsColumnArea.getHeight() - controlGap) / 2 - valueHeight,
        knobsColumnArea.getHeight() - modeHeight - controlGap - dcReserve - valueHeight,
        (outputColumnArea.getWidth() - controlGap) / 2,
        outputColumnArea.getHeight() - buttonAreaHeight - controlGap - valueHeight
    });

    const int secondaryKnobHeight = fire::ui::ordinaryKnobHeight(secondaryKnobSize, uiScale);

    effectNavigation.setBounds(tabAreaRect);
    effectNavigation.setScale(uiScale);
    insertControls.setScale(uiScale);
    insertControls.setKnobWidth(secondaryKnobSize);
    insertControls.setBounds(selectedInsert >= 0 && insertControls.usesFullWidthLayout()
        ? contentArea(knobsAreaRect.getUnion(graphAreaRect)) : knobsColumnArea);

    // --- Active module controls ---
    if (oscSwitch.getToggleState())
    {
        const bool modern = usesModernDriveCompensation();
        // Reuse the card's title strip rather than shrinking the hero dial.
        // Its original dimensions are retained; only its vertical position
        // changes to make room for the paired gain control below it.
        const int driveSize = juce::jmax(1, std::min({ scaledKnobSize * 2,
                                                      knobsColumnArea.getWidth(), knobsColumnArea.getHeight() }));
        auto driveArea = knobsAreaRect.reduced(cardPadding);
        const auto compRowHeight = juce::jmin(titleHeight, driveArea.getHeight());
        const auto compGap = juce::jmax(2, juce::roundToInt(4.0f * uiScale));
        auto compArea = driveArea.removeFromBottom(compRowHeight);
        modulatableSliderComponents.at(DRIVE_NAME)->setBounds(
            driveArea.withSizeKeepingCentre(driveSize, driveSize));
        const auto rowWidth = juce::jmin(compArea.getWidth(), juce::roundToInt((modern ? 206.0f : 224.0f) * uiScale));
        auto compRow = compArea.withSizeKeepingCentre(rowWidth, compRowHeight);
        linkedButton.setBounds(compRow.removeFromLeft(juce::jmin(compRow.getWidth(), juce::roundToInt((modern ? 96.0f : 98.0f) * uiScale))));
        compRow.removeFromLeft(juce::jmin(compGap, compRow.getWidth()));
        driveCompReadout.setBounds(compRow);
        driveCompReadout.setFont(fire::ui::valueFont((modern ? 12.0f : 10.5f) * uiScale));
        auto upgradeArea = knobsAreaRect.reduced(cardPadding, 0).removeFromTop(titleHeight);
        upgradeDriveCompButton.setBounds(upgradeArea.removeFromRight(
            juce::jmin(upgradeArea.getWidth(), juce::roundToInt(132.0f * uiScale))));
        upgradeDriveCompButton.toFront(false);
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

    auto selectorArea = graphAreaRect.reduced(cardPadding, cardPadding).removeFromTop(titleHeight);
    if (zoomedGraph != nullptr && zoomedGraph->isVisible())
    {
        selectorArea = zoomedGraph->getBounds().reduced(juce::roundToInt(7.0f * uiScale),
                                                       juce::roundToInt(5.0f * uiScale));
        selectorArea = selectorArea.removeFromTop(juce::roundToInt(23.0f * uiScale));
        selectorArea.removeFromRight(juce::roundToInt(24.0f * uiScale));
        selectorArea = selectorArea.removeFromRight(juce::jmin(selectorArea.getWidth(), juce::roundToInt(216.0f * uiScale)));
    }
    graphSelectorStrip.setBounds(selectorArea);
    auto selectorBounds = graphSelectorStrip.getLocalBounds();
    graphViewLabel.setFont(fire::ui::labelFont(10.0f * uiScale));
    graphViewLabel.setBounds(selectorBounds.removeFromLeft(juce::roundToInt(34.0f * uiScale)));
    graphViewMenu.setBounds(selectorBounds);
    graphSelectorStrip.toFront(false);

    // --- Output card ---
    auto buttonArea = outputColumnArea.removeFromBottom(buttonAreaHeight);
    outputColumnArea.removeFromBottom(juce::jmin(controlGap, outputColumnArea.getHeight()));
    auto twoKnobsBounds = outputColumnArea.withSizeKeepingCentre(secondaryKnobSize * 2 + controlGap,
                                                                 secondaryKnobHeight);
    modulatableSliderComponents.at(OUTPUT_NAME)->setBounds(twoKnobsBounds.removeFromLeft(secondaryKnobSize));
    modulatableSliderComponents.at(MIX_NAME)->setBounds(twoKnobsBounds.removeFromRight(secondaryKnobSize));

    const int compactButtonGap = juce::jmax(3, juce::roundToInt(5.0f * uiScale));
    const int compactGroupWidth = juce::jmin(buttonArea.getWidth(), juce::roundToInt(184.0f * uiScale));
    const int compactButtonWidth = juce::jmax(1, (compactGroupWidth - compactButtonGap * 2) / 3);
    // Keep the original button dimensions while centring the remaining pair.
    auto compactButtonRow = buttonArea.withSizeKeepingCentre(compactButtonWidth * 2 + compactButtonGap, buttonAreaHeight);
    safeButton.setBounds(compactButtonRow.removeFromLeft(compactButtonWidth));
    compactButtonRow.removeFromLeft(juce::jmin(compactButtonGap, compactButtonRow.getWidth()));
    extremeButton.setBounds(compactButtonRow);

    hardwareColour.setBounds(contentArea(graphAreaRect).reduced(0, juce::roundToInt(12 * uiScale)));
    if (usesAnalogShape())
    {
        auto module = contentArea(knobsAreaRect.getUnion(graphAreaRect));
        hardwareColour.setBounds(module.removeFromLeft(module.getWidth() * 47 / 100));
        module.removeFromLeft(juce::roundToInt(18 * uiScale));
        auto controls = module;
        auto menuBounds = controls.removeFromTop(modeHeight);
        for (auto& menu : distortionModes) menu.setBounds(menuBounds.reduced(juce::roundToInt(12 * uiScale), 0));
        controls.removeFromTop(controlGap);
        auto utility = controls.removeFromBottom(dcReserve);
        const auto size = fire::ui::ordinaryKnobWidth(uiScale, {(controls.getWidth() - controlGap * 3) / 4,
            controls.getHeight() - valueHeight});
        const auto height = fire::ui::ordinaryKnobHeight(size, uiScale);
        auto strip = controls.withSizeKeepingCentre(size * 4 + controlGap * 3, height);
        for (const char* name : {DRIVE_NAME, BIAS_NAME, REC_NAME, SHAPE_MIX_NAME})
        {modulatableSliderComponents.at(name)->setBounds(strip.removeFromLeft(size)); strip.removeFromLeft(controlGap);}
        dcFilterButton.setBounds(utility.withSizeKeepingCentre(juce::roundToInt(22 * uiScale), juce::roundToInt(20 * uiScale)));
        dcFilterLabel.setBounds(dcFilterButton.getBounds().translated(juce::roundToInt(25 * uiScale), 0).withWidth(juce::roundToInt(28 * uiScale)));
    }
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    updateDriveCompensationPresentation(false);
    if (safeThis) invalidateChromeCache();
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

    fire::ui::drawCanvas(cacheGraphics, getLocalBounds().toFloat(), fire::ui::skinFor(*this));
    drawMinimalSurface(cacheGraphics, *this, knobsAreaRect.getUnion(outputAreaRect).toFloat());

    const auto titleHeight = juce::roundToInt(22.0f * scale);
    const auto titleInset = juce::roundToInt(8.0f * scale);
    auto titleFor = [titleHeight, titleInset](juce::Rectangle<int> area)
    {
        area.reduce(titleInset, 0);
        return area.removeFromTop(juce::jmin(titleHeight, area.getHeight())).toFloat();
    };

    juce::String moduleTitle {oscSwitch.getToggleState() ? "DRIVE" : "EMPTY CHAIN"};
    if (shapeSwitch.getToggleState())
    {
        moduleTitle = "SHAPE";
    }
    else if (compressorSwitch.getToggleState())
    {
        moduleTitle = "COMPRESSOR";
    }
    else if (widthSwitch.getToggleState())
    {
        moduleTitle = "STEREO";
    }

    if (ottSwitch.getToggleState()) moduleTitle = "OTT";

    if (selectedInsert >= 0) moduleTitle = fire::effects::name(processor.getInsertEffectType(focusBandNum + 1, selectedInsert));
    drawMinimalTitle(cacheGraphics, *this, titleFor(tabAreaRect), "CHAIN");
    const bool expandedInsert = selectedInsert >= 0 && insertControls.usesExpandedLayout();
    const bool fullWidthInsert = selectedInsert >= 0 && insertControls.usesFullWidthLayout();
    drawMinimalTitle(cacheGraphics, *this,
                     titleFor(fullWidthInsert ? knobsAreaRect.getUnion(graphAreaRect) : knobsAreaRect),
                     expandedInsert ? "GRANULAR / CLOUDS" : moduleTitle);
    drawMinimalTitle(cacheGraphics, *this,
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

void BandPanel::configureGraphViewMenu()
{
    addAndMakeVisible(graphSelectorStrip);
    graphSelectorStrip.setInterceptsMouseClicks(false, true);
    graphSelectorStrip.addAndMakeVisible(graphViewLabel);
    graphSelectorStrip.addAndMakeVisible(graphViewMenu);
    graphViewLabel.setText("VIEW", juce::dontSendNotification);
    graphViewLabel.setBorderSize({});
    graphViewLabel.setInterceptsMouseClicks(false, false);
    graphViewLabel.setColour(juce::Label::textColourId, fire::ui::paletteFor(*this).textSecondary);
    graphViewMenu.setComponentID("band_graph_view");
    graphViewMenu.addItem("Auto: Waveform", 1);
    graphViewMenu.addItem("Waveform", 2);
    graphViewMenu.addItem("Transfer", 3);
    graphViewMenu.addItem("Meters", 4);
    graphViewMenu.addItem("Stereo", 5);
    graphViewMenu.setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface0);
    graphViewMenu.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    graphViewMenu.setColour(juce::ComboBox::textColourId, fire::ui::paletteFor(*this).textPrimary);
    graphViewMenu.setColour(juce::ComboBox::arrowColourId, fire::ui::colours::signalCool);
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    graphViewMenu.configurePopupSession(
        [safeThis] { return safeThis ? safeThis->graphViewGeneration : 0; },
        [safeThis]
        {
            return safeThis && safeThis->isShowing() && safeThis->isEnabled()
                && safeThis->graphSelectorStrip.isShowing()
                && ! (safeThis->selectedInsert >= 0 && safeThis->insertControls.usesFullWidthLayout());
        },
        [safeThis](int itemId) { if (safeThis) safeThis->selectGraphView(itemId); });
    updateGraphViewMenu();
}

void BandPanel::dismissGraphViewMenu() noexcept
{
    ++graphViewGeneration;
    graphViewMenu.dismissTransientInteraction();
}

void BandPanel::selectGraphView(int itemId)
{
    if (! juce::isPositiveAndBelow(itemId - 1, 5) || ! graphSelectorStrip.isShowing())
        return;
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    const auto context = graphViewGeneration;
    if (driveGraphPreviewPhase != DriveGraphPreviewPhase::idle)
        getDriveKnob()->dismissTransientInteraction();
    if (! safeThis || graphViewGeneration != context) return;
    const bool keepZoomed = zoomedGraph != nullptr;
    clearGraphZoom();
    if (! safeThis || graphViewGeneration != context) return;
    dismissGraphViewMenu();
    if (! safeThis) return;
    selectedGraphView = itemId;
    const auto selectionGeneration = graphViewGeneration;
    applySelectedGraphView();
    if (! safeThis || graphViewGeneration != selectionGeneration) return;
    resized();
    if (! safeThis || graphViewGeneration != selectionGeneration) return;
    if (keepZoomed)
        toggleGraphZoom(getSelectedModuleGraph());
}

void BandPanel::applySelectedGraphView()
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    auto* selected = getSelectedModuleGraph();
    const bool showHardware = usesAnalogShape() && selectedGraphView == 1 && zoomedGraph == nullptr;
    const bool available = selected != nullptr;
    if (graphSelectorStrip.isVisible() != available)
        dismissGraphViewMenu();
    if (! safeThis) return;
    const auto context = graphViewGeneration;
    if (available && driveGraphPreviewPhase != DriveGraphPreviewPhase::idle)
        selected = &distortionGraph;
    for (auto* graph : { static_cast<GraphTemplate*>(&oscilloscope),
                         static_cast<GraphTemplate*>(&distortionGraph),
                         static_cast<GraphTemplate*>(&vuPanel),
                         static_cast<GraphTemplate*>(&widthGraph),
                         static_cast<GraphTemplate*>(&ottGraph) })
    {
        graph->setVisible(graph == selected);
        if (! safeThis || graphViewGeneration != context) return;
    }
    hardwareColour.setVisible(showHardware);
    if (!safeThis) return;
    if (shapeSwitch.getToggleState() && selectedInsert < 0)
        modulatableSliderComponents.at(DRIVE_NAME)->setVisible(usesAnalogShape() && zoomedGraph == nullptr);
    if (!safeThis) return;
    graphSelectorStrip.setVisible(available);
    if (safeThis && graphViewGeneration == context)
        updateGraphViewMenu();
}

void BandPanel::updateGraphViewMenu()
{
    const auto graphName = [this](GraphTemplate* graph) -> juce::String
    {
        if (graph == &distortionGraph) return "Transfer";
        if (graph == &vuPanel) return "Meters";
        if (graph == &widthGraph) return "Stereo";
        if (graph == &ottGraph) return "OTT dynamics";
        if (graph == nullptr && usesAnalogShape()) return "Hardware";
        return "Waveform";
    };
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    graphViewMenu.changeItemText(1, "Auto: " + graphName(getAutomaticModuleGraph()));
    if (! safeThis) return;
    const bool previewing = driveGraphPreviewPhase != DriveGraphPreviewPhase::idle;
    graphViewMenu.setSelectedId(previewing ? 3 : selectedGraphView, juce::dontSendNotification);
    if (! safeThis) return;
    graphViewMenu.setTitle("Band " + juce::String(focusBandNum + 1) + " graph view");
    graphViewMenu.setTooltip(previewing
        ? "Transfer preview while adjusting Drive. Release to restore your chosen view."
        : "Choose Waveform, Transfer, Meters or Stereo. Auto follows the selected module.");
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

    dismissGraphViewMenu();
    if (! safeThis) return;

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
        if (component == &graph || component == &graphSelectorStrip || component == nullptr
            || component->getBounds().isEmpty()
            || ! cover.intersects(component->getBounds())
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
    if (selectedInsert >= 0 && insertControls.usesFullWidthLayout()) return nullptr;
    switch (selectedGraphView)
    {
        case 2: return &oscilloscope;
        case 3: return &distortionGraph;
        case 4: return &vuPanel;
        case 5: return &widthGraph;
        default: return getAutomaticModuleGraph();
    }
}

GraphTemplate* BandPanel::getAutomaticModuleGraph() noexcept
{
    if (selectedInsert >= 0) return insertControls.usesFullWidthLayout() ? nullptr : &oscilloscope;
    if (ottSwitch.getToggleState()) return &ottGraph;
    if (shapeSwitch.getToggleState())
        return usesAnalogShape() ? nullptr : &distortionGraph;
    if (compressorSwitch.getToggleState())
        return &vuPanel;
    if (widthSwitch.getToggleState())
        return &widthGraph;
    return oscSwitch.getToggleState() ? &oscilloscope : nullptr;
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
    {
        updateGraphViewMenu();
        return;
    }

    const juce::Component::SafePointer<BandPanel> safeThis(this);
    distortionGraph.setVisible(false);
    if (safeThis != nullptr)
        graphToRestore->setVisible(true);
    if (safeThis != nullptr)
        updateGraphViewMenu();
}

void BandPanel::setAnimatedModuleTarget(int moduleIndex)
{
    moduleIndex = juce::jlimit(0, 4, moduleIndex);
    const auto targetPosition = static_cast<float>(moduleIndex);
    if (fire::ui::isVintage(*this))
    {
        moduleSelectionPosition.snapTo(targetPosition);
        repaint();
        return;
    }
    if (juce::approximatelyEqual(moduleSelectionPosition.target, targetPosition))
        return;
    if (isShowing())
        moduleSelectionPosition.setTarget(targetPosition);
    else
        moduleSelectionPosition.snapTo(targetPosition);
    repaint(knobsAreaRect.getUnion(graphAreaRect));
}

void BandPanel::selectInsertEffect(int slot)
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    if (slot < 0 || processor.getInsertEffectType(focusBandNum + 1, slot) == fire::effects::Type::none)
    {
        clearGraphZoom();
        if (!safeThis) return;
        selectedInsert = -1;
        insertControls.setActive(false);
        if (!safeThis) return;
        for (auto* button : {&oscSwitch, &shapeSwitch, &compressorSwitch, &widthSwitch, &ottSwitch}) button->setToggleState(false, juce::dontSendNotification);
        for (auto* group : {&driveComponents, &shapeComponents, &compressorComponents, &widthComponents, &ottComponents})
        {setVisibility(*group, false); if (!safeThis) return;}
        applySelectedGraphView();
        if (safeThis) resized();
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
    insertControls.bind(focusBandNum + 1, slot);
    if (! safeThis) return;
    insertControls.setActive(true);
    if (! safeThis) return;
    applySelectedGraphView();
    if (! safeThis) return;
    modulatableSliderComponents.at(OUTPUT_NAME)->setInteractionOnlyReadout(true);
    modulatableSliderComponents.at(MIX_NAME)->setInteractionOnlyReadout(true);
    repaint(knobsAreaRect.getUnion(graphAreaRect));
    resized(); invalidateChromeCache();
    auto callback = onModuleChanged;
    if (callback) callback();
}

void BandPanel::refreshInsertLayout()
{
    if (selectedInsert < 0) return;
    const auto expectedSlot = selectedInsert;
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    clearGraphZoom();
    if (! safeThis || selectedInsert != expectedSlot) return;
    applySelectedGraphView();
    if (! safeThis) return;
    resized();
    if (! safeThis) return;
    invalidateChromeCache();
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
    if (hardwareColour.isShowing())
        hardwareColour.setState(processor.getShapeMode(focusBandNum + 1) - fire::analog::legacyCount,
            static_cast<float>(getDriveKnob()->getValue()), juce::jmax(processor.getBandInputPeakLevel(focusBandNum, 0), processor.getBandInputPeakLevel(focusBandNum, 1)), deltaSeconds, processor.getAudioActivitySequence());
    updateDriveCompensationPresentation();
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
    if (ottGraph.isShowing())
    {
        bool reading = spectrumOttInteraction != 0;
        for (auto* name : ParameterIDAndName::ottControlNames)
            reading = reading || modulatableSliderComponents.at(name)->isValueReadoutRequested();
        ottGraph.advanceVisuals(deltaSeconds, reading, getOttPreviewDirection() | spectrumOttInteraction);
    }
    else ottGraph.resetVisuals();
    if (! isShowing())
    {
        moduleSelectionPosition.snapTo(moduleSelectionPosition.target);
        return;
    }

    if (fire::ui::isVintage(*this))
    {
        const auto oldBounds = getModuleSelectionBounds(moduleSelectionPosition.current);
        const bool moved = !juce::approximatelyEqual(moduleSelectionPosition.current,
                                                     moduleSelectionPosition.target);
        moduleSelectionPosition.snapTo(moduleSelectionPosition.target);
        if (moved)
            repaint(oldBounds.getUnion(getModuleSelectionBounds(moduleSelectionPosition.current))
                        .expanded(3.0f * scale).getSmallestIntegerContainer());
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
    if (! dismiss(upgradeDriveCompButton))
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
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    dismissGraphViewMenu();
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

    dismissButtonInteractions();
    if (safeThis == nullptr)
        return;

    invalidateDistortionModeInteractions();
}

void BandPanel::dismissTransientInteractionForParameterRebind() noexcept
{
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    dismissGraphViewMenu();
    if (! safeThis) return;
    insertControls.dismissButtons();
    if (! safeThis) return;
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

void BandPanel::enablementChanged()
{
    juce::Component::enablementChanged();
    dismissGraphViewMenu();
}

void BandPanel::parentHierarchyChanged()
{
    juce::Component::parentHierarchyChanged();
    dismissGraphViewMenu();
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

    linkedButton.setComponentID(ParameterIDAndName::getIDString(LINKED_ID, focusBandNum));
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
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    setBandKnobsStates(bandEnabled, false);
    if (safeThis) updateDriveCompensationPresentation();
}

void BandPanel::initFlatButton(juce::TextButton& button, juce::String buttonName)
{
    addAndMakeVisible(button);
    button.setClickingTogglesState(true);
    button.setComponentID("rounded");
    button.setColour(juce::TextButton::buttonColourId, fire::ui::paletteFor(*this).surface1);
    button.setColour(juce::TextButton::buttonOnColourId, fire::ui::paletteFor(*this).raised);
    button.setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    button.setColour(juce::TextButton::textColourOnId, fire::ui::colours::gold);
    button.setColour(juce::TextButton::textColourOffId, fire::ui::paletteFor(*this).textSecondary);
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
        dismissGraphViewMenu();
        if (! safeThis) return;
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
        applySelectedGraphView();
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
    effectNavigation.refresh();
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
    values.mode = processor.getShapeMode(focusBandNum + 1);

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

bool BandPanel::usesModernDriveCompensation() const noexcept
{
    if (! juce::isPositiveAndBelow(focusBandNum, 4)) return false;
    const auto* parameter = driveCompParameters[static_cast<size_t>(focusBandNum)].modern;
    return parameter != nullptr && parameter->load(std::memory_order_relaxed) > 0.5f;
}

void BandPanel::updateDriveCompensationPresentation(bool updateLayout)
{
    if (! juce::isPositiveAndBelow(focusBandNum, 4)) return;
    const juce::Component::SafePointer<BandPanel> safe(this);
    const auto band = focusBandNum;
    const auto& parameters = driveCompParameters[static_cast<size_t>(band)];
    const auto read = [](const std::atomic<float>* parameter, float fallback)
    { return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback; };
    const bool modern = usesModernDriveCompensation();
    const bool layoutChanged = displayedDriveCompMode != static_cast<int>(modern) || displayedDriveCompBand != band;
    if (layoutChanged)
    {
        displayedDriveCompMode = static_cast<int>(modern);
        displayedDriveCompBand = band;
        linkedButton.dismissPointerGesture();
        if (! safe || focusBandNum != band) return;
        upgradeDriveCompButton.dismissPointerGesture();
        if (! safe || focusBandNum != band) return;
        linkedButton.setButtonText(modern ? "Gain Comp" : "Legacy Link");
        linkedButton.setTitle((modern ? "Drive volume compensation for Band " : "Legacy Drive to Output link for Band ") + juce::String(band + 1));
        linkedButton.setTooltip(modern
            ? "Drive volume compensation: a coarse gain adjustment that follows Drive modulation, Safe, Extreme and bypass. Output stays independently adjustable."
            : "Legacy Link overrides the manual Output setting with gain linked to Drive. Use Drive Comp moves compensation into Drive and restores manual Output.");
        linkedButton.setHelpText(linkedButton.getTooltip());
        driveCompReadout.setTitle("Drive volume compensation for Band " + juce::String(band + 1));
    }
    const bool drivePage = selectedInsert < 0 && oscSwitch.getToggleState() && zoomedGraph == nullptr;
    for (auto* component : std::array<juce::Component*, 3> {&linkedButton, &driveCompReadout, &upgradeDriveCompButton})
    {
        const bool visible = drivePage && (component != &upgradeDriveCompButton || ! modern);
        if (! dismissInteractionBeforeComponentStateChange(component, component->isVisible() != visible, safe)) return;
        component->setVisible(visible);
        if (! safe || focusBandNum != band) return;
    }
    const bool enabled = read(parameters.linked, 1.0f) > 0.5f;
    juce::String text, help;
    if (! modern)
    {
        text = enabled ? "Output linked" : "Manual Output";
        help = enabled ? "Legacy Link is active: the stored manual Output setting is overridden."
                       : "Legacy Link is off: the stored manual Output setting is active.";
    }
    else if (! enabled)
    {
        text = "Off";
        help = "Drive compensation is off. Output remains manually adjustable.";
    }
    else if (read(parameters.bandEnabled, 1.0f) <= 0.5f || read(parameters.driveEnabled, 1.0f) <= 0.5f)
    {
        text = "Bypassed";
        help = "Drive compensation is bypassed with this band or the Drive stage.";
    }
    else
    {
        const auto index = static_cast<size_t>(band);
        const auto sequence = processor.getBandDriveCompensationSequence(band);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (sequence == 0)
        {
            driveCompSequences[index] = 0;
            driveCompReceivedAtMs[index] = -1.0;
        }
        else if (driveCompSequences[index] != sequence)
        {
            driveCompSequences[index] = sequence;
            driveCompReceivedAtMs[index] = now;
        }
        auto compensation = processor.getBandDriveCompensationDb(band);
        const bool estimated = sequence == 0 || ! std::isfinite(compensation)
                            || now - driveCompReceivedAtMs[index] >= driveCompTelemetryTimeoutMs;
        if (estimated)
        {
            const auto drive = read(parameters.drive, 0.0f);
            compensation = -0.1f * (std::isfinite(drive) ? drive : 0.0f);
            if (read(parameters.extreme, 0.0f) > 0.5f) compensation *= std::log2(10.0f);
        }
        if (std::abs(compensation) < 0.05f) compensation = 0.0f;
        text = (estimated ? juce::String::charToString(0x2248) + " " : juce::String())
             + (compensation < 0.0f ? juce::String::charToString(0x2212) : juce::String())
             + juce::String(std::abs(compensation), 1) + " dB";
        help = estimated
            ? "Estimated from base Drive and Extreme while audio updates are unavailable. Drive modulation and Safe limiting are reflected when processing resumes. Output remains manual."
            : "Applied Drive compensation from the latest processed audio, including Drive modulation, Safe and Extreme. This is a gain estimate, not a loudness measurement. Output remains manual.";
    }
    if (driveCompReadout.getText() != text) driveCompReadout.setText(text, juce::dontSendNotification);
    if (! safe || focusBandNum != band) return;
    if (driveCompReadout.getTooltip() != help)
    {
        driveCompReadout.setTooltip(help);
        driveCompReadout.setHelpText(help);
    }
    if (updateLayout && layoutChanged) resized();
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
            const bool compensationControl = component == &linkedButton || component == &driveCompReadout
                                             || component == &upgradeDriveCompButton;
            if (! setComponentEnabled(component, compensationControl || driveIsEnabled))
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

        bool affectsGraph = isDriveParameter || parameterID == fire::analog_params::bandID(bandIndex);
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
    combobox->setColour(juce::ComboBox::backgroundColourId, fire::ui::paletteFor(*this).surface1);
    combobox->setColour(juce::ComboBox::outlineColourId, fire::ui::paletteFor(*this).hairline);
    combobox->setColour(juce::ComboBox::textColourId, fire::ui::paletteFor(*this).textPrimary);
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
    combobox->addSectionHeading("Analog Hardware");
    for (int model = 0; model < fire::analog::count; ++model) combobox->addItem(fire::analog::names[static_cast<size_t>(model)], fire::analog::legacyCount + model + 1);
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
    if (usesAnalogShape() && selectedGraphView == 1) return;
    const juce::Component::SafePointer<BandPanel> safeThis(this);
    if (isDragging)
    {
        // A Slider emits one main-drag start per accepted gesture. Ignore a
        // duplicate notification rather than replacing the graph owned by the
        // current preview session with its temporary transfer graph.
        if (driveGraphPreviewPhase != DriveGraphPreviewPhase::idle)
            return;

        dismissGraphViewMenu();
        if (! safeThis) return;

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
        updateGraphViewMenu();
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
