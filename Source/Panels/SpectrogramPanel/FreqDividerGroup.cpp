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
FreqDividerGroup::FreqDividerGroup(FireAudioProcessor &p, int index) : processor(p), closeButton(verticalLine), freqTextLabel(verticalLine)
{
    margin = getHeight() / 20.0f;

    addAndMakeVisible(verticalLine);
    addAndMakeVisible(closeButton);
    closeButton.addListener(this);
//    closeButton.onClick = [this] { updateCloseButtonState (); };
    
    verticalLine.addListener(this);
    
    if (index == 0)
    {
        lineStatelId = LINE_STATE_ID1;
        sliderFreqId = FREQ_ID1;
    }
    if (index == 1)
    {
        lineStatelId = LINE_STATE_ID2;
        sliderFreqId = FREQ_ID2;
    }
    if (index == 2)
    {
        lineStatelId = LINE_STATE_ID3;
        sliderFreqId = FREQ_ID3;
    }
    closeButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, lineStatelId, closeButton);
    multiFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, sliderFreqId, verticalLine);

    addAndMakeVisible(freqTextLabel);
    // The parent component WON'T respond to mouse clicks,
    // while child components WILL respond to mouse clicks!
    setInterceptsMouseClicks(false, true);
    freqTextLabel.setLookAndFeel(&otherLookAndFeel);
}

FreqDividerGroup::~FreqDividerGroup()
{
    closeButton.removeListener(this);
    freqTextLabel.setLookAndFeel(nullptr);
}

void FreqDividerGroup::paint (juce::Graphics& g)
{
    if (closeButton.getToggleState())
    {
        
        if (verticalLine.isMoving() || verticalLine.isMouseOver() || freqTextLabel.isMouseOverCustom())
        {
            freqTextLabel.setFade(true, true);
            freqTextLabel.setVisible(true);
        }
        else
        {
            freqTextLabel.setFade(true, false);
        }
    }
    else
    {
        freqTextLabel.setFreq(21);
        verticalLine.setXPercent(0);
    }
}

void FreqDividerGroup::resized()
{
    margin = getHeight() / 20.0f;
    size = getWidth() / 1000.0f * 15;
    width = verticalLine.getWidth() / 2.0f;
    verticalLine.setBounds(0, 0, getWidth() / 100.0f, getHeight());
    closeButton.setBounds(width + margin, margin, size, size);
    freqTextLabel.setBounds(width + margin * 2, getHeight() / 5 + margin, size * 5, size * 2);
}

bool FreqDividerGroup::getDeleteState()
{
    return verticalLine.getDeleteState();
}

void FreqDividerGroup::setDeleteState(bool deleteState)
{
    verticalLine.setDeleteState(deleteState);
}

void FreqDividerGroup::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[], int sortedIndex[])
{
    float leftLimit;
    float rightLimit;

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
    verticalLine.setValue(SpectrumComponent::transformFromLog(newXPercent)); // * (44100 / 2.0)
}

VerticalLine& FreqDividerGroup::getVerticalLine()
{
    return verticalLine;
}

CloseButton& FreqDividerGroup::getCloseButton()
{
    return closeButton;
}

void FreqDividerGroup::setCloseButtonValue(bool value)
{
    closeButton.setToggleState(value, juce::NotificationType::sendNotification);
    updateCloseButtonState();
}

void FreqDividerGroup::buttonClicked(juce::Button* button)
{
    if (button == &closeButton)
    {
        if (closeButton.getToggleState())
        {
            setVisible(true);
            closeButton.setVisible(true);
        }
        else
        {
            verticalLine.setDeleteState(true);
            setVisible(false);
            closeButton.setVisible(false);
            freqTextLabel.setVisible(false);
        }
    }
}

void FreqDividerGroup::sliderValueChanged(juce::Slider *slider)
{
    // ableton move sliders
    if (slider == &verticalLine && getCloseButton().getToggleState())
    {
        //dragLinesByFreq(freqDividerGroup[0].getValue(), getSortedIndex(0));
        int freq = slider->getValue();
        freqTextLabel.setFreq(freq);
    }
}

void FreqDividerGroup::updateCloseButtonState()
{
//    if (closeButton.getToggleState())
//    {
//        setVisible(true);
//        closeButton.setVisible(true);
//    }
//    else
//    {
//        verticalLine.setDeleteState(true);
//        setVisible(false);
//        closeButton.setVisible(false);
//        freqTextLabel.setVisible(false);
//    }
}

void FreqDividerGroup::mouseDoubleClick(const juce::MouseEvent &e)
{
    // do nothing, override the silder function, which will reset value.
}

void FreqDividerGroup::setFreq(float f)
{
    verticalLine.setValue(f);
    freqTextLabel.setFreq(f);
}

void FreqDividerGroup::mouseUp(const juce::MouseEvent &e) {}
void FreqDividerGroup::mouseEnter(const juce::MouseEvent &e) {}
void FreqDividerGroup::mouseExit(const juce::MouseEvent &e) {}
void FreqDividerGroup::mouseDown(const juce::MouseEvent &e) {}
void FreqDividerGroup::mouseDrag(const juce::MouseEvent &e) {}

void FreqDividerGroup::setScale(float scale)
{
    otherLookAndFeel.scale = scale;
    freqTextLabel.setScale(scale);
}
