/*
  ==============================================================================

    FireTooltipWindow.h
    Tooltip timing and window presentation shared by the Fire editor.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FireTooltipWindow final : public juce::TooltipWindow
{
public:
    static constexpr int hoverDelayMilliseconds = 900;

    explicit FireTooltipWindow(juce::Component& parent)
        : juce::TooltipWindow(&parent, hoverDelayMilliseconds)
    {
        // The Fire tooltip renderer has genuinely rounded transparent corners.
        // JUCE marks TooltipWindow opaque by default, which can leave a square
        // backing region when the window is embedded in a plug-in editor.
        setOpaque(false);
    }

    int getConfiguredDelayMilliseconds() const noexcept
    {
        return hoverDelayMilliseconds;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireTooltipWindow)
};
