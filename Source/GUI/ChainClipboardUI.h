#pragma once
#include "PrimaryButton.h"
#include "../Utility/ChainSnapshotCodec.h"

namespace fire::ui
{
inline const juce::String chainRoutingNote =
    "Routing uses the destination instance's current LFOs, envelopes, macros and MIDI CC sources.";

inline juce::String copiedModuleName(const std::shared_ptr<const chain::Snapshot>& snapshot)
{
    return snapshot && snapshot->modules.size() == 1
        ? juce::String(effects::name(snapshot->modules.front().type)) + (snapshot->modules.front().branch==0?" (A)":" (B)") : juce::String();
}

class ChainMenuButton final : public PrimaryTextButton
{
public:
    ChainMenuButton() : PrimaryTextButton("...")
    {
        setTitle("Chain tools");
        setTooltip("Copy, paste and save this effect chain. " + chainRoutingNote);
    }
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        if (! isLineSkin(*this))
        {
            PrimaryTextButton::paintButton(g, over, down);
            return;
        }
        const auto& palette = paletteFor(*this);
        const auto area = getLocalBounds().toFloat().reduced(1.0f);
        if (over || down || hasKeyboardFocus(false))
        {
            g.setColour(palette.raised.withAlpha(down ? .85f : .6f));
            g.fillRoundedRectangle(area, 3.0f);
        }
        g.setColour(isEnabled() ? palette.textSecondary : palette.textMuted);
        const float radius = juce::jlimit(1.2f, 2.0f, area.getHeight() * .07f);
        const auto centre = area.getCentre();
        for (int dot = -1; dot <= 1; ++dot)
            g.fillEllipse(centre.x + dot * radius * 3.3f - radius,
                          centre.y - radius, radius * 2, radius * 2);
    }
};
}
