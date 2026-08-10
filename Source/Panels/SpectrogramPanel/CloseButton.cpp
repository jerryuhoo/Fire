/*
  ==============================================================================

    CloseButton.cpp
    Created: 8 Nov 2020 7:57:32pm
    Author:  羽翼深蓝Wings

 ==============================================================================
*/

#include "CloseButton.h"

//==============================================================================
CloseButton::CloseButton ()
{
    setClickingTogglesState(false);
}

CloseButton::~CloseButton()
{
}

void CloseButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (isEntered)
    {
        g.setColour(fire::ui::colours::danger.withAlpha(0.13f));
        g.fillEllipse(bounds);
        g.setColour(fire::ui::colours::danger.withAlpha(0.62f));
        g.drawEllipse(bounds, 1.0f);
    }

    const auto cross = bounds.reduced(bounds.getWidth() * 0.29f);
    g.setColour((isEntered ? fire::ui::colours::danger : fire::ui::colours::textMuted)
                    .withAlpha(isEntered ? 0.94f : 0.58f));
    const auto stroke = juce::jmax(1.0f, bounds.getWidth() * 0.085f);
    g.drawLine({ cross.getTopLeft(), cross.getBottomRight() }, stroke);
    g.drawLine({ cross.getTopRight(), cross.getBottomLeft() }, stroke);
}

void CloseButton::resized()
{
}

void CloseButton::mouseDown(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseDown(e);
}

void CloseButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseEnter(e);
    isEntered = true;
    repaint();
}

void CloseButton::mouseExit(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseExit(e);
    isEntered = false;
    repaint();
}
