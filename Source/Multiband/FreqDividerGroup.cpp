/*
  ==============================================================================

    FreqDividerGroup.cpp
    Created: 4 Sep 2021 1:14:37pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FreqDividerGroup.h"

//==============================================================================
/** FreqDividerGroup is a component that contains FreqTextLabel, VerticalLine, and CloseButton
 */
//==============================================================================
FreqDividerGroup::FreqDividerGroup() : closeButton(verticalLine), freqTextLabel(verticalLine)
{
    margin = getHeight() / 20.0f;
    addAndMakeVisible(verticalLine);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(freqTextLabel);
    // The parent component WON'T respond to mouse clicks,
    // while child components WILL respond to mouse clicks!
    setInterceptsMouseClicks(false, true);
    
}

FreqDividerGroup::~FreqDividerGroup()
{
}

void FreqDividerGroup::paint (juce::Graphics& g)
{
    verticalLine.setBounds(0, 0, getWidth() / 100.0f, getHeight());
    closeButton.setBounds(width + margin, margin, size, size);
    //freqTextLabel.setBounds(width + margin - width * 7, getHeight() / 5 + margin, width * 15, size);
    freqTextLabel.setBounds(width + margin, getHeight() / 5 + margin, size * 4, size);
    
    if (verticalLine.getState())
    {
        if (verticalLine.isMoving() || verticalLine.isMouseOver() || freqTextLabel.isMouseOverCustom())
        {
            freqTextLabel.setVisible(true);
        }
        else
        {
            freqTextLabel.setVisible(false);
        }
        closeButton.setVisible(true);
    }
    else
    {
        // reset frequency if it is deleted
        setFrequency(0);
    }
}

void FreqDividerGroup::resized()
{
    margin = getHeight() / 20.0f;
    size = getHeight() / 10.0f;
    width = verticalLine.getWidth() / 2.0f;
}

int FreqDividerGroup::getFrequency()
{
    return  freqTextLabel.getFreq();
}

void FreqDividerGroup::setFrequency(int frequency)
{
    freqTextLabel.setFreq(frequency);
}

bool FreqDividerGroup::getChangeState()
{
    return freqTextLabel.getChangeState();
}

void FreqDividerGroup::setChangeState(bool state)
{
    freqTextLabel.setChangeState(state);
}

void FreqDividerGroup::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[], int sortedIndex[])
{
    float leftLimit;
    float rightLimit;

//    int index = leftIndex + 1;
    leftLimit = (verticalLine.getIndex() + 1) * margin;
    rightLimit = 1 - (lineNum - verticalLine.getIndex()) * margin;

    if (newXPercent < leftLimit) newXPercent = leftLimit;
    if (newXPercent > rightLimit) newXPercent = rightLimit;

    int idx = sortedIndex[verticalLine.getLeft()];

    if (verticalLine.getLeft() >= 0 && newXPercent - freqDividerGroup[idx]->verticalLine.getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        freqDividerGroup[sortedIndex[verticalLine.getLeft()]]->moveToX(lineNum, newXPercent - margin, margin, freqDividerGroup, sortedIndex);
    }
    if (verticalLine.getRight() < lineNum && freqDividerGroup[sortedIndex[verticalLine.getRight()]]->verticalLine.getXPercent() - newXPercent - margin < -0.00001f)
    {
        freqDividerGroup[sortedIndex[verticalLine.getRight()]]->moveToX(lineNum, newXPercent + margin, margin, freqDividerGroup, sortedIndex);
    }
    verticalLine.setXPercent(newXPercent);
    setFrequency(static_cast<int>(SpectrumComponent::transformFromLog(newXPercent) * (44100 / 2.0)));
}
