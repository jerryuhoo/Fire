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
VUPanel::VUPanel(FireAudioProcessor& p) : processor(p), focusBandNum(0), vuMeterIn(&p), vuMeterOut(&p)
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
        // The original code didn't set threshID or compBypassID in this case,
        // so we can leave them as they are or assign a default if needed.
    }
    else
    {
        // Assert if the focusBandNum is an unexpected value (e.g., 4, 5, -2).
        jassertfalse;
    }

    // draw threshold pointer
    if (! isGlobal)
    {
        float threshValue = *(processor.treeState.getRawParameterValue(threshID));
        float compressorLineY = VU_METER_Y + VU_METER_HEIGHT * -threshValue / VU_METER_RANGE;
        float pointerX;
        if (processor.getTotalNumInputChannels() == 2)
        {
            pointerX = VU_METER_X_1;
        }
        else
        {
            pointerX = VU_METER_X_1 + vuMeterIn.getWidth() / 3.0f;
        }

        bool compBypassState = *(processor.treeState.getRawParameterValue(compBypassID));
        if (compBypassState)
        {
            g.setColour(juce::Colours::yellowgreen);
            g.drawLine(pointerX + vuMeterIn.getWidth() / 3.0f, compressorLineY, pointerX + vuMeterIn.getWidth() / 3.0f * 2.0f, compressorLineY, 1.0f);
        }
    }

    if (mZoomState)
    {
        // show db meter scale text
        float textX = (VU_METER_X_1 + VU_METER_X_2 - VU_METER_WIDTH) / 2.0f;
        float text20Y = VU_METER_Y + 20.0f / VU_METER_RANGE * VU_METER_HEIGHT;
        float text40Y = VU_METER_Y + 40.0f / VU_METER_RANGE * VU_METER_HEIGHT;
        float text60Y = VU_METER_Y + 60.0f / VU_METER_RANGE * VU_METER_HEIGHT;
        float text80Y = VU_METER_Y + 80.0f / VU_METER_RANGE * VU_METER_HEIGHT;
        float textWidth = getWidth() / 5;
        float textHeight = getHeight() / 10;
        //        g.drawText("  0", textX, VU_METER_Y - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT) // 指定 Futura 字体
                .withHeight(14.0f * getHeight() / 150.0f)
                .withStyle("Plain") });
        g.drawText("-20", textX, text20Y - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-40", textX, text40Y - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-60", textX, text60Y - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);
        g.drawText("-80", textX, text80Y - textHeight / 2.0f, textWidth, textHeight, juce::Justification::centred);

        // show input/output db
        g.setColour(juce::Colours::yellowgreen);
        float inputValue = ((vuMeterIn.getLeftChannelLevel() + vuMeterIn.getRightChannelLevel()) / 2.0f) * VU_METER_RANGE - VU_METER_RANGE;
        float outputValue = ((vuMeterOut.getLeftChannelLevel() + vuMeterOut.getRightChannelLevel()) / 2.0f) * VU_METER_RANGE - VU_METER_RANGE;

        if (updateCounter == 5)
        {
            displayInputValue = inputValue;
            displayOutputValue = outputValue;
            updateCounter = 0;
        }
        else
        {
            updateCounter++;
        }

        juce::Rectangle<int> localBounds = getLocalBounds();
        juce::Rectangle<int> leftArea = localBounds.removeFromLeft(getWidth() / 4);
        juce::Rectangle<int> rightArea = localBounds.removeFromRight(getWidth() / 3);
        g.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withHeight(20.0f * getHeight() / 150.0f)
                .withStyle("Bold") });
        g.drawText(juce::String(displayInputValue, 1), leftArea, juce::Justification::centred);
        g.drawText(juce::String(displayOutputValue, 1), rightArea, juce::Justification::centred);

        g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
        g.setFont(juce::Font {
            juce::FontOptions()
                .withName(KNOB_FONT)
                .withHeight(14.0f * getHeight() / 150.0f)
                .withStyle("Plain") });
        g.drawText("RMS Input", leftArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);
        g.drawText("RMS Output", rightArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);
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
