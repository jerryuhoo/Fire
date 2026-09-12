#pragma once
#include "GraphTemplate.h"
#include "../../../GUI/OttVisuals.h"

class OttGraph final : public GraphTemplate
{
public:
    OttGraph() { setGraphIdentity("LIFT / PRESS", fire::ui::ModuleRole::ott); }
    void setLevels(float inputDb, float gainDb, float activityDb = 0.0f)
    {
        inputDb = std::isfinite(inputDb) ? inputDb : -120.0f;
        gainDb = std::isfinite(gainDb) ? gainDb : 0.0f;
        activityDb = std::isfinite(activityDb) ? activityDb : 0.0f;
        visualDirty = visualDirty || input != inputDb || gain != gainDb || activity != activityDb;
        input = inputDb;
        gain = gainDb;
        activity = activityDb;
    }
    void setThresholds(float lowerDb, float upperDb)
    {
        lowerDb = juce::jmin(lowerDb, upperDb - 6.0f);
        visualDirty = visualDirty || lower != lowerDb || upper != upperDb;
        lower = lowerDb;
        upper = upperDb;
    }
    void setSpectrum(const fire::ui::OttSpectrumProfile& profile, float frequencyCentroid)
    {
        visualDirty = visualDirty || spectrum != profile;
        spectrum = profile; centroid = frequencyCentroid;
    }
    void resetVisuals()
    {
        motion.reset(); spectrum.fill(0.0f);
        readout = 0.0f; shownInput = -120.0f; shownGain = 0.0f;
        visualDirty = true;
    }
    void advanceVisuals(float dt, bool reading, int previewDirection)
    {
        previewLift = (previewDirection & 1) != 0;
        previewPress = (previewDirection & 2) != 0;
        const auto previousInput = shownInput;
        const auto previousGain = shownGain;
        const auto previousMotion = motion;
        const bool readoutChanged = fire::ui::advanceReadout(readout, reading, dt);
        shownInput += (input - shownInput) * fire::ui::Motion::step(dt, 0.10f);
        shownGain += (gain - shownGain) * fire::ui::Motion::step(dt, 0.08f);
        motion.advance(dt, activity, previewLift, previewPress, centroid);
        // A settled, silent OTT view has no animated pixels. Keep the shared
        // clock for live meters and previews, but avoid invalidating its
        // background and title on every frame.
        if (visualDirty || readoutChanged || shownInput != previousInput
            || shownGain != previousGain || motion.phase != previousMotion.phase
            || motion.lift != previousMotion.lift || motion.press != previousMotion.press
            || motion.liftPreview != previousMotion.liftPreview
            || motion.pressPreview != previousMotion.pressPreview)
            repaint(getGraphPlotBounds().getSmallestIntegerContainer());
        visualDirty = false;
    }
    void paint(juce::Graphics& g) override
    {
        GraphTemplate::paint(g);
        const auto scale = getScale();
        auto plot = getGraphPlotBounds().reduced(12.0f * scale, 14.0f * scale);
        if (plot.isEmpty()) return;
        const auto inputX = plot.getX() + 5.0f * scale;
        const auto gainX = plot.getRight() - 5.0f * scale;
        const auto zero = plot.getCentreY();
        const auto inputY = juce::jmap(juce::jlimit(-96.0f, 0.0f, shownInput), -96.0f, 0.0f, plot.getBottom(), plot.getY());
        const auto gainY = zero - juce::jlimit(-24.0f, 24.0f, shownGain) / 24.0f * plot.getHeight() * 0.5f;
        g.setColour(fire::ui::colours::hairline.withAlpha(0.55f));
        g.drawVerticalLine(juce::roundToInt(inputX), plot.getY(), plot.getBottom());
        g.drawVerticalLine(juce::roundToInt(gainX), plot.getY(), plot.getBottom());
        g.setColour(fire::ui::colours::ott.withAlpha(0.60f));
        g.fillRect(juce::Rectangle<float>(inputX - 1.5f * scale, inputY, 3.0f * scale, plot.getBottom() - inputY));
        g.setColour((activity < 0 ? fire::ui::colours::ottPress : fire::ui::colours::ottLift).withAlpha(0.72f));
        g.fillRect(juce::Rectangle<float>(gainX - 1.5f * scale, juce::jmin(zero, gainY), 3.0f * scale, std::abs(gainY - zero)));
        g.setColour(fire::ui::colours::textMuted.withAlpha(0.45f));
        g.drawHorizontalLine(juce::roundToInt(zero), gainX - 5 * scale, gainX + 5 * scale);
        for (float threshold : {lower, upper})
        {
            const auto y = juce::jmap(threshold, -96.0f, 0.0f, plot.getBottom(), plot.getY());
            g.drawHorizontalLine(juce::roundToInt(y), inputX - 5 * scale, inputX + 5 * scale);
        }
        auto fluid = plot.reduced(16.0f * scale, 0.0f);
        fire::ui::drawOttRipples(g, fluid, zero, spectrum, motion.lift, false, motion.phase, scale, motion.liftPreview);
        fire::ui::drawOttRipples(g, fluid, zero, spectrum, motion.press, true, motion.phase, scale, motion.pressPreview);
        if (readout > 0.003f)
        {
            auto label = plot.withSizeKeepingCentre(juce::jmin(plot.getWidth(), 126.0f * scale), 31.0f * scale);
            label.setY(plot.getBottom() - label.getHeight());
            g.setColour(fire::ui::colours::surface0.withAlpha(readout * 0.85f));
            g.fillRoundedRectangle(label, 4.0f * scale);
            g.setColour(fire::ui::colours::textSecondary.withAlpha(readout));
            g.setFont(fire::ui::valueFont(11.0f * scale));
            g.drawFittedText("IN " + juce::String(input, 1) + " dB\nGAIN "
                            + (gain > 0 ? "+" : "") + juce::String(gain, 1) + " dB",
                            label.toNearestInt(), juce::Justification::centred, 2);
        }
    }
private:
    float input = -120.0f, gain = 0.0f, activity = 0.0f;
    float shownInput = -120.0f, shownGain = 0.0f;
    float lower = -48.0f, upper = -18.0f, centroid = 0.5f, readout = 0.0f;
    bool previewLift = false, previewPress = false;
    bool visualDirty = true;
    fire::ui::OttSpectrumProfile spectrum {};
    fire::ui::OttRippleMotion motion;
};
