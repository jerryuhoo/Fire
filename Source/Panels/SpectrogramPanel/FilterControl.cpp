/*
  ==============================================================================

    FilterControl.cpp
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "FilterControl.h"
#include "../../GUI/InterfaceDefines.h"

//==============================================================================
FilterControl::FilterControl(FireAudioProcessor& p, GlobalPanel& panel) : processor(p), globalPanel(panel)
{
    const auto& params = processor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }

    updateChain();
    addAndMakeVisible(draggableLowButton);
    addAndMakeVisible(draggablePeakButton);
    addAndMakeVisible(draggableHighButton);
    startTimerHz(30);
}

FilterControl::~FilterControl()
{
    stopTimer();
    const auto& params = processor.getParameters();
    for (auto param : params)
    {
        param->removeListener(this);
    }
}

void FilterControl::paint(juce::Graphics& g)
{
    if (isVisible())
        processor.setHistoryArray(5); // 5 means global

    bool isFilterEnabled = *processor.treeState.getRawParameterValue(FILTER_BYPASS_ID);
    if (isFilterEnabled)
    {
        draggableLowButton.setState(true);
        draggablePeakButton.setState(true);
        draggableHighButton.setState(true);

        g.setColour(juce::Colours::hotpink.withBrightness(0.8f));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colours::hotpink.withBrightness(0.8f).withAlpha(0.2f));
        g.fillPath(responseCurve);

        g.setColour(juce::Colours::red.withAlpha(0.5f));
        g.strokePath(lfoResponseCurve, juce::PathStrokeType(2.0f));
    }
    else
    {
        draggableLowButton.setState(false);
        draggablePeakButton.setState(false);
        draggableHighButton.setState(false);

        g.setColour(juce::Colours::dimgrey.withAlpha(0.8f));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colours::dimgrey.withAlpha(0.8f).withAlpha(0.2f));
        g.fillPath(responseCurve);
    }

    float size = getWidth() / 1000.0f * 15;

    // ======================== DRAG LOGIC & VALUE DISPLAY ========================
    bool isDragging = false;
    double currentFreq = 0;
    double currentGain = 0;

    auto mousePos = getMouseXYRelative();

    // Clamp mouse position to stay within the component bounds for calculations
    mousePos.x = juce::jlimit(0, getWidth(), mousePos.x);
    mousePos.y = juce::jlimit(0, getHeight(), mousePos.y);

    float buttonX = mousePos.x - size / 2.0f;
    float buttonY = mousePos.y - size / 2.0f;

    // A helper lambda to set parameter values directly in the processor state
    auto setParameterValue = [&](const juce::String& paramID, float value)
    {
        if (auto* param = processor.treeState.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    if (draggableLowButton.isMouseButtonDown() && isFilterEnabled)
    {
        isDragging = true;
        // Instead of calling GlobalPanel, directly set the parameter that controls the filter type
        processor.treeState.getParameter(LOW_ID)->setValueNotifyingHost(1.0f);

        draggableLowButton.setBounds(buttonX, buttonY, size, size);

        currentFreq = juce::mapToLog10(static_cast<double>(mousePos.x / static_cast<double>(getWidth())), 20.0, 20000.0);
        currentGain = 15.0f * (getHeight() / 2.0f - mousePos.y) / (getHeight() / 48.0f * 15.0f);

        // Set the parameter values directly, not the slider
        setParameterValue(LOWCUT_FREQ_ID, currentFreq);
        setParameterValue(LOWCUT_GAIN_ID, currentGain);
    }
    if (draggablePeakButton.isMouseButtonDown() && isFilterEnabled)
    {
        isDragging = true;
        // Set the parameter for the Peak filter type
        processor.treeState.getParameter(BAND_ID)->setValueNotifyingHost(1.0f);

        draggablePeakButton.setBounds(buttonX, buttonY, size, size);

        currentFreq = juce::mapToLog10(static_cast<double>(mousePos.x / static_cast<double>(getWidth())), 20.0, 20000.0);
        currentGain = 15.0f * (getHeight() / 2.0f - mousePos.y) / (getHeight() / 48.0f * 15.0f);

        // Set the parameter values directly
        setParameterValue(PEAK_FREQ_ID, currentFreq);
        setParameterValue(PEAK_GAIN_ID, currentGain);
    }
    if (draggableHighButton.isMouseButtonDown() && isFilterEnabled)
    {
        isDragging = true;
        // Set the parameter for the High-pass filter type
        processor.treeState.getParameter(HIGH_ID)->setValueNotifyingHost(1.0f);

        draggableHighButton.setBounds(buttonX, buttonY, size, size);

        currentFreq = juce::mapToLog10(static_cast<double>(mousePos.x / static_cast<double>(getWidth())), 20.0, 20000.0);
        currentGain = 15.0f * (getHeight() / 2.0f - mousePos.y) / (getHeight() / 48.0f * 15.0f);

        // Set the parameter values directly
        setParameterValue(HIGHCUT_FREQ_ID, currentFreq);
        setParameterValue(HIGHCUT_GAIN_ID, currentGain);
    }

    if (isDragging)
    {
        // Format the text to display
        juce::String freqString = (currentFreq < 1000.0) ? juce::String(currentFreq, 1) + " Hz" : juce::String(currentFreq / 1000.0, 2) + " kHz";

        juce::String gainString = juce::String(currentGain, 1) + " dB";

        // Use juce::newLine for cross-platform compatibility
        juce::String textToShow = freqString + juce::newLine + gainString;

        // Define text box properties
        int boxWidth = 100;
        int boxHeight = 40;
        int xOffset = 20;
        int yOffset = -20;

        // Calculate initial position
        juce::Rectangle<int> textBox(mousePos.x + xOffset, mousePos.y + yOffset - boxHeight, boxWidth, boxHeight);

        // Adjust position to keep the box within component bounds
        if (textBox.getRight() > getWidth())
            textBox.setX(mousePos.x - xOffset - boxWidth);
        if (textBox.getY() < 0)
            textBox.setY(mousePos.y + 10);

        // Draw the text box
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(textBox.toFloat(), 5.0f);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawFittedText(textToShow, textBox, juce::Justification::centred, 2);
    }
}

void FilterControl::timerCallback()
{
    ModulatedFilterValues modulatedValues;

    // Safely pull the latest data packet from the FIFO queue.
    if (processor.getLatestModulatedFilterValues(modulatedValues))
    {
        // If we got new data, update the LFO chain and its response curve.
        updateLfoChain(modulatedValues);
        updateLfoResponseCurve();

        // Trigger a repaint to show the changes on screen.
        repaint();
    }
}

void FilterControl::resized()
{
    updateResponseCurve();
    setDraggableButtonBounds();
}

void FilterControl::setDraggableButtonBounds()
{
    float size = getWidth() / 1000.0f * 15;
    float lowcutX = getWidth() * juce::mapFromLog10(static_cast<double>(*processor.treeState.getRawParameterValue(LOWCUT_FREQ_ID)), 20.0, 20000.0);
    float peakX = getWidth() * juce::mapFromLog10(static_cast<double>(*processor.treeState.getRawParameterValue(PEAK_FREQ_ID)), 20.0, 20000.0);
    float highcutX = getWidth() * juce::mapFromLog10(static_cast<double>(*processor.treeState.getRawParameterValue(HIGHCUT_FREQ_ID)), 20.0, 20000.0);

    float lowcutY = getHeight() / 2.0f * *processor.treeState.getRawParameterValue(LOWCUT_GAIN_ID) / 24.0f;
    float peakY = getHeight() / 2.0f * *processor.treeState.getRawParameterValue(PEAK_GAIN_ID) / 24.0f;
    float highcutY = getHeight() / 2.0f * *processor.treeState.getRawParameterValue(HIGHCUT_GAIN_ID) / 24.0f;

    draggableLowButton.setBounds(lowcutX - size / 2.0f, getHeight() / 2.0f - lowcutY - size / 2.0f, size, size);
    draggablePeakButton.setBounds(peakX - size / 2.0f, getHeight() / 2.0f - peakY - size / 2.0f, size, size);
    draggableHighButton.setBounds(highcutX - size / 2.0f, getHeight() / 2.0f - highcutY - size / 2.0f, size, size);
}

void FilterControl::updateChain()
{
    auto chainSettings = getChainSettings(processor.treeState);
    const auto sampleRate = processor.getSampleRate();

    if (sampleRate <= 0)
        return;

    const float minFreq = 20.0f;
    float maxFreq = sampleRate / 2.0f;

    if (maxFreq < minFreq)
    {
        maxFreq = minFreq;
    }

    chainSettings.lowCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.lowCutFreq);
    chainSettings.peakFreq = juce::jlimit(minFreq, maxFreq, chainSettings.peakFreq);
    chainSettings.highCutFreq = juce::jlimit(minFreq, maxFreq, chainSettings.highCutFreq);

    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    monoChain.setBypassed<ChainPositions::LowCutQ>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::HighCutQ>(chainSettings.highCutBypassed);

    auto peakCoefficients = makePeakFilter(chainSettings, processor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

    auto lowCutCoefficients = makeLowCutFilter(chainSettings, processor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, processor.getSampleRate());

    auto lowCutQCoefficients = makeLowcutQFilter(chainSettings, processor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::LowCutQ>().coefficients, lowCutQCoefficients);
    auto highCutQCoefficients = makeHighcutQFilter(chainSettings, processor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::HighCutQ>().coefficients, highCutQCoefficients);

    updateCutFilter(monoChain.get<ChainPositions::LowCut>(),
                    lowCutCoefficients,
                    chainSettings.lowCutSlope);

    updateCutFilter(monoChain.get<ChainPositions::HighCut>(),
                    highCutCoefficients,
                    chainSettings.highCutSlope);
}

void FilterControl::updateResponseCurve()
{
    int w = getWidth();

    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();
    auto& lowcutQ = monoChain.get<ChainPositions::LowCutQ>();
    auto& highcutQ = monoChain.get<ChainPositions::HighCutQ>();

    auto sampleRate = processor.getSampleRate();

    if (sampleRate <= 0)
        return;

    const auto nyquist = sampleRate / 2.0;
    const auto maxDisplayFreq = std::min(20000.0, nyquist);

    std::vector<float> mags;
    mags.resize(w);

    for (int i = 0; i < w; ++i)
    {
        double mag = 1.0f;

        auto freq = juce::mapToLog10(double(i) / double(w), 20.0, maxDisplayFreq);

        if (! monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (! monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (! lowcut.isBypassed<0>())
                mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<1>())
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<2>())
                mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<3>())
                mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            mag *= lowcutQ.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (! monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (! highcut.isBypassed<0>())
                mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<1>())
                mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<2>())
                mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<3>())
                mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            mag *= highcutQ.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        mags[i] = juce::Decibels::gainToDecibels(mag);
    }

    responseCurve.clear();

    const double outputMin = getHeight();
    const double outputMax = 0;
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    juce::Point<float> startPoint(-3, getHeight() + 2);
    juce::Point<float> endPoint(getWidth() + 3, getHeight() + 2);

    responseCurve.startNewSubPath(startPoint);

    for (size_t i = 0; i < mags.size(); ++i)
    {
        responseCurve.lineTo(0 + i, map(mags[i]));
    }

    responseCurve.lineTo(endPoint);
    responseCurve.closeSubPath();
}

void FilterControl::parameterValueChanged(int parameterIndex, float newValue)
{
    // When a parameter changes (on ANY thread), we simply trigger an async update.
    // This is a very lightweight, non-blocking, and thread-safe call.
    // It posts a message to the message queue, ensuring handleAsyncUpdate()
    // will be called safely on the message thread.
    triggerAsyncUpdate();
}

void FilterControl::handleAsyncUpdate()
{
    // This function is guaranteed to be called on the message thread after
    // triggerAsyncUpdate() has been called.
    // All UI updates are now safely and efficiently handled here.
    updateChain();
    updateResponseCurve();
    setDraggableButtonBounds();
    repaint();
}

void FilterControl::updateLfoChain(const ModulatedFilterValues& modulatedValues)
{
    // Create a temporary ChainSettings struct and populate it with the
    // modulated values received from the audio thread.
    ChainSettings settings;
    settings.lowCutFreq = modulatedValues.lowCutFreq;
    settings.lowCutGainInDecibels = modulatedValues.lowCutGain;
    settings.lowCutQuality = modulatedValues.lowCutQ;
    settings.highCutFreq = modulatedValues.highCutFreq;
    settings.highCutGainInDecibels = modulatedValues.highCutGain;
    settings.highCutQuality = modulatedValues.highCutQ;
    settings.peakFreq = modulatedValues.peakFreq;
    settings.peakGainInDecibels = modulatedValues.peakGain;
    settings.peakQuality = modulatedValues.peakQ;

    // Get non-modulated settings directly from the processor's state tree.
    auto& apvts = processor.treeState;
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue(LOWCUT_SLOPE_ID)->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue(HIGHCUT_SLOPE_ID)->load());
    settings.lowCutBypassed = apvts.getRawParameterValue(LOWCUT_BYPASSED_ID)->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue(PEAK_BYPASSED_ID)->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue(HIGHCUT_BYPASSED_ID)->load() > 0.5f;

    auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0)
        return;

    // The rest of this logic is identical to updateChain, but for lfoMonoChain.
    auto peakCoefficients = makePeakFilter(settings, sampleRate);
    updateCoefficients(lfoMonoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

    auto lowCutCoefficients = makeLowCutFilter(settings, sampleRate);
    auto highCutCoefficients = makeHighCutFilter(settings, sampleRate);

    auto lowCutQCoefficients = makeLowcutQFilter(settings, sampleRate);
    updateCoefficients(lfoMonoChain.get<ChainPositions::LowCutQ>().coefficients, lowCutQCoefficients);
    auto highCutQCoefficients = makeHighcutQFilter(settings, sampleRate);
    updateCoefficients(lfoMonoChain.get<ChainPositions::HighCutQ>().coefficients, highCutQCoefficients);

    updateCutFilter(lfoMonoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, settings.lowCutSlope);
    updateCutFilter(lfoMonoChain.get<ChainPositions::HighCut>(), highCutCoefficients, settings.highCutSlope);
}

void FilterControl::updateLfoResponseCurve()
{
    auto& lowcut = lfoMonoChain.get<ChainPositions::LowCut>();
    auto& peak = lfoMonoChain.get<ChainPositions::Peak>();
    auto& highcut = lfoMonoChain.get<ChainPositions::HighCut>();
    auto& lowcutQ = lfoMonoChain.get<ChainPositions::LowCutQ>();
    auto& highcutQ = lfoMonoChain.get<ChainPositions::HighCutQ>();

    auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0)
        return;

    std::vector<double> mags;
    mags.resize(getWidth());

    for (int i = 0; i < getWidth(); ++i)
    {
        double mag = 1.0;
        auto freq = juce::mapToLog10(double(i) / double(getWidth()), 20.0, 20000.0);

        if (! lfoMonoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (! lfoMonoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (! lowcut.isBypassed<0>())
                mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<1>())
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<2>())
                mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! lowcut.isBypassed<3>())
                mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            mag *= lowcutQ.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (! lfoMonoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (! highcut.isBypassed<0>())
                mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<1>())
                mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<2>())
                mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (! highcut.isBypassed<3>())
                mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            mag *= highcutQ.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        mags[i] = juce::Decibels::gainToDecibels(mag);
    }

    lfoResponseCurve.clear();
    const double outputMin = getHeight();
    const double outputMax = 0;
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    lfoResponseCurve.startNewSubPath(0, map(mags.front()));
    for (size_t i = 1; i < mags.size(); ++i)
    {
        lfoResponseCurve.lineTo(i, map(mags[i]));
    }
}