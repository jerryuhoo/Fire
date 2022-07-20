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
FreqDividerGroup::FreqDividerGroup (FireAudioProcessor& p, int index) : processor (p), closeButton (verticalLine), freqTextLabel (verticalLine)
{
    margin = getHeight() / 20.0f;

    addAndMakeVisible (verticalLine);
    addAndMakeVisible (closeButton);
    closeButton.addListener (this);

    verticalLine.addListener (this);
//    verticalLine.addMouseListener (this, true);

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
    closeButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, lineStatelId, closeButton);
    multiFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.treeState, sliderFreqId, verticalLine);

    addAndMakeVisible (freqTextLabel);
    // The parent component WON'T respond to mouse clicks,
    // while child components WILL respond to mouse clicks!
    setInterceptsMouseClicks (false, true);
    freqTextLabel.setLookAndFeel (&otherLookAndFeel);
}

FreqDividerGroup::~FreqDividerGroup()
{
    closeButton.removeListener (this);
    freqTextLabel.setLookAndFeel (nullptr);
}

void FreqDividerGroup::paint (juce::Graphics& g)
{
    if (closeButton.getToggleState())
    {
        if (verticalLine.getMoveState() || verticalLine.isMouseOver() || freqTextLabel.isMouseOverCustom())
        {
            freqTextLabel.setFade (true, true);
            freqTextLabel.setVisible (true);
        }
        else
        {
            freqTextLabel.setFade (true, false);
        }
    }
    else
    {
        freqTextLabel.setFreq (21);
        verticalLine.setXPercent (0);
    }
    g.setColour(juce::Colours::green.withAlpha(0.2f));
    g.fillAll();
}

void FreqDividerGroup::resized()
{
    margin = getHeight() / 20.0f;
    size = getWidth() / 100.0f * 15;
    width = verticalLine.getWidth() / 2.0f;
    verticalLine.setBounds (0, 0, getWidth() / 10.0f, getHeight());
    closeButton.setBounds (width + margin, margin, size, size);
    freqTextLabel.setBounds (width + margin * 2, getHeight() / 5 + margin, size * 5, size * 2);
    
//    DBG(getHeight());
//    DBG(getWidth());
//
//
//
//    dragger.
//    boundsConstrainer.setMinimumOnscreenAmounts (getHeight(), getWidth(), getHeight(), getWidth());
}

void FreqDividerGroup::setDeleteState (bool deleteState)
{
    verticalLine.setDeleteState (deleteState);
}

void FreqDividerGroup::moveToX (int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[])
{
    float leftLimit;
    float rightLimit;

    leftLimit = (verticalLine.getIndex() + 1) * margin;
    rightLimit = 1 - (lineNum - verticalLine.getIndex()) * margin;

    if (newXPercent < leftLimit)
        newXPercent = leftLimit;
    if (newXPercent > rightLimit)
        newXPercent = rightLimit;

    if (verticalLine.getLeft() >= 0 && newXPercent - freqDividerGroup[verticalLine.getLeft()]->verticalLine.getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        freqDividerGroup[verticalLine.getLeft()]->moveToX (lineNum, newXPercent - margin, margin, freqDividerGroup);
    }
    if (verticalLine.getRight() < lineNum && freqDividerGroup[verticalLine.getRight()]->verticalLine.getXPercent() - newXPercent - margin < -0.00001f)
    {
        freqDividerGroup[verticalLine.getRight()]->moveToX (lineNum, newXPercent + margin, margin, freqDividerGroup);
    }
    verticalLine.setXPercent (newXPercent);
    verticalLine.setValue (SpectrumComponent::transformFromLog (newXPercent)); // * (44100 / 2.0)
}

VerticalLine& FreqDividerGroup::getVerticalLine()
{
    return verticalLine;
}

CloseButton& FreqDividerGroup::getCloseButton()
{
    return closeButton;
}

void FreqDividerGroup::setCloseButtonValue (bool value)
{
    closeButton.setToggleState (value, juce::NotificationType::sendNotification);
}

void FreqDividerGroup::buttonClicked (juce::Button* button)
{
    if (button == &closeButton)
    {
        if (closeButton.getToggleState())
        {
            setVisible (true);
            closeButton.setVisible (true);
        }
        else
        {
            verticalLine.setDeleteState (true);
            setVisible (false);
            closeButton.setVisible (false);
            freqTextLabel.setVisible (false);
        }
    }
}

void FreqDividerGroup::sliderValueChanged (juce::Slider* slider)
{
    // ableton move sliders
    if (slider == &verticalLine && getCloseButton().getToggleState())
    {
        //dragLinesByFreq(freqDividerGroup[0].getValue(), getSortedIndex(0));
        int freq = slider->getValue();
        freqTextLabel.setFreq (freq);
    }
}

void FreqDividerGroup::mouseDoubleClick (const juce::MouseEvent& e)
{
    // do nothing, override the silder function, which will reset value.
}

void FreqDividerGroup::setFreq (float f)
{
    verticalLine.setValue (f);
    verticalLine.setXPercent (static_cast<float> (SpectrumComponent::transformToLog (f)));
    freqTextLabel.setFreq (f);
}

int FreqDividerGroup::getFreq()
{
    return verticalLine.getValue();
}
void FreqDividerGroup::mouseUp (const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseEnter (const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseExit (const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseDown (const juce::MouseEvent& e)
{
    // call parent mousedown(Multiband)
//    getParentComponent()->mouseDown(e.getEventRelativeTo(getParentComponent()));
//    DBG("group down");
//    dragger.startDraggingComponent (this, e);
}

void FreqDividerGroup::mouseDrag (const juce::MouseEvent& e)
{
//    DBG("group drag");
//    float mouseX = getParentComponent()->getMouseXYRelative().getX();
//    float xPercent = mouseX / static_cast<float>(getParentWidth());
    
//    if (xPercent >= 0.1f && xPercent <= 0.9f)
//        dragger.dragComponent (this, e, &boundsConstrainer);
//    else if (xPercent < 0.1f)
//        xPercent = 0.1f;
//    else if (xPercent > 0.9f)
//        xPercent = 0.9f;
//    dragger.dragComponent (this, e, &boundsConstrainer);
//    verticalLine.setXPercent(xPercent);
    
}

void FreqDividerGroup::setScale (float scale)
{
    otherLookAndFeel.scale = scale;
    freqTextLabel.setScale (scale);
}

void FreqDividerGroup::setToggleState (bool state)
{
    closeButton.setToggleState (state, juce::dontSendNotification);
}

bool FreqDividerGroup::getToggleState()
{
    return closeButton.getToggleState();
}
