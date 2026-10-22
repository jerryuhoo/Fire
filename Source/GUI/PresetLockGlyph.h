#pragma once
#include "PrimaryButton.h"

namespace fire::ui
{
inline void drawPresetLock(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    if (area.isEmpty()) return;
    const float unit = juce::jmin(area.getWidth(), area.getHeight()) / 12.0f;
    const auto centre = area.getCentre();
    g.setColour(colour);
    g.drawRoundedRectangle(centre.x - 4 * unit, centre.y - unit, 8 * unit, 6 * unit, 1.3f * unit, juce::jmax(.7f, unit));
    juce::Path shackle;
    shackle.startNewSubPath(centre.x - 2.5f * unit, centre.y - unit);
    shackle.lineTo(centre.x - 2.5f * unit, centre.y - 3 * unit);
    shackle.cubicTo(centre.x - 2.5f * unit, centre.y - 6.5f * unit,
                    centre.x + 2.5f * unit, centre.y - 6.5f * unit, centre.x + 2.5f * unit, centre.y - 3 * unit);
    shackle.lineTo(centre.x + 2.5f * unit, centre.y - unit);
    g.strokePath(shackle, juce::PathStrokeType(juce::jmax(.7f, unit), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
class PresetLocksButton final : public PrimaryTextButton
{
public:
    PresetLocksButton()
    {
        setComponentID("header_preset_locks"); setTitle("Manage preset parameter locks");
        setTooltip("Keep selected base values when loading presets. Manual edits, automation and the new preset's modulation still work. Click to manage or unlock all.");
    }
    void setCount(int value) { count = value; setButtonText(juce::String(count)); setTitle("Manage " + juce::String(count) + " preset parameter locks"); repaint(); }
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const auto& palette = paletteFor(*this);
        auto area = getLocalBounds().toFloat().reduced(3);
        if (over || down || hasKeyboardFocus(false))
        { g.setColour(palette.accent.withAlpha(down ? .18f : .09f)); g.fillRoundedRectangle(area, 3); }
        const auto size = juce::jmin(15.f, area.getHeight() * .58f);
        drawPresetLock(g, area.removeFromLeft(size + 4).withSizeKeepingCentre(size,size), palette.accent);
        g.setColour(palette.textPrimary); g.setFont(bodyFont(11));
        g.drawText(juce::String(count), area, juce::Justification::centred);
    }
private:
    int count = 0;
};
}
