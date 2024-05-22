/*
  ==============================================================================

    VUPanel.cpp
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VUPanel.h"

//==============================================================================
VUPanel::VUPanel(FireAudioProcessor &p) : processor(p), focusBandNum(0), vuMeterIn(&p), vuMeterOut(&p)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    
    vuMeterIn.setParameters(true, "Band1");
    vuMeterOut.setParameters(false, "Band1");
    
    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);
    
    startTimerHz(60);
}

VUPanel::~VUPanel()
{
}

void VUPanel::paint (juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    
    // draw compressor threshold line
    g.setColour(KNOB_SUBFONT_COLOUR);
    
    juce::String threshID = "";
    
    bool isGlobal = false;
    if (focusBandNum == 0)
    {
        vuMeterIn.setParameters(true, "Band1");
        vuMeterOut.setParameters(false, "Band1");
        threshID = COMP_THRESH_ID1;
    }
    else if (focusBandNum == 1)
    {
        vuMeterIn.setParameters(true, "Band2");
        vuMeterOut.setParameters(false, "Band2");
        threshID = COMP_THRESH_ID2;
    }
    else if (focusBandNum == 2)
    {
        vuMeterIn.setParameters(true, "Band3");
        vuMeterOut.setParameters(false, "Band3");
        threshID = COMP_THRESH_ID3;
    }
    else if (focusBandNum == 3)
    {
        vuMeterIn.setParameters(true, "Band4");
        vuMeterOut.setParameters(false, "Band4");
        threshID = COMP_THRESH_ID4;
    }
    else if (focusBandNum == -1)
    {
        vuMeterIn.setParameters(true, "Global");
        vuMeterOut.setParameters(false, "Global");
        isGlobal = true;
    }
    else jassertfalse;
    
    // draw threshold pointer
    if (!isGlobal)
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

        g.setColour(juce::Colours::yellowgreen);
        g.drawLine(pointerX + vuMeterIn.getWidth() / 3.0f, compressorLineY, pointerX + vuMeterIn.getWidth() / 3.0f * 2.0f, compressorLineY, 1);
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
        g.setFont(juce::Font(KNOB_FONT, 14.0f * getHeight() / 150.0f, juce::Font::plain));
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
        g.setFont(juce::Font(KNOB_FONT, 20.0f * getHeight() / 150.0f, juce::Font::bold));
        g.drawText(juce::String(displayInputValue, 1), leftArea, juce::Justification::centred);
        g.drawText(juce::String(displayOutputValue, 1), rightArea, juce::Justification::centred);
        
        g.setColour(juce::Colours::yellowgreen.withAlpha(0.5f));
        g.setFont(juce::Font(KNOB_FONT, 14.0f * getHeight() / 150.0f, juce::Font::plain));
        g.drawText("RMS Input", leftArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);
        g.drawText("RMS Output", rightArea.removeFromBottom(getHeight() / 3), juce::Justification::centredTop);
    }
    
    if (isMouseOn && !mZoomState)
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
