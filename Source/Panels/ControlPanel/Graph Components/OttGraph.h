#pragma once
#include "GraphTemplate.h"
#include "../../../DSP/OttProcessor.h"

class OttGraph final : public GraphTemplate
{
public:
    OttGraph() { setGraphIdentity("OTT DYNAMICS", fire::ui::ModuleRole::compressor); }
    void setLevels(float inputDb, float gainDb)
    {
        if (juce::approximatelyEqual(input, inputDb) && juce::approximatelyEqual(gain, gainDb)) return;
        input = std::isfinite(inputDb) ? inputDb : -120.0f;
        gain = std::isfinite(gainDb) ? gainDb : 0.0f;
        repaint();
    }
    void setThresholds(float lowerDb, float upperDb)
    {
        if (juce::approximatelyEqual(lower, juce::jmin(lowerDb, upperDb - 6.0f)) && juce::approximatelyEqual(upper, upperDb)) return;
        lower = juce::jmin(lowerDb, upperDb - 6.0f);
        upper = upperDb;
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        GraphTemplate::paint(g);
        auto plot = getGraphPlotBounds().reduced(8.0f);
        const auto barWidth = plot.getWidth() * 0.26f;
        auto levels = plot.withWidth(barWidth);
        auto changes = plot.withWidth(barWidth).withRightX(plot.getRight());
        const auto y = [&](float db) { return juce::jmap(juce::jlimit(-96.0f, 0.0f, db), -96.0f, 0.0f, plot.getBottom(), plot.getY()); };
        g.setColour(fire::ui::colours::surface2);
        g.fillRoundedRectangle(levels, 4.0f);
        g.fillRoundedRectangle(changes, 4.0f);
        g.setColour(fire::ui::colours::textSecondary.withAlpha(0.6f));
        g.fillRect(levels.withTop(y(input)));
        for (float threshold : { lower, upper })
        {
            g.setColour(fire::ui::colours::positive);
            g.drawHorizontalLine(juce::roundToInt(y(threshold)), levels.getX() - 4, levels.getRight() + 4);
        }
        const auto zero = plot.getCentreY();
        const auto gainY = zero - juce::jlimit(-24.0f, 24.0f, gain) / 24.0f * plot.getHeight() * 0.5f;
        g.setColour(gain >= 0 ? fire::ui::colours::positive : fire::ui::colours::ember);
        g.fillRect(changes.withTop(juce::jmin(zero, gainY)).withBottom(juce::jmax(zero, gainY)));
        g.setColour(fire::ui::colours::textSecondary);
        g.drawHorizontalLine(juce::roundToInt(zero), changes.getX() - 4, changes.getRight() + 4);
        g.setFont(fire::ui::valueFont(11.0f * getScale()));
        auto text = plot.withTrimmedLeft(barWidth + 5).withTrimmedRight(barWidth + 5);
        g.drawFittedText("IN\n" + juce::String(input, 1) + " dB\n\nGAIN\n"
                         + (gain > 0 ? "+" : "") + juce::String(gain, 1) + " dB",
                         text.toNearestInt(), juce::Justification::centred, 5);
    }
private:
    float input = -120.0f, gain = 0.0f, lower = -48.0f, upper = -18.0f;
};
