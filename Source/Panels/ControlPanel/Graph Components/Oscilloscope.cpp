/*
  ==============================================================================

    Oscilloscope.cpp
    Created: 25 Oct 2020 7:26:35pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Oscilloscope.h"

//==============================================================================
Oscilloscope::Oscilloscope(FireAudioProcessor& p) : processor(p)
{
    startTimerHz(60);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    // draw outline
    g.setColour(COLOUR6);
    g.drawRect(getLocalBounds(), 1);

    juce::ColourGradient grad(juce::Colours::red.withBrightness(0.9f), getWidth() / 2, getHeight() / 2, juce::Colours::red.withAlpha(0.1f), 0, getHeight() / 2, true);
    g.setGradientFill(grad);

    bool monoChannel = false;
    historyL = processor.getHistoryArrayL();
    if (processor.getTotalNumInputChannels() == 1)
    {
        monoChannel = true;
    }
    else
    {
        historyR = processor.getHistoryArrayR();
    }

    const int sampleCount = monoChannel ? historyL.size()
                                        : juce::jmin(historyL.size(), historyR.size());
    if (sampleCount <= 0 || getWidth() <= 0)
        return;

    juce::Path pathL;
    juce::Path pathR;

    float amp;
    if (monoChannel)
        amp = getHeight() / 2;
    else
        amp = getHeight() / 4;

    // get max
    float maxValue = 0.0f;

    for (int i = 0; i < sampleCount; ++i)
    {
        maxValue = juce::jmax(maxValue, std::abs(historyL[i]));
        if (! monoChannel)
            maxValue = juce::jmax(maxValue, std::abs(historyR[i]));
    }

    //TODO: this may cause high CPU usage! maybe use i += 2?
    float valL = 0.0f;
    float valR = 0.0f;

    for (int i = 0; i < getWidth(); i++)
    {
        const int scaledIndex = juce::jlimit(0, sampleCount - 1,
                                             static_cast<int>(i * (float) sampleCount / (float) getWidth()));

        valL = historyL[scaledIndex];
        if (! monoChannel)
            valR = historyR[scaledIndex];

        // normalize
        if (maxValue > 0.005f)
        {
            valL = valL / maxValue * 0.6f;
            if (! monoChannel)
                valR = valR / maxValue * 0.6f;
        }

        valL = juce::jlimit<float>(-1, 1, valL);
        if (! monoChannel)
            valR = juce::jlimit<float>(-1, 1, valR);

        if (i == 0)
        {
            pathL.startNewSubPath(0, amp);
            if (! monoChannel)
                pathR.startNewSubPath(0, amp * 3);
        }
        else
        {
            pathL.lineTo(i, amp - valL * amp);
            if (! monoChannel)
                pathR.lineTo(i, amp * 3 - valR * amp);
        }
    }

    g.strokePath(pathL, juce::PathStrokeType(2.0));
    if (! monoChannel)
        g.strokePath(pathR, juce::PathStrokeType(2.0));
}

void Oscilloscope::timerCallback()
{
    repaint();
}
