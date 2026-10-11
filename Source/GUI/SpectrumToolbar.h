#pragma once
#include "FireTheme.h"
#include "Skin.h"
#include <array>

namespace fire::ui
{
// Shared surface for the analyser's compact, separately accessible controls.
// Keeping the controls as editor children preserves their input/lifetime guards.
class SpectrumToolbar final : public juce::Component
{
public:
    SpectrumToolbar() { setComponentID("spectrum_toolbar"); }
    void setDividers(int first, int second)
    {
        dividers = {first, second};
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        const auto& palette = paletteFor(*this);
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const float unit = getHeight() / 24.0f;
        const float radius = 5.0f * unit;
        g.setColour(palette.surface0.withAlpha(0.96f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(palette.hairline.withAlpha(0.75f));
        g.drawRoundedRectangle(bounds, radius, juce::jmax(0.7f, unit * 0.65f));
        g.setColour(palette.hairline.withAlpha(0.55f));
        for (int x : dividers)
            if (x > 0) g.drawVerticalLine(x, bounds.getY() + 6 * unit, bounds.getBottom() - 6 * unit);
    }
private:
    std::array<int, 2> dividers {-1, -1};
};
}
