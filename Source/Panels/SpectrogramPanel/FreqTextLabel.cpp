/*
  ==============================================================================

    FreqTextLabel.cpp
    Created: 2 Dec 2020 7:53:08pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FreqTextLabel.h"

//==============================================================================
FreqTextLabel::FreqTextLabel(VerticalLine &v) : verticalLine(v)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    mFrequency = -1;
    addAndMakeVisible(freqLabel);
    freqLabel.setEditable(true);
    setLookAndFeel(&flatButtonLnf);
    startTimerHz(60);
}

FreqTextLabel::~FreqTextLabel()
{
    setLookAndFeel(nullptr);
}

void FreqTextLabel::paint (juce::Graphics& g)
{
    if (mUpdate && mFadeIn)
    {
        if (currentStep < maxStep)
        {
            currentStep += 1;
        }
        else
        {
            mUpdate = false;
        }
    }
    else if (mUpdate && !mFadeIn)
    {
        if (currentStep > 0)
        {
            currentStep -= 1;
        }
        else
        {
            mUpdate = false;
        }
    }
    
    float alpha = juce::jmin(1.0f, currentStep / static_cast<float>(maxStep));
    setAlpha(alpha);
    float cornerSize = 10.0f * mScale;
    juce::Rectangle<float> rect = getLocalBounds().toFloat();
    g.setColour (COLOUR1.withAlpha(0.5f));
    g.fillRoundedRectangle(rect, cornerSize);
    g.setColour (COLOUR1);
    //    g.drawText (static_cast<juce::String>(mFrequency) + " Hz", getLocalBounds(),
    //                juce::Justification::centred, true);   // draw some placeholder text
    
    freqLabel.setBounds(0, 0, getWidth(), getHeight());
    freqLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    freqLabel.setJustificationType (juce::Justification::centred);
    freqLabel.setFont (juce::Font (14.0f * mScale, juce::Font::plain));
    juce::String freqText;
    freqText = static_cast<juce::String>(mFrequency) + " Hz";
    if (!freqLabel.isBeingEdited())
    {
        freqLabel.setText(freqText, juce::dontSendNotification);
    }
    
    freqLabel.onTextChange = [this]
    {
        mFrequency = freqLabel.getText().getIntValue();
        verticalLine.setValue(mFrequency);
        // trigger silderValueChanged in FreqDividerGroup?
    };
}

void FreqTextLabel::resized()
{
    repaint();
}

void FreqTextLabel::setFreq(int freq)
{
    mFrequency = freq;
}

int FreqTextLabel::getFreq()
{
    return mFrequency;
}

void FreqTextLabel::setScale(float scale)
{
    mScale = scale;
}

bool FreqTextLabel::isMouseOverCustom()
{
    return isMouseOver() || freqLabel.isMouseOverOrDragging() || freqLabel.isBeingEdited();
}

void FreqTextLabel::timerCallback()
{
    if(mUpdate)
    {
        repaint();
    }
}

void FreqTextLabel::setFade(bool update, bool isFadeIn)
{
    mUpdate = update;
    mFadeIn = isFadeIn;
    if (isFadeIn && getAlpha() == 0)
        setAlpha(0.01f);
}
