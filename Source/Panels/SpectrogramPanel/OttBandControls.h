#pragma once
#include "../../GUI/PrimarySlider.h"
#include "../../GUI/LookAndFeel.h"
#include "../../PluginProcessor.h"

// Overlay sliders reuse the plugin's guarded pointer, keyboard, accessibility
// and host-gesture implementation. Their hit targets cover only the two lines.
class OttBandControls final : public juce::Component
{
public:
    class ThresholdSlider final : public PrimarySlider
    {
    public:
        ThresholdSlider(FireAudioProcessor& processor, int band, bool isUpper)
            : upper(isUpper)
        {
            const auto id = ParameterIDAndName::getIDString(upper ? OTT_DOWNWARD_ID : OTT_UPWARD_ID, band);
            opposite = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(upper ? OTT_UPWARD_ID : OTT_DOWNWARD_ID, band));
            getProperties().set("ottThreshold", true);
            setComponentID(id);
            setTitle("Band " + juce::String(band + 1) + (upper ? " OTT downward threshold" : " OTT upward threshold"));
            setTooltip(getTitle() + ". Drag vertically or use arrow keys. Double-click to reset.");
            setSliderStyle(juce::Slider::LinearVertical);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            setSliderSnapsToMousePosition(false);
            setScrollWheelEnabled(false);
            setTextValueSuffix(" dB");
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, id, *this);
        }
        ~ThresholdSlider() override { dismissTransientInteraction(); }
        void resized() override
        {
            PrimarySlider::resized();
            setMouseDragSensitivity(juce::jmax(1, juce::roundToInt((getMaximum() - getMinimum()) * getHeight() / 100.0)));
        }
        float lineY() const { return juce::jmap(static_cast<float>(getValue()), -100.0f, 0.0f, static_cast<float>(getHeight()), 0.0f); }
        bool hitTest(int x, int y) override
        {
            return x >= 10 * uiScale() && x < getWidth() - 10 * uiScale() && std::abs(static_cast<float>(y) - lineY()) <= 7.0f * uiScale();
        }
        void paint(juce::Graphics& g) override
        {
            const auto y = lineY();
            const auto scale = uiScale();
            const auto colour = isEnabled() ? fire::ui::colours::positive : fire::ui::colours::disabled;
            g.setColour(colour.withAlpha(0.65f + 0.30f * getHoverAnimation()));
            g.drawLine(10.0f * scale, y, getWidth() - 10.0f * scale, y, 1.8f * scale);
            const auto x = getWidth() * (upper ? 0.73f : 0.27f);
            g.fillEllipse(x - 4 * scale, y - 4 * scale, 8 * scale, 8 * scale);
            const auto separation = opposite ? std::abs(getValue() - opposite->load()) * getHeight() / 100.0 : 100.0;
            if (getWidth() > 180 * scale && separation > 18 * scale)
            {
                g.setFont(fire::ui::valueFont(10.0f * scale));
                g.drawText((upper ? "D " : "U ") + juce::String(getValue(), 1),
                    juce::Rectangle<float>(juce::jmin(x + 8 * scale, getWidth() - 82 * scale), y + 4 * scale, 72 * scale, 14 * scale), juce::Justification::centredLeft);
            }
            if (getFocusAnimation() > 0.01f)
            {
                g.setColour(fire::ui::colours::textPrimary.withAlpha(getFocusAnimation()));
                g.drawLine(10.0f * scale, y, getWidth() - 10.0f * scale, y, 3.0f * scale);
            }
        }
        void valueChanged() override
        {
            if (opposite == nullptr) return;
            const double other = peerValue ? peerValue() : opposite->load();
            const auto legal = upper ? juce::jmax(getValue(), other + 6.0) : juce::jmin(getValue(), other - 6.0);
            if (! juce::approximatelyEqual(legal, getValue()))
                setValue(legal, juce::dontSendNotification);
        }
        std::function<double()> peerValue;
    private:
        float uiScale() const
        {
            if (auto* look = dynamic_cast<const FireLookAndFeel*>(&getLookAndFeel())) return look->scale;
            return 1.0f;
        }
        bool upper;
        std::atomic<float>* opposite = nullptr;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    OttBandControls(FireAudioProcessor& processor, int band)
        : up(processor, band, false), down(processor, band, true)
    {
        enabled = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_ENABLED_ID, band));
        bandEnabled = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(BAND_ENABLE_ID, band));
        rawUp = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_UPWARD_ID, band));
        rawDown = processor.treeState.getRawParameterValue(ParameterIDAndName::getIDString(OTT_DOWNWARD_ID, band));
        down.peerValue = [this] { return up.getValue(); };
        setInterceptsMouseClicks(false, true);
        addAndMakeVisible(up);
        addAndMakeVisible(down);
    }
    ~OttBandControls() override { down.peerValue = nullptr; dismiss(); }
    void resized() override { up.setBounds(getLocalBounds()); down.setBounds(getLocalBounds()); }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    void refresh()
    {
        const juce::Component::SafePointer<OttBandControls> safeThis(this);
        const bool active = bandEnabled != nullptr && bandEnabled->load() > 0.5f;
        up.setEnabled(active);
        if (! safeThis) return;
        down.setEnabled(active);
        if (! safeThis) return;
        // Host automation may cross the two raw parameters. Display the same
        // effective lower threshold as DSP, without writing back to the host.
        if (rawUp && rawDown && ! up.hasActivePointerGesture() && ! down.hasActivePointerGesture())
        {
            up.setValue(juce::jmin(rawUp->load(), rawDown->load() - 6.0f), juce::dontSendNotification);
            down.setValue(rawDown->load(), juce::dontSendNotification);
        }
        repaint();
    }
    void dismiss()
    {
        const juce::Component::SafePointer<OttBandControls> safeThis(this);
        up.dismissTransientInteraction();
        if (safeThis) down.dismissTransientInteraction();
    }
    void setMeter(float value) { gain = std::isfinite(value) ? value : 0.0f; }
    void paint(juce::Graphics& g) override
    {
        if (getWidth() < 85) return;
        const bool on = enabled != nullptr && enabled->load() > 0.5f;
        g.setColour(on ? fire::ui::colours::textSecondary : fire::ui::colours::textMuted);
        const auto* look = dynamic_cast<const FireLookAndFeel*>(&getLookAndFeel());
        const auto scale = look ? look->scale : 1.0f;
        g.setFont(fire::ui::valueFont(10 * scale));
        g.drawText(on ? "OTT " + juce::String(gain, 1) + " dB" : "OTT OFF",
                   getLocalBounds().removeFromBottom(juce::roundToInt(16 * scale)), juce::Justification::centred);
    }
    ThresholdSlider up, down;
private:
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* bandEnabled = nullptr;
    std::atomic<float>* rawUp = nullptr;
    std::atomic<float>* rawDown = nullptr;
    float gain = 0.0f;
};
