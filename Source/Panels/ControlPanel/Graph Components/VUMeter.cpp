/*
  ==============================================================================

    VUMeter.cpp
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "VUMeter.h"
#include "../../../GUI/InterfaceDefines.h"
#include "../../../Utility/AudioHelpers.h"

//==============================================================================
VUMeter::VUMeter(FireAudioProcessor* inProcessor)
:   mProcessor(inProcessor),
    mIsInput(true),
    mCh0Level(0),
    mCh1Level(0),
    mMaxCh0Level(-96.0f),
    mMaxCh1Level(-96.0f),
    mMaxValueDecayCounter(0)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(60);
}

VUMeter::~VUMeter()
{
}

void VUMeter::paint (juce::Graphics& g)
{
    
    g.setColour(COLOUR6);
    const int meterWidth = getWidth() / 3;
    
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        // left
        g.fillRect(0, 0, meterWidth, getHeight());
        // right
        g.fillRect(meterWidth * 2, 0, meterWidth, getHeight());
    }
    else
    {
        g.fillRect(meterWidth, 0, meterWidth, getHeight());
    }
    
    int ch0fill = getHeight() - (getHeight() * mCh0Level);
    int ch1fill = getHeight() - (getHeight() * mCh1Level);
    
    if (ch0fill < 0)
    {
        ch0fill = 0;
    }
    if (ch1fill < 0)
    {
        ch1fill = 0;
    }
    
    // draw VU meter values
    g.setColour(juce::Colours::yellowgreen.withBrightness(0.9));
    
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.fillRect(0, ch0fill, meterWidth, getHeight());
        g.fillRect(meterWidth * 2, ch1fill, meterWidth, getHeight());
    }
    else
    {
        g.fillRect(meterWidth, ch0fill, meterWidth, getHeight());
    }
    
    // draw max VU meter values
    g.setColour(juce::Colours::yellowgreen.withBrightness(0.5));
    int maxCh0fill = getHeight() - (getHeight() * mMaxCh0Level);
    int maxCh1fill = getHeight() - (getHeight() * mMaxCh1Level);
    
    if (mProcessor->getTotalNumInputChannels() == 2)
    {
        g.drawLine(0, maxCh0fill, meterWidth, maxCh0fill, 2.0f);
        g.drawLine(meterWidth * 2, maxCh1fill, meterWidth * 3, maxCh1fill, 2.0f);
    }
    else
    {
        g.drawLine(meterWidth, maxCh0fill, meterWidth * 2, maxCh0fill, 2.0f);
    }
    
}

void VUMeter::resized()
{
}

void VUMeter::setParameters(bool isInput, int bandIndex)
{
    mIsInput = isInput;
    mBandIndex = bandIndex;
}

void VUMeter::timerCallback()
{
    float updatedCh0Level = 0.0f;
    float updatedCh1Level = 0.0f;

    if(mIsInput) // input
    {
        updatedCh0Level = mProcessor->getMeterRMSLevel(true, 0, mBandIndex);
        updatedCh1Level = mProcessor->getMeterRMSLevel(true, 1, mBandIndex);
    }
    else // output
    {
        updatedCh0Level = mProcessor->getMeterRMSLevel(false, 0, mBandIndex);
        updatedCh1Level = mProcessor->getMeterRMSLevel(false, 1, mBandIndex);
    }
    
    // update max values
    mMaxCh0Level = juce::jmax(mMaxCh0Level, updatedCh0Level);
    mMaxCh1Level = juce::jmax(mMaxCh1Level, updatedCh1Level);
    
    if (updatedCh0Level > mCh0Level)
    {
        mCh0Level = updatedCh0Level;
    }
    else
    {
        mCh0Level = SMOOTH_COEFF * (mCh0Level - updatedCh0Level) + updatedCh0Level;
    }
    
    if (updatedCh1Level > mCh1Level)
    {
        mCh1Level = updatedCh1Level;
    }
    else
    {
        mCh1Level = SMOOTH_COEFF * (mCh1Level - updatedCh1Level) + updatedCh1Level;
    }
    
    mCh0Level = helper_denormalize(mCh0Level);
    mCh1Level = helper_denormalize(mCh1Level);
    
    // decay max values
    if (mMaxCh0Level > mCh0Level || mMaxCh1Level > mCh1Level)
    {
        if (mMaxValueDecayCounter < MAX_VALUE_HOLD_FRAMES)
        {
            ++mMaxValueDecayCounter;
        }
        else
        {
            mMaxCh0Level -= 0.01f;
            mMaxCh1Level -= 0.01f;
        }
    }
    else
    {
        mMaxValueDecayCounter = 0;
    }
    
    repaint();
    
}

float VUMeter::getLeftChannelLevel()
{
    return mCh0Level;
}

float VUMeter::getRightChannelLevel()
{
    return mCh1Level;
}
