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
    verticalLine.addMouseListener(this, true);

    lineStatelId = ParameterIDAndName::getIDString(LINE_STATE_ID, index);
    sliderFreqId = ParameterIDAndName::getIDString(FREQ_ID, index);

    multiFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, sliderFreqId, verticalLine);

    addChildComponent(freqTextLabel);
    freqTextLabel.addMouseListener(this, true);
    // The parent component WON'T respond to mouse clicks,
    // while child components WILL respond to mouse clicks!
    setInterceptsMouseClicks(false, true);
}

FreqDividerGroup::~FreqDividerGroup()
{
    freqTextLabel.setLookAndFeel(nullptr);
    freqTextLabel.removeMouseListener(this);
    verticalLine.removeMouseListener(this);
    verticalLine.removeListener(this);
}

void FreqDividerGroup::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void FreqDividerGroup::resized()
{
    const float uiScale = juce::jlimit(0.75f, 2.0f, getWidth() / 100.0f);
    margin = 7.5f * uiScale;
    size = 15.0f * uiScale;
    verticalLine.setBounds(0, 0, getWidth() / 10.0f, getHeight());
    width = verticalLine.getWidth() / 2.0f;
    freqTextLabel.setScale(uiScale);
    freqTextLabel.setBounds(juce::roundToInt(width + margin),
                            juce::roundToInt(getHeight() / 5.0f + margin),
                            juce::roundToInt(size * 5.0f),
                            juce::roundToInt(size * 1.9f));
}

void FreqDividerGroup::setDeleteState(bool deleteState)
{
    verticalLine.setDeleteState(deleteState);
}

void FreqDividerGroup::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[])
{
    const int index = verticalLine.getIndex();
    if (! getToggleState() || ! juce::isPositiveAndBelow(index, lineNum)
        || lineNum > 3 || ! std::isfinite(newXPercent))
        return;
    float leftLimit;
    float rightLimit;

    leftLimit = (index + 1) * margin;
    rightLimit = 1 - (lineNum - index) * margin;

    if (newXPercent < leftLimit)
    {
        newXPercent = leftLimit;
    }
    else if (newXPercent > rightLimit)
    {
        newXPercent = rightLimit;
    }

    verticalLine.setXPercent(newXPercent);
    // Keep the APVTS frequency authoritative in the same message-thread turn.
    // Publishing a larger NUM_BANDS value before SliderAttachment had consumed
    // an async notification allowed the audio thread to process the new band
    // with an old hidden crossover frequency.
    verticalLine.setValueAsPartOfGesture(transformFromLog(newXPercent),
                                         juce::sendNotificationSync);

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
    juce::ignoreUnused(modifiers);
    // called by changing toggle state
    if (getToggleState())
        setVisible(true);
    else
    {
        freqTextLabel.setFreq(-1);
        freqTextLabel.setFade(true, false);
        verticalLine.setXPercent(0.0f);
        setVisible(false);
    }
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

void FreqDividerGroup::setFreq(float f, juce::NotificationType notification)
{
    if (f <= 0.0f)
    {
        freqTextLabel.setFreq(-1);
        verticalLine.setXPercent(0.0f);
        return;
    }

    verticalLine.setValue(f, notification);
    const auto clampedFrequency = static_cast<float>(verticalLine.getValue());
    verticalLine.setXPercent(static_cast<float>(transformToLog(clampedFrequency)));
    freqTextLabel.setFreq(juce::roundToInt(clampedFrequency));
}

int FreqDividerGroup::getFreq()
{
    return juce::roundToInt(verticalLine.getValue());
}
void FreqDividerGroup::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::mouseMove(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::mouseDrag(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateLabelFade();
}

void FreqDividerGroup::updateLabelFade()
{
    const bool shouldShow = getToggleState()
                         && (verticalLine.isMouseOverOrDragging()
                             || freqTextLabel.isMouseOverCustom());
    freqTextLabel.setFade(true, shouldShow);
}
