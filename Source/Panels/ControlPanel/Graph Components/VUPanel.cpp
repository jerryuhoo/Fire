/*
  ==============================================================================

    VUPanel.cpp
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VUPanel.h"
#include "../../../Utility/AudioHelpers.h"

//==============================================================================
VUPanel::VUPanel(FireAudioProcessor& p) : processor(p),
                                          focusBandNum(0),
                                          vuMeterIn(&p),
                                          vuMeterOut(&p),
                                          realtimeThresholdDb(-100.0f)
{
    vuMeterIn.setParameters(true, -1); // Default to global
    vuMeterOut.setParameters(false, -1);

    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);

    startTimerHz(60);
}

VUPanel::~VUPanel()
{
    stopTimer();
}

void VUPanel::paint(juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);

    // --- 1. Define layout variables consistently ---
    const float meterHeight = (float) getHeight() / 10.0f * 9.0f;
    const float meterY = (float) getHeight() / 10.0f;

    // --- 2. Create a unified dB-to-Y coordinate mapping function ---
    auto dbToY = [&](float db)
    {
        const float minDb = -96.0f;
        const float maxDb = 0.0f;
        db = juce::jlimit(minDb, maxDb, db);
        float normalizedPosition = (db - minDb) / (maxDb - minDb);
        return meterY + meterHeight * (1.0f - normalizedPosition);
    };

    // --- 3. Draw background scale ---
    g.setColour(KNOB_SUBFONT_COLOUR);
    auto textBounds = getLocalBounds().withX(vuMeterIn.getRight()).withRight(vuMeterOut.getX());
    float textHeight = 12.0f;
    g.setFont(textHeight);

    g.drawText("0", textBounds.withY(dbToY(0.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-24", textBounds.withY(dbToY(-24.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-48", textBounds.withY(dbToY(-48.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);
    g.drawText("-72", textBounds.withY(dbToY(-72.0f) - textHeight / 2.0f).withHeight(textHeight), juce::Justification::centred, false);

    // --- 4. Draw Compressor Threshold line ---
    bool isGlobal = (focusBandNum == -1);
    if (! isGlobal && juce::isPositiveAndBelow(focusBandNum, 4))
    {
        compBypassID = ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum);
        bool compIsEnabled = *processor.treeState.getRawParameterValue(compBypassID);
        if (compIsEnabled)
        {
            float compressorLineY = dbToY(realtimeThresholdDb);
            g.setColour(juce::Colours::yellowgreen);
            g.drawLine((float) textBounds.getX(), compressorLineY, (float) textBounds.getRight(), compressorLineY, 1.5f);
        }
    }

    // Update child component parameters
    vuMeterIn.setParameters(true, focusBandNum);
    vuMeterOut.setParameters(false, focusBandNum);

    // --- 5. Draw RMS and Peak readouts ---
    // This helper converts the meter's normalized [0,1] value back to dB for text display.
    auto normalizedToDb = [](float norm)
    {
        // CORRECTED: Use juce::jmap for linear mapping from [0, 1] back to [-96, 0] dB.
        // This fixes the jassert caused by using juce::mapToLog10 with negative values.
        return juce::jmap(norm, 0.0f, 1.0f, -96.0f, 0.0f);
    };

    float inputRmsDb = normalizedToDb(vuMeterIn.getRmsLeftChannelLevel());
    float inputPeakDb = normalizedToDb(vuMeterIn.getPeakLeftChannelLevel());
    float outputRmsDb = normalizedToDb(vuMeterOut.getRmsLeftChannelLevel());
    float outputPeakDb = normalizedToDb(vuMeterOut.getPeakLeftChannelLevel());

    auto leftArea = getLocalBounds().withRight(vuMeterIn.getX());
    auto rightArea = getLocalBounds().withLeft(vuMeterOut.getRight());

    g.setColour(juce::Colours::yellowgreen);
    auto fontSizeBig = 14.0f * getWidth() / 150.0f;
    auto fontSizeSmall = 10.0f * getWidth() / 150.0f;

    // Input Readouts
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeBig).withStyle("Bold") });
    g.drawText(juce::String(inputPeakDb, 1), leftArea.withTrimmedBottom(leftArea.getHeight() / 2), juce::Justification::centredBottom);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeSmall).withStyle("Plain") });
    g.drawText(juce::String(inputRmsDb, 1), leftArea.withTrimmedTop(leftArea.getHeight() / 2), juce::Justification::centredTop);

    g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
    g.setFont(fontSizeSmall);
    g.drawFittedText("In", leftArea.removeFromBottom(getHeight() / 4).toNearestInt(), juce::Justification::centredTop, 1);

    // Output Readouts
    g.setColour(juce::Colours::yellowgreen);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeBig).withStyle("Bold") });
    g.drawText(juce::String(outputPeakDb, 1), rightArea.withTrimmedBottom(rightArea.getHeight() / 2), juce::Justification::centredBottom);
    g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(fontSizeSmall).withStyle("Plain") });
    g.drawText(juce::String(outputRmsDb, 1), rightArea.withTrimmedTop(rightArea.getHeight() / 2), juce::Justification::centredTop);

    g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
    g.setFont(fontSizeSmall);
    g.drawFittedText("Out", rightArea.removeFromBottom(getHeight() / 4).toNearestInt(), juce::Justification::centredTop, 1);
}

void VUPanel::resized()
{
    const float width = (float) getWidth();
    const float height = (float) getHeight();

    const float meterWidth = width / 10.0f;
    const float meterHeight = height / 10.0f * 9.0f;
    const float meterY = height / 10.0f;
    const float meterX1 = width / 3.0f - meterWidth / 2.0f;
    const float meterX2 = width / 3.0f * 2.0f - meterWidth / 2.0f;

    vuMeterIn.setBounds(juce::Rectangle<float>(meterX1, meterY, meterWidth, meterHeight).toNearestInt());
    vuMeterOut.setBounds(juce::Rectangle<float>(meterX2, meterY, meterWidth, meterHeight).toNearestInt());
}

void VUPanel::setFocusBandNum(int num)
{
    focusBandNum = num;
}

void VUPanel::timerCallback()
{
    MeterValues latestValues;
    if (processor.getLatestMeterValues(latestValues))
    {
        vuMeterIn.updateLevels(latestValues);
        vuMeterOut.updateLevels(latestValues);
    }

    repaint();
}

void VUPanel::updateRealtimeThreshold(float newThresholdDb)
{
    realtimeThresholdDb = newThresholdDb;
}