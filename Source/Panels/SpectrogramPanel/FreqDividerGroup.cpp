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
    const int lineHitWidth = juce::jmax(4, juce::roundToInt(getWidth() / 10.0f));
    verticalLine.setBounds(0, 0, lineHitWidth, getHeight());
    width = verticalLine.getWidth() * 0.5f;
    freqTextLabel.setScale(uiScale);

    const int labelX = juce::roundToInt(width + 5.0f * uiScale);
    const int rightInset = juce::jmax(1, juce::roundToInt(2.0f * uiScale));
    const int availableWidth = juce::jmax(0, getWidth() - labelX - rightInset);
    const int labelWidth = juce::jmin(juce::roundToInt(72.0f * uiScale),
                                     availableWidth);
    const int labelHeight = juce::jmin(getHeight(),
        juce::roundToInt(juce::jlimit(20.0f, 38.0f, 22.0f * uiScale)));
    const int labelY = juce::jlimit(0,
                                    juce::jmax(0, getHeight() - labelHeight),
                                    juce::roundToInt(getHeight() / 5.0f + margin));
    freqTextLabel.setBounds(labelX, labelY, labelWidth, labelHeight);
}

void FreqDividerGroup::setDeleteState(bool deleteState)
{
    verticalLine.setDeleteState(deleteState);
}

void FreqDividerGroup::setFrequencyEditCallback(
    FreqTextLabel::FrequencyEditCallback callback)
{
    freqTextLabel.setFrequencyEditCallback(std::move(callback));
}

void FreqDividerGroup::setHiddenCallback(HiddenCallback callback)
{
    hiddenCallback = std::move(callback);
}

bool FreqDividerGroup::advanceAnimation(float deltaSeconds)
{
    return freqTextLabel.advanceAnimation(deltaSeconds);
}

void FreqDividerGroup::dismissImmediately()
{
    juce::Component::SafePointer<FreqDividerGroup> safeThis(this);
    freqTextLabel.dismissImmediately();

    if (safeThis == nullptr)
        return;

    // The gesture-end callback may synchronously delete the owning editor, so
    // this is deliberately the final component operation.
    verticalLine.dismissTransientInteraction();
}

void FreqDividerGroup::visibilityChanged()
{
    juce::ToggleButton::visibilityChanged();

    // A deferred LINE_STATE cleanup belongs to one hidden lifetime of this
    // fixed divider slot. Re-showing the same component starts a new session.
    ++visibilityGeneration;
}

std::uint64_t FreqDividerGroup::getVisibilityGeneration() const noexcept
{
    return visibilityGeneration;
}

void FreqDividerGroup::dismissIfHidden(std::uint64_t expectedGeneration)
{
    if (visibilityGeneration != expectedGeneration || isVisible())
        return;

    dismissImmediately();
}

void FreqDividerGroup::moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[])
{
    juce::Component::SafePointer<FreqDividerGroup> safeThis(this);
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

    // Reserve the final local position before moving constrained neighbours.
    // The neighbours publish first, so every synchronous host notification
    // observes an already strictly ordered active crossover tuple.
    verticalLine.setXPercent(newXPercent);

    if (verticalLine.getLeft() >= 0 && freqDividerGroup[verticalLine.getLeft()]->getToggleState() && newXPercent - freqDividerGroup[verticalLine.getLeft()]->verticalLine.getXPercent() - margin < -0.00001f) // float is not accurate!!!!
    {
        freqDividerGroup[verticalLine.getLeft()]->moveToX(lineNum, newXPercent - margin, margin, freqDividerGroup);

        if (safeThis == nullptr)
            return;
    }
    if (verticalLine.getRight() > 0 && verticalLine.getRight() < lineNum && freqDividerGroup[verticalLine.getRight()]->getToggleState() && freqDividerGroup[verticalLine.getRight()]->verticalLine.getXPercent() - newXPercent - margin < -0.00001f)
    {
        freqDividerGroup[verticalLine.getRight()]->moveToX(lineNum, newXPercent + margin, margin, freqDividerGroup);

        if (safeThis == nullptr)
            return;
    }

    // Keep the APVTS frequency authoritative in the same message-thread turn,
    // after any pushed neighbours have published their ordered positions.
    verticalLine.setValueAsPartOfGesture(transformFromLog(newXPercent),
                                         juce::sendNotificationSync);
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

        // APVTS ButtonAttachment invokes clicked() inside its own
        // ScopedValueSetter. The parent clears drag ownership synchronously,
        // then defers any gesture-end callback until that JUCE stack unwinds.
        auto callback = hiddenCallback;
        if (callback)
            callback();
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

    juce::Component::SafePointer<FreqDividerGroup> safeThis(this);
    verticalLine.setValue(f, notification);

    if (safeThis == nullptr)
        return;

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
