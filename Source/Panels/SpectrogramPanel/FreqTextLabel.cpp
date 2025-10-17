/*
 ==============================================================================
 
 FreqTextLabel.cpp
 Created: 2 Dec 2020 7:53:08pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "FreqTextLabel.h"
#include "../../Utility/AudioHelpers.h"

//==============================================================================
FreqTextLabel::FreqTextLabel(VerticalLine& v) : verticalLine(v)
{
    mFrequency = -1;

    // Add and configure the child juce::Label component.
    addAndMakeVisible(freqLabel);
    freqLabel.setEditable(true);

    // --- One-time setup for the child Label ---
    // These properties are set once here instead of inefficiently in paint().
    freqLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    freqLabel.setJustificationType(juce::Justification::centred);

    // Set the text editing callback once in the constructor.
    freqLabel.onTextChange = [this]
    {
        mFrequency = freqLabel.getText().getIntValue();

        // Update the associated VerticalLine component when the text changes.
        verticalLine.setValue(mFrequency);

        // Assumes transformToLog is defined in AudioHelpers.h
        float xPercent = static_cast<float>(transformToLog(mFrequency));
        verticalLine.setXPercent(xPercent);
    };
}

FreqTextLabel::~FreqTextLabel()
{
    // It's good practice to stop the timer in the destructor to prevent leaks
    // if the component is deleted while an animation is running.
    stopTimer();

    // Unregister the look and feel if one was set.
    setLookAndFeel(nullptr);
}

void FreqTextLabel::paint(juce::Graphics& g)
{
    // The paint() method is for drawing only. All setup is done elsewhere.

    // 1. Calculate the current alpha based on the animation step.
    float alpha = juce::jmin(1.0f, currentStep / static_cast<float>(maxStep));
    setAlpha(alpha);

    // 2. Draw the rounded background rectangle.
    float cornerSize = 10.0f * mScale;
    juce::Rectangle<float> rect = getLocalBounds().toFloat();
    g.setColour(COLOUR1.withAlpha(0.5f));
    g.fillRoundedRectangle(rect, cornerSize);
    g.setColour(COLOUR1);

    // 3. Update the label's text, but only if the user is not currently editing it.
    if (! freqLabel.isBeingEdited())
    {
        juce::String freqText = static_cast<juce::String>(mFrequency) + " Hz";
        freqLabel.setText(freqText, juce::dontSendNotification);
    }
}

void FreqTextLabel::resized()
{
    // The resized() method is the correct place to set the bounds of child components.
    freqLabel.setBounds(getLocalBounds());

    // It's also a good place to update anything that depends on size, like font height.
    freqLabel.setFont(juce::Font {
        juce::FontOptions()
            .withHeight(14.0f * mScale)
            .withStyle("Plain") });
}

void FreqTextLabel::timerCallback()
{
    // The timerCallback is solely responsible for updating the animation state.

    bool animationIsFinished = false;

    if (mFadeIn)
    {
        if (currentStep < maxStep)
        {
            currentStep += 1;
        }
        else
        {
            animationIsFinished = true;
        }
    }
    else // Fading out
    {
        if (currentStep > 0)
        {
            currentStep -= 1;
        }
        else
        {
            animationIsFinished = true;
        }
    }

    if (animationIsFinished)
    {
        mUpdate = false;
        stopTimer(); // CRITICAL: Stop the timer when animation is complete.
    }

    // Trigger a repaint to draw the new state.
    repaint();
}

void FreqTextLabel::setFade(bool update, bool isFadeIn)
{
    mUpdate = update;
    mFadeIn = isFadeIn;

    // This ensures a component with alpha=0 is still visible to the mouse
    // so that a mouse-over can trigger the fade-in.
    if (isFadeIn && getAlpha() == 0.0f)
        setAlpha(0.01f);

    // Start the timer only if an animation is requested and the timer isn't already running.
    if (mUpdate && ! isTimerRunning())
    {
        startTimerHz(60);
    }
}

void FreqTextLabel::setFreq(int freq)
{
    mFrequency = freq;
    repaint(); // Repaint to show the new frequency value.
}

int FreqTextLabel::getFreq()
{
    return mFrequency;
}

void FreqTextLabel::setScale(float scale)
{
    mScale = scale;
    resized(); // Call resized() to update font size based on the new scale.
}

bool FreqTextLabel::isMouseOverCustom()
{
    // Checks if the mouse is over this component OR its child label.
    return isMouseOver() || freqLabel.isMouseOverOrDragging() || freqLabel.isBeingEdited();
}