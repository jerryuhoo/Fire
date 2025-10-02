/*
  ==============================================================================

    FreqDividerGroup.cpp
    Created: 4 Sep 2021 1:14:37pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "FreqDividerGroup.h"

//==============================================================================
/** FreqDividerGroup is a component that contains FreqTextLabel, VerticalLine, and CloseButton
 */
//==============================================================================
FreqDividerGroup::FreqDividerGroup(FireAudioProcessor& p, int index) : processor(p), freqTextLabel(verticalLine)
{
    margin = getHeight() / 20.0f;

    addAndMakeVisible(verticalLine);

    verticalLine.addListener(this);

    lineStatelId = ParameterIDAndName::getIDString(LINE_STATE_ID, index);
    sliderFreqId = ParameterIDAndName::getIDString(FREQ_ID, index);

    multiFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, sliderFreqId, verticalLine);

    addAndMakeVisible(freqTextLabel);
    // The parent component WON'T respond to mouse clicks,
    // while child components WILL respond to mouse clicks!
    setInterceptsMouseClicks(false, true);
}

FreqDividerGroup::~FreqDividerGroup()
{
    freqTextLabel.setLookAndFeel(nullptr);
}

void FreqDividerGroup::paint(juce::Graphics& g)
{
    if (getToggleState())
    {
        if (verticalLine.isMouseOverOrDragging() || freqTextLabel.isMouseOverCustom())
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
        freqTextLabel.setFreq(-1);
        verticalLine.setXPercent(0);
    }
    //    g.setColour(juce::Colours::green.withAlpha(0.2f));
    //    g.fillAll();
}

void FreqDividerGroup::resized()
{
    margin = getHeight() / 20.0f;
    size = getWidth() / 100.0f * 15;
    width = verticalLine.getWidth() / 2.0f;
    verticalLine.setBounds(0, 0, getWidth() / 10.0f, getHeight());
    freqTextLabel.setBounds(width + margin * 2, getHeight() / 5 + margin, size * 5, size * 2);
}

void FreqDividerGroup::setDeleteState(bool deleteState)
{
    verticalLine.setDeleteState(deleteState);
}

void FreqDividerGroup::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[])
{
    if (! getToggleState())
        return;
    float leftLimit;
    float rightLimit;

    leftLimit = (verticalLine.getIndex() + 1) * margin;
    rightLimit = 1 - (lineNum - verticalLine.getIndex()) * margin;

    if (newXPercent < leftLimit)
    {
        newXPercent = leftLimit;
    }
    else if (newXPercent > rightLimit)
    {
        newXPercent = rightLimit;
    }

    verticalLine.setXPercent(newXPercent);
    verticalLine.setValue(transformFromLog(newXPercent)); // * (44100 / 2.0)

    if (verticalLine.getLeft() >= 0 && freqDividerGroup[verticalLine.getLeft()]->getToggleState() && newXPercent - freqDividerGroup[verticalLine.getLeft()]->verticalLine.getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        freqDividerGroup[verticalLine.getLeft()]->moveToX(lineNum, newXPercent - margin, margin, freqDividerGroup);
    }
    if (verticalLine.getRight() > 0 && verticalLine.getRight() < lineNum && freqDividerGroup[verticalLine.getRight()]->getToggleState() && freqDividerGroup[verticalLine.getRight()]->verticalLine.getXPercent() - newXPercent - margin < -0.00001f)
    {
        freqDividerGroup[verticalLine.getRight()]->moveToX(lineNum, newXPercent + margin, margin, freqDividerGroup);
    }
}

VerticalLine& FreqDividerGroup::getVerticalLine()
{
    return verticalLine;
}

void FreqDividerGroup::buttonClicked(juce::Button* button)
{
}

void FreqDividerGroup::clicked(const juce::ModifierKeys& modifiers)
{
    // called by changing toggle state
    if (getToggleState())
        setVisible(true);
    else
        setVisible(false);
}

void FreqDividerGroup::sliderValueChanged(juce::Slider* slider)
{
    // TODO: maybe i don't need this
    // ableton move sliders
    if (slider == &verticalLine && getToggleState())
    {
        //dragLinesByFreq(freqDividerGroup[0].getValue(), getSortedIndex(0));
        int freq = slider->getValue();
        freqTextLabel.setFreq(freq);
        float xPercent = static_cast<float>(transformToLog(freq));
        verticalLine.setXPercent(xPercent); // set freq -> set X percent
    }
}

void FreqDividerGroup::mouseDoubleClick(const juce::MouseEvent& e)
{
    // do nothing, override the silder function, which will reset value.
}

void FreqDividerGroup::setFreq(float f)
{
    verticalLine.setValue(f);
    verticalLine.setXPercent(static_cast<float>(transformToLog(f)));
    freqTextLabel.setFreq(f);
}

int FreqDividerGroup::getFreq()
{
    return verticalLine.getValue();
}
void FreqDividerGroup::mouseUp(const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseEnter(const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseExit(const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseDown(const juce::MouseEvent& e) {}
void FreqDividerGroup::mouseDrag(const juce::MouseEvent& e) {}
