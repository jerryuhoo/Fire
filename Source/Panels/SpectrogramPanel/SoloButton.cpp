/*
  ==============================================================================

    SoloButton.cpp
    Created: 3 Dec 2020 8:18:19pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "SoloButton.h"

//==============================================================================
SoloButton::SoloButton()
{
}

SoloButton::~SoloButton()
{
}

void SoloButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.75f);
    const bool active = getToggleState();
    const auto accent = getColour();

    juce::ColourGradient metal(fire::ui::colours::raised.brighter(isEntered ? 0.10f : 0.03f),
                               bounds.getCentreX(), bounds.getY(),
                               fire::ui::colours::surface0, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(metal);
    g.fillEllipse(bounds);
    g.setColour((active ? accent : fire::ui::colours::hairline)
                    .withAlpha(active ? 0.88f : (isEntered ? 0.78f : 0.58f)));
    g.drawEllipse(bounds, 1.0f);

    g.setFont(fire::ui::labelFont(juce::jlimit(8.0f, 13.0f, bounds.getHeight() * 0.52f)));
    g.setColour(active ? fire::ui::colours::whiteHot
                       : fire::ui::colours::textMuted.brighter(isEntered ? 0.18f : 0.0f));
    g.drawText("S", bounds, juce::Justification::centred);
}

void SoloButton::resized()
{
}

void SoloButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseEnter(e);
    isEntered = true;
    repaint();
}

void SoloButton::mouseExit(const juce::MouseEvent& e)
{
    juce::ToggleButton::mouseExit(e);
    isEntered = false;
    repaint();
}

juce::Colour SoloButton::getColour()
{
    if (isEntered)
    {
        if (! getToggleState())
        {
            return fire::ui::colours::disabled.brighter(0.10f);
        }
        else
        {
            return fire::ui::colours::gold.brighter(0.08f);
        }
    }
    else
    {
        if (! getToggleState())
        {
            return fire::ui::colours::disabled;
        }
        else
        {
            return fire::ui::colours::gold;
        }
    }
}
