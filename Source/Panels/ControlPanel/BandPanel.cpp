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
    g.setFont(fire::ui::labelFont(juce::jlimit(9.0f, 12.0f, bounds.getHeight() * 0.36f)));
    g.setColour(fire::ui::colours::textSecondary.withAlpha(0.82f));
    g.drawText(text.toUpperCase(), bounds, juce::Justification::centredLeft);
}

juce::Colour moduleColourForIndex(int index)
{
    switch (index)
    {
        case 1:  return fire::ui::colours::shape;
        case 2:  return fire::ui::colours::compressor;
        case 3:  return fire::ui::colours::stereo;
        default: return fire::ui::colours::drive;
    }
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
    constexpr std::array<const char*, distortionGraphParameterCount> graphParameterBases {
        REC_ID, MIX_ID, SHAPE_MIX_ID, BIAS_ID, MODE_ID, SAFE_ID
    };

    for (int i = 0; i < 4; ++i)
    {
        driveParameterIds[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(DRIVE_ID, i);
        linkedParameterIds[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(LINKED_ID, i);
        outputParameterIds[static_cast<size_t>(i)] = ParameterIDAndName::getIDString(OUTPUT_ID, i);
        processor.treeState.addParameterListener(driveParameterIds[static_cast<size_t>(i)], this);
        processor.treeState.addParameterListener(linkedParameterIds[static_cast<size_t>(i)], this);

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
    moduleSelectionColourMix.snapTo(1.0f);
    buttonClicked(&oscSwitch);
    startTimerHz(30);
}

BandPanel::~BandPanel()
{
    // Remove all parameter listeners that were added in the constructor.
    for (int i = 0; i < 4; ++i)
    {
        processor.treeState.removeParameterListener(driveParameterIds[static_cast<size_t>(i)], this);
        processor.treeState.removeParameterListener(linkedParameterIds[static_cast<size_t>(i)], this);

        for (const auto& parameterIds : distortionGraphParameterIds)
            processor.treeState.removeParameterListener(parameterIds[static_cast<size_t>(i)], this);
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
    createAndConfigureSlider(DRIVE_NAME, "Drive", fire::ui::colours::drive);
    createAndConfigureSlider(OUTPUT_NAME, "Output", fire::ui::colours::flame, " dB");
    createAndConfigureSlider(MIX_NAME, "Mix", fire::ui::colours::gold);

    // Shape Panel
    createAndConfigureSlider(REC_NAME, "Rectification", fire::ui::colours::shape);
    createAndConfigureSlider(BIAS_NAME, "Bias", fire::ui::colours::shape);
    createAndConfigureSlider(SHAPE_MIX_NAME, "Mix", fire::ui::colours::shape);

    // Compressor Panel
    createAndConfigureSlider(COMP_THRESH_NAME, "Threshold", fire::ui::colours::compressor, " dB");
    createAndConfigureSlider(COMP_RATIO_NAME, "Ratio", fire::ui::colours::compressor);
    createAndConfigureSlider(COMP_ATTACK_NAME, "Attack", fire::ui::colours::compressor, " ms");
    createAndConfigureSlider(COMP_RELEASE_NAME, "Release", fire::ui::colours::compressor, " ms");
    createAndConfigureSlider(COMP_MIX_NAME, "Mix", fire::ui::colours::compressor);

    // Width Panel
    createAndConfigureSlider(WIDTH_NAME, "Width", fire::ui::colours::stereo);
    createAndConfigureSlider(PAN_NAME, "Pan", fire::ui::colours::stereo);
    createAndConfigureSlider(WIDTH_MIX_NAME, "Mix", fire::ui::colours::stereo);

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

    initBypassButton(dcFilterButton, fire::ui::colours::shape);

    auto setupSwitch = [this](juce::TextButton& btn, const juce::String& text, juce::Colour colour)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(switchButtons);
        btn.getProperties().set("fireAnimatedSelection", true);

        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId, colour);

        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, fire::ui::colours::textPrimary);

        btn.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

        btn.addListener(this);
    };

    setupSwitch(oscSwitch, "Drive", fire::ui::colours::drive);
    setupSwitch(shapeSwitch, "Shape", fire::ui::colours::shape);
    setupSwitch(compressorSwitch, "Compressor", fire::ui::colours::compressor);
    setupSwitch(widthSwitch, "Stereo", fire::ui::colours::stereo);
    oscSwitch.setToggleState(true, juce::dontSendNotification);

    driveBypassButton.toFront(false);
    shapeBypassButton.toFront(false);
    compressorBypassButton.toFront(false);
    widthBypassButton.toFront(false);
}

void BandPanel::createComboBoxes()
{
    for (size_t i = 0; i < distortionModes.size(); ++i)
    {
        setMenu(&distortionModes[i]);
        modeAttachments[i] = std::make_unique<ComboBoxAttachment>(
            processor.treeState,
            ParameterIDAndName::getIDString(MODE_ID, static_cast<int>(i)),
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
    const auto displayScale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (chromeCacheDirty
        || ! juce::approximatelyEqual(chromeCacheDisplayScale, displayScale)
        || chromeCache.isNull())
        rebuildChromeCache(displayScale);

    if (! chromeCache.isNull())
        g.drawImage(chromeCache, getLocalBounds().toFloat());

    const auto selectionBounds = getModuleSelectionBounds(moduleSelectionPosition.current);
    if (! selectionBounds.isEmpty())
    {
        const auto accent = getModuleSelectionColour();
        const auto radius = juce::jmin(selectionBounds.getHeight() * 0.24f,
                                       fire::ui::Metrics::radius * scale);

        g.setColour(accent.withAlpha(0.09f));
        g.fillRoundedRectangle(selectionBounds, radius);
        g.setColour(accent.withAlpha(0.72f));
        g.drawRoundedRectangle(selectionBounds.reduced(0.75f), radius, 1.5f * scale);
    }
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

    auto switchColumnArea = contentArea(tabAreaRect);
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
    const int secondaryKnobSize = juce::jmax(1, std::min({
        juce::roundToInt(KNOB_SIZE * 0.82f * uiScale),
        (knobsColumnArea.getWidth() - controlGap * 2) / 3,
        (knobsColumnArea.getHeight() - controlGap) / 2,
        knobsColumnArea.getHeight() - modeHeight - controlGap - dcReserve,
        (outputColumnArea.getWidth() - controlGap) / 2,
        outputColumnArea.getHeight() - buttonAreaHeight - controlGap
    }));

    // --- Module rail ---
    juce::FlexBox switchColumnBox;
    switchColumnBox.flexDirection = juce::FlexBox::Direction::column;
    switchColumnBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    const auto rowMargin = juce::FlexItem::Margin(juce::jmax(1.0f, 2.0f * uiScale));
    switchColumnBox.items.add(juce::FlexItem(oscSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.items.add(juce::FlexItem(shapeSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.items.add(juce::FlexItem(compressorSwitch).withFlex(1.0f).withMargin(rowMargin));
    switchColumnBox.items.add(juce::FlexItem(widthSwitch).withFlex(1.0f).withMargin(rowMargin));
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

    layoutBypassButton(driveBypassButton, oscSwitch);
    layoutBypassButton(shapeBypassButton, shapeSwitch);
    layoutBypassButton(compressorBypassButton, compressorSwitch);
    layoutBypassButton(widthBypassButton, widthSwitch);

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
                                                             secondaryKnobSize + dcReserve);
        knobRow = knobRow.removeFromTop(secondaryKnobSize);
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
                                                                  secondaryKnobSize * 2 + controlGap);
        auto topRow = centeredArea.removeFromTop(secondaryKnobSize);
        centeredArea.removeFromTop(controlGap);
        auto bottomRow = centeredArea.removeFromTop(secondaryKnobSize);

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
                                                             secondaryKnobSize);
        modulatableSliderComponents.at(WIDTH_NAME)->setBounds(knobRow.removeFromLeft(secondaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(PAN_NAME)->setBounds(knobRow.removeFromLeft(secondaryKnobSize));
        knobRow.removeFromLeft(controlGap);
        modulatableSliderComponents.at(WIDTH_MIX_NAME)->setBounds(knobRow);
    }

    // --- Live visualiser card ---
    oscilloscope.setBounds(graphColumnArea);
    distortionGraph.setBounds(graphColumnArea);
    vuPanel.setBounds(graphColumnArea);
    widthGraph.setBounds(graphColumnArea);

    // --- Output card ---
    auto buttonArea = outputColumnArea.removeFromBottom(buttonAreaHeight);
    outputColumnArea.removeFromBottom(juce::jmin(controlGap, outputColumnArea.getHeight()));
    auto twoKnobsBounds = outputColumnArea.withSizeKeepingCentre(secondaryKnobSize * 2 + controlGap,
                                                                 secondaryKnobSize);
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

    drawMinimalTitle(cacheGraphics, titleFor(tabAreaRect), "MODULE");
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

void BandPanel::setAnimatedModuleTarget(int moduleIndex)
{
    moduleIndex = juce::jlimit(0, 3, moduleIndex);
    const auto targetColour = moduleColourForIndex(moduleIndex);
    const auto targetPosition = static_cast<float>(moduleIndex);

    if (juce::approximatelyEqual(moduleSelectionPosition.target, targetPosition)
        && moduleSelectionColourTarget == targetColour)
        return;

    moduleSelectionColourStart = getModuleSelectionColour();
    moduleSelectionColourTarget = targetColour;
    moduleSelectionColourMix.snapTo(0.0f);
    moduleSelectionColourMix.setTarget(1.0f);
    moduleSelectionPosition.setTarget(targetPosition);
}

juce::Rectangle<float> BandPanel::getModuleSelectionBounds(float modulePosition) const
{
    const std::array<const juce::TextButton*, 4> switches {
        &oscSwitch, &shapeSwitch, &compressorSwitch, &widthSwitch
    };

    modulePosition = juce::jlimit(0.0f, 3.0f, modulePosition);
    const auto lowerIndex = juce::jlimit(0, 3, static_cast<int>(std::floor(modulePosition)));
    const auto upperIndex = juce::jmin(3, lowerIndex + 1);
    const auto mix = modulePosition - static_cast<float>(lowerIndex);
    const auto lower = switches[static_cast<size_t>(lowerIndex)]->getBounds().toFloat();
    const auto upper = switches[static_cast<size_t>(upperIndex)]->getBounds().toFloat();

    if (lower.isEmpty())
        return {};

    return { juce::jmap(mix, lower.getX(), upper.getX()),
             juce::jmap(mix, lower.getY(), upper.getY()),
             juce::jmap(mix, lower.getWidth(), upper.getWidth()),
             juce::jmap(mix, lower.getHeight(), upper.getHeight()) };
}

juce::Colour BandPanel::getModuleSelectionColour() const
{
    return moduleSelectionColourStart.interpolatedWith(
        moduleSelectionColourTarget,
        juce::jlimit(0.0f, 1.0f, moduleSelectionColourMix.current));
}

void BandPanel::animationTick(float deltaSeconds)
{
    if (! isShowing())
    {
        moduleSelectionPosition.snapTo(moduleSelectionPosition.target);
        moduleSelectionColourMix.snapTo(moduleSelectionColourMix.target);
        return;
    }

    if (moduleSelectionPosition.isSettled() && moduleSelectionColourMix.isSettled())
        return;

    const auto oldBounds = getModuleSelectionBounds(moduleSelectionPosition.current);
    moduleSelectionPosition.advance(deltaSeconds, 0.07f);
    moduleSelectionColourMix.advance(deltaSeconds, 0.07f);
    const auto newBounds = getModuleSelectionBounds(moduleSelectionPosition.current);

    repaint(oldBounds.getUnion(newBounds).expanded(3.0f * scale)
                .getSmallestIntegerContainer());
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
    bool isSwitch = false;
    if (clickedButton == &oscSwitch && oscSwitch.getToggleState())
    {
        setAnimatedModuleTarget(0);
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
        setAnimatedModuleTarget(1);
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
        setAnimatedModuleTarget(2);
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
        setAnimatedModuleTarget(3);
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
        const auto* bandEnabledParameter = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
        const bool isBandEnabled = bandEnabledParameter != nullptr
                                   && bandEnabledParameter->load() > 0.5f;
        setBandKnobsStates(isBandEnabled, true);
        return;
    }

    if (isSwitch)
    {
        resized();
        invalidateChromeCache();
    }
}

void BandPanel::setFocusBandNum(int num, bool forceUpdate)
{
    if (! juce::isPositiveAndBelow(num, 4))
    {
        jassertfalse;
        return;
    }

    vuPanel.setFocusBandNum(num);
    if (focusBandNum == num && ! forceUpdate)
        return;

    processor.setUiFocusBand(num);

    focusBandNum = num;
    updateAttachments();
    updateWhenChangingFocus();

    const auto* bandEnabledParameter = processor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(BAND_ENABLE_ID, focusBandNum));
    const bool isBandEnabled = bandEnabledParameter != nullptr
                               && bandEnabledParameter->load() > 0.5f;
    setBandKnobsStates(isBandEnabled, false);

    updateDistortionModeVisibility();
    updateDistortionGraphFromParameters();
    invalidateChromeCache();
}

void BandPanel::updateLinkedValue(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4))
        return;

    const auto index = static_cast<size_t>(bandIndex);
    const auto* linked = processor.treeState.getRawParameterValue(linkedParameterIds[index]);
    const auto* drive = processor.treeState.getRawParameterValue(driveParameterIds[index]);
    auto* output = processor.treeState.getParameter(outputParameterIds[index]);

    if (linked == nullptr || drive == nullptr || output == nullptr || linked->load() <= 0.5f)
        return;

    const float newOutputValue = -drive->load() * 0.1f;
    const float normalisedValue = output->convertTo0to1(newOutputValue);
    if (! juce::approximatelyEqual(output->getValue(), normalisedValue))
        output->setValueNotifyingHost(normalisedValue);
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

    const float driveForCalc = readBandParameter(DRIVE_ID, 0.0f) * 6.5f / 100.0f;
    const float powerDrive = std::pow(2.0f, driveForCalc);
    const float sampleMaxValue = processor.getSampleMaxValue(focusBandNum);
    const bool safeMode = readBandParameter(SAFE_ID, 1.0f) > 0.5f;
    values.drive = safeMode && sampleMaxValue > 0.0001f && sampleMaxValue * powerDrive > 2.0f
                       ? 2.0f / sampleMaxValue + 0.1f * driveForCalc
                       : powerDrive;

    const auto* downsample = processor.treeState.getRawParameterValue(DOWNSAMPLE_ID);
    const auto* downsampleBypass = processor.treeState.getRawParameterValue(DOWNSAMPLE_BYPASS_ID);
    values.rateDivide = downsample != nullptr ? downsample->load() : 1.0f;
    if (downsampleBypass != nullptr && downsampleBypass->load() > 0.5f)
        values.rateDivide = 1.0f;

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

void BandPanel::setBandKnobsStates(bool isBandEnabled, bool /*callFromSubBypass*/)
{
    for (auto* component : allControls)
    {
        component->setEnabled(isBandEnabled);
    }

    if (isBandEnabled)
    {
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
        if (isDriveParameter || parameterID == linkedParameterIds[index])
            linkedValueDirtyMask.fetch_or(bandMask, std::memory_order_release);

        bool affectsGraph = isDriveParameter;
        for (const auto& parameterIds : distortionGraphParameterIds)
            affectsGraph = affectsGraph || parameterID == parameterIds[index];

        if (affectsGraph)
        {
            distortionGraphDirtyMask.fetch_or(bandMask, std::memory_order_release);
            return;
        }

        if (parameterID == linkedParameterIds[index])
            return;
    }
}

void BandPanel::timerCallback()
{
    // Link is an audio/control semantic, so it must remain live even while the
    // page is hidden. The work below is coalesced by band and only writes when
    // the derived output value actually changed.
    const auto dirtyMask = linkedValueDirtyMask.exchange(0, std::memory_order_acq_rel);
    for (int bandIndex = 0; bandIndex < 4; ++bandIndex)
        if ((dirtyMask & (1u << static_cast<unsigned int>(bandIndex))) != 0)
            updateLinkedValue(bandIndex);

    // The transfer curve is presentation-only. Keep its dirty bits pending
    // while hidden and rebuild just that graph once it can actually be seen.
    if (! isShowing() || ! distortionGraph.isShowing())
        return;

    const auto graphDirtyMask = distortionGraphDirtyMask.exchange(0, std::memory_order_acq_rel);
    if (juce::isPositiveAndBelow(focusBandNum, 4)
        && (graphDirtyMask & (1u << static_cast<unsigned int>(focusBandNum))) != 0)
        updateDistortionGraphFromParameters();
}

void BandPanel::comboBoxChanged(juce::ComboBox*)
{
    // Logic for combo box changes if any
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
    combobox->addListener(this);
}

void BandPanel::updateDistortionModeVisibility()
{
    const bool shouldShowAny = shapeSwitch.getToggleState();

    for (size_t i = 0; i < distortionModes.size(); ++i)
    {
        distortionModes[i].setVisible(shouldShowAny
                                      && focusBandNum == static_cast<int>(i));
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
