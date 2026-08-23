/*
 ==============================================================================
 
 FreqTextLabel.cpp
 Created: 2 Dec 2020 7:53:08pm
 Author:  羽翼深蓝Wings
 
 ==============================================================================
 */

#include "FreqTextLabel.h"
#include "../../Utility/AudioHelpers.h"
#include <cerrno>
#include <cstdlib>
#include <optional>

namespace
{
std::optional<double> parseFrequencyText(juce::String text)
{
    text = text.trim().toLowerCase();
    double multiplier = 1.0;

    if (text.endsWithIgnoreCase("khz"))
    {
        multiplier = 1000.0;
        text = text.dropLastCharacters(3).trim();
    }
    else if (text.endsWithChar('k'))
    {
        multiplier = 1000.0;
        text = text.dropLastCharacters(1).trim();
    }
    else if (text.endsWithIgnoreCase("hz"))
    {
        text = text.dropLastCharacters(2).trim();
    }

    if (text.isEmpty())
        return std::nullopt;

    const char* parseBegin = text.toRawUTF8();
    char* parseEnd = nullptr;
    errno = 0;
    const double parsedValue = std::strtod(parseBegin, &parseEnd);
    if (parseEnd == parseBegin || *parseEnd != '\0' || errno == ERANGE
        || ! std::isfinite(parsedValue))
        return std::nullopt;

    const double frequency = parsedValue * multiplier;
    if (! std::isfinite(frequency))
        return std::nullopt;

    return frequency;
}
} // namespace

//==============================================================================
FreqTextLabel::FreqTextLabel(VerticalLine& v) : verticalLine(v)
{
    mFrequency = -1;

    // Add and configure the child juce::Label component.
    addAndMakeVisible(freqLabel);
    freqLabel.setEditable(true);

    // --- One-time setup for the child Label ---
    // These properties are set once here instead of inefficiently in paint().
    freqLabel.setColour(juce::Label::textColourId, fire::ui::colours::whiteHot);
    freqLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    freqLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    freqLabel.setColour(juce::Label::backgroundWhenEditingColourId, fire::ui::colours::surface0);
    freqLabel.setColour(juce::Label::outlineWhenEditingColourId, fire::ui::colours::ember);
    freqLabel.setJustificationType(juce::Justification::centred);
    freqLabel.setAlpha(0.0f);

    freqLabel.onEditorShow = [this]
    {
        verticalLine.beginParameterGesture();
    };

    freqLabel.onEditorHide = [this]
    {
        // JUCE's Return/focus-loss commit has copied the TextEditor value into
        // the Label before this callback. Apply it now so the host sees the
        // value change before the matching endChangeGesture notification.
        applyEditedText();
        updateLabelText();
        verticalLine.endParameterGesture();
    };
}

FreqTextLabel::~FreqTextLabel()
{
    // Close the editor while our callbacks and the VerticalLine are still alive.
    // This also balances a gesture if the containing editor is destroyed midway
    // through frequency text entry.
    if (freqLabel.isBeingEdited())
        freqLabel.hideEditor(true);

    freqLabel.onTextChange = nullptr;
    freqLabel.onEditorShow = nullptr;
    freqLabel.onEditorHide = nullptr;
    frequencyEditCallback = nullptr;

    // It's good practice to stop the timer in the destructor to prevent leaks
    // if the component is deleted while an animation is running.
    stopTimer();

    // Unregister the look and feel if one was set.
    setLookAndFeel(nullptr);
}

void FreqTextLabel::paint(juce::Graphics& g)
{
    if (currentAlpha <= 0.001f)
        return;

    g.setOpacity(currentAlpha);
    auto rect = getLocalBounds().toFloat().reduced(0.5f);
    fire::ui::drawGlassPill(g, rect, fire::ui::colours::ember, true, false, false);

    auto energyRail = rect.reduced(fire::ui::Metrics::space8, 0.0f).removeFromBottom(1.0f);
    g.setColour(fire::ui::colours::flame.withAlpha(0.72f));
    g.fillRect(energyRail);

}

void FreqTextLabel::resized()
{
    // The resized() method is the correct place to set the bounds of child components.
    freqLabel.setBounds(getLocalBounds());

    // It's also a good place to update anything that depends on size, like font height.
    freqLabel.setFont(fire::ui::displayFont(juce::jlimit(9.0f, 14.0f, 11.0f * mScale)));
}

void FreqTextLabel::timerCallback()
{
    currentAlpha += (targetAlpha - currentAlpha) * 0.42f;
    if (std::abs(targetAlpha - currentAlpha) < 0.01f)
    {
        currentAlpha = targetAlpha;
        stopTimer();

        if (currentAlpha <= 0.0f)
            setVisible(false);
    }

    freqLabel.setAlpha(currentAlpha);
    repaint();
}

void FreqTextLabel::setFade(bool update, bool isFadeIn)
{
    if (! update)
        return;

    const float newTarget = isFadeIn ? 1.0f : 0.0f;
    if (juce::approximatelyEqual(targetAlpha, newTarget) && ! isTimerRunning())
        return;

    targetAlpha = newTarget;
    if (targetAlpha > 0.0f)
        setVisible(true);

    if (! isTimerRunning())
        startTimerHz(60);
}

void FreqTextLabel::setFreq(int freq)
{
    mFrequency = freq;
    updateLabelText();
}

void FreqTextLabel::applyEditedText()
{
    const auto requestedFrequency = parseFrequencyText(freqLabel.getText());
    if (! requestedFrequency.has_value() || ! frequencyEditCallback)
        return;

    const auto range = verticalLine.getNormalisableRange();
    if (*requestedFrequency < range.start || *requestedFrequency > range.end)
        return;

    const double constrainedFrequency = range.snapToLegalValue(*requestedFrequency);
    if (juce::approximatelyEqual(constrainedFrequency, verticalLine.getValue()))
        return;

    frequencyEditCallback(static_cast<float>(transformToLog(*requestedFrequency)));
    mFrequency = juce::roundToInt(verticalLine.getValue());
}

void FreqTextLabel::setFrequencyEditCallback(FrequencyEditCallback callback)
{
    frequencyEditCallback = std::move(callback);
}

void FreqTextLabel::updateLabelText()
{
    if (freqLabel.isBeingEdited())
        return;

    const auto freqText = mFrequency >= 1000
                            ? juce::String(mFrequency / 1000.0f, 2) + " kHz"
                            : juce::String(mFrequency) + " Hz";
    freqLabel.setText(freqText, juce::dontSendNotification);
}

int FreqTextLabel::getFreq()
{
    return mFrequency;
}

void FreqTextLabel::setScale(float scale)
{
    mScale = juce::jlimit(0.75f, 2.0f, scale);
    resized(); // Call resized() to update font size based on the new scale.
}

bool FreqTextLabel::isMouseOverCustom()
{
    // Checks if the mouse is over this component OR its child label.
    return isMouseOver() || freqLabel.isMouseOverOrDragging() || freqLabel.isBeingEdited();
}
