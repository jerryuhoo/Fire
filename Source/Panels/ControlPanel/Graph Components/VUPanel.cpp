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
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

    vuMeterIn.setParameters(true, 0);
    vuMeterOut.setParameters(false, 0);

    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);

    startTimerHz(60);
}

VUPanel::~VUPanel()
{
}

void VUPanel::paint(juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1); // draw an outline around the component

    // draw compressor threshold line
    g.setColour(KNOB_SUBFONT_COLOUR);

    bool isGlobal = false;
    // This call is the same for all cases, so we can do it once at the start.
    vuMeterIn.setParameters(true, focusBandNum);
    vuMeterOut.setParameters(false, focusBandNum);

    // Check if the focusBandNum corresponds to a valid band (0, 1, 2, or 3).
    if (juce::isPositiveAndBelow(focusBandNum, 4))
    {
        // Use arrays to look up the correct parameter IDs based on the index.
        // This replaces the long if-else chain.
        threshID = ParameterIDAndName::getIDString(COMP_THRESH_ID, focusBandNum);
        compBypassID = ParameterIDAndName::getIDString(COMP_BYPASS_ID, focusBandNum);
    }
    else if (focusBandNum == -1)
    {
        // Handle the special case for the global view.
        isGlobal = true;
    }
    else
    {
        // Assert if the focusBandNum is an unexpected value (e.g., 4, 5, -2).
        jassertfalse;
    }

    // draw threshold pointer
    if (! isGlobal)
    {
        // Use the realtimeThresholdDb member, which is updated by the editor's timer.
        float threshValue = realtimeThresholdDb;

        const float vuMeterRange = 82.0f; // Range from -70dB to +12dB
        float compressorLineY = VU_METER_Y + VU_METER_HEIGHT * (1.0f - (threshValue + 70.0f) / vuMeterRange);

        float pointerX = (processor.getTotalNumInputChannels() == 2) ? VU_METER_X_1 : VU_METER_X_1 + vuMeterIn.getWidth() / 3.0f;

        bool compIsEnabled = *processor.treeState.getRawParameterValue(compBypassID);
        if (compIsEnabled)
        {
            g.setColour(juce::Colours::yellowgreen);
            g.drawLine(pointerX + vuMeterIn.getWidth() / 3.0f,
                       compressorLineY,
                       pointerX + vuMeterIn.getWidth() / 3.0f * 2.0f,
                       compressorLineY,
                       1.0f);
        }
    }

    if (mZoomState)
    {
        // show db meter scale text
        float textX = (VU_METER_X_1 + VU_METER_X_2 - VU_METER_WIDTH) / 2.0f;
        float textWidth = getWidth() / 5;
        float textHeight = getHeight() / 10;
        g.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withHeight(14.0f * getHeight() / 150.0f)
                .withStyle("Plain") });
        g.drawText("-20", textX, VU_METER_Y + 20.0f / VU_METER_RANGE * VU_METER_HEIGHT - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-40", textX, VU_METER_Y + 40.0f / VU_METER_RANGE * VU_METER_HEIGHT - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-60", textX, VU_METER_Y + 60.0f / VU_METER_RANGE * VU_METER_HEIGHT - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-80", textX, VU_METER_Y + 80.0f / VU_METER_RANGE * VU_METER_HEIGHT - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);

        // --- Calculate RMS & Peak values in dB ---
        // Note: The VUMeter returns a normalized linear value [0, 1]. We must convert it to dB.
        auto toDB = [](float linear)
        { return juce::Decibels::gainToDecibels(linear, -96.0f); };

        float avgInputRmsDb = toDB((vuMeterIn.getLeftChannelLevel() + vuMeterIn.getRightChannelLevel()) * 0.5f);
        float avgInputPeakDb = toDB((vuMeterIn.getLeftChannelPeakLevel() + vuMeterIn.getRightChannelPeakLevel()) * 0.5f);

        float avgOutputRmsDb = toDB((vuMeterOut.getLeftChannelLevel() + vuMeterOut.getRightChannelLevel()) * 0.5f);
        float avgOutputPeakDb = toDB((vuMeterOut.getLeftChannelPeakLevel() + vuMeterOut.getRightChannelPeakLevel()) * 0.5f);

        // --- Define drawing areas ---
        juce::Rectangle<int> localBounds = getLocalBounds();
        juce::Rectangle<int> leftArea = localBounds.removeFromLeft(getWidth() / 4);
        juce::Rectangle<int> rightArea = localBounds.removeFromRight(getWidth() / 3);

        // --- Draw Input Levels (Left Side) ---
        g.setColour(juce::Colours::yellowgreen);

        // Large Text: Peak Value
        g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(20.0f * getHeight() / 150.0f).withStyle("Bold") });
        g.drawText(juce::String(avgInputPeakDb, 1), leftArea.withTrimmedBottom(leftArea.getHeight() / 2), juce::Justification::centredBottom);

        // Small Text: RMS Value
        g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(14.0f * getHeight() / 150.0f).withStyle("Plain") });
        g.drawText(juce::String(avgInputRmsDb, 1), leftArea.withTrimmedTop(leftArea.getHeight() / 2), juce::Justification::centredTop);

        // Label
        g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
        g.drawText("RMS In", leftArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);

        // --- Draw Output Levels (Right Side) ---
        g.setColour(juce::Colours::yellowgreen);

        // Large Text: Peak Value
        g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(20.0f * getHeight() / 150.0f).withStyle("Bold") });
        g.drawText(juce::String(avgOutputPeakDb, 1), rightArea.withTrimmedBottom(rightArea.getHeight() / 2), juce::Justification::centredBottom);

        // Small Text: RMS Value
        g.setFont(juce::Font { juce::FontOptions().withName(KNOB_FONT).withHeight(14.0f * getHeight() / 150.0f).withStyle("Plain") });
        g.drawText(juce::String(avgOutputRmsDb, 1), rightArea.withTrimmedTop(rightArea.getHeight() / 2), juce::Justification::centredTop);

        // Label
        g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
        g.drawText("RMS Out", rightArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);
    }

    if (isMouseOn && ! mZoomState)
    {
        g.setColour(COMP_COLOUR.withAlpha(0.05f));
        g.fillAll();
    }
}

void VUPanel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    vuMeterIn.setBounds(VU_METER_X_1, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
    vuMeterOut.setBounds(VU_METER_X_2, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
}

void VUPanel::setFocusBandNum(int num)
{
    focusBandNum = num;
}

void VUPanel::timerCallback()
{
    repaint();
}

void VUPanel::updateRealtimeThreshold(float newThresholdDb)
{
    // Store the live value. No need to repaint here, as the timerCallback already does.
    realtimeThresholdDb = newThresholdDb;
}