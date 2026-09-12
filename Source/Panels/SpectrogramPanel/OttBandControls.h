#pragma once
#include "../../GUI/PrimarySlider.h"
#include "../../GUI/LookAndFeel.h"
#include "../../GUI/OttVisuals.h"
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
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
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
        bool isInteracting() const { return hasActivePointerGesture() || getFocusAnimation() > 0.01f; }
        float getReadoutOpacity() const noexcept { return readout; }
        void resetReadout() noexcept { readout = 0.0f; }
        void advanceVisuals(float dt, bool processingEnabled, bool externalDrag,
                            const fire::ui::OttSpectrumProfile& profile, float dynamics)
        {
            processing = processingEnabled;
            spectrum = profile;
            activity = dynamics;
            fire::ui::advanceReadout(readout, isInteracting() || externalDrag, dt);
        }
        void paint(juce::Graphics& g) override
        {
            if (getLocalBounds().isEmpty()) return;
            const auto y = lineY();
            const auto scale = uiScale();
            const auto base = isEnabled() ? fire::ui::colours::ott : fire::ui::colours::disabled;
            const auto colourAt = [&](float energy) {
                const auto alpha = juce::jlimit(0.0f, 0.9f,
                    (processing ? 0.24f : 0.10f) + 0.18f * getHoverAnimation()
                    + 0.40f * readout + energy * activity * 0.20f);
                return base.withMultipliedSaturation(0.35f + 0.9f * energy + readout * 0.3f).withAlpha(alpha);
            };
            juce::ColourGradient ink(colourAt(spectrum.front()), 10 * scale, y,
                                      colourAt(spectrum.back()), getWidth() - 10 * scale, y, false);
            for (size_t i = 1; i + 1 < spectrum.size(); ++i)
            {
                ink.addColour(static_cast<double>(i) / static_cast<double>(spectrum.size() - 1),
                               colourAt(spectrum[i]));
            }
            g.setGradientFill(ink);
            g.drawLine(10.0f * scale, y, getWidth() - 10.0f * scale, y,
                       (1.05f + 0.7f * getFocusAnimation()) * scale);
            auto area = getLocalBounds().toFloat().reduced(4.0f * scale);
            if (readout < 0.003f || area.isEmpty()) return;
            auto label = juce::Rectangle<float>(juce::jmin(112.0f * scale, area.getWidth()),
                                                juce::jmin(19.0f * scale, area.getHeight()))
                .withCentre({area.getCentreX(), juce::jmax(32.0f * scale, y + 15.0f * scale)})
                .constrainedWithin(area);
            g.setColour(fire::ui::colours::surface1.withAlpha(readout * 0.94f));
            g.fillRoundedRectangle(label, 4.0f * scale);
            g.setColour(fire::ui::colours::textPrimary.withAlpha(readout));
            g.setFont(fire::ui::valueFont(11.0f * scale));
            g.drawFittedText((upper ? "Down " : "Up ") + juce::String(getValue(), 1) + " dB",
                             label.toNearestInt(), juce::Justification::centred, 1);
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
        bool processing = false;
        float readout = 0.0f, activity = 0.0f;
        fire::ui::OttSpectrumProfile spectrum {};
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
    void refresh(float deltaSeconds = 1.0f / 60.0f)
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
        const bool on = active && enabled != nullptr && enabled->load() > 0.5f;
        const bool liftPreview = up.isInteracting() || externalUp;
        const bool pressPreview = down.isInteracting() || externalDown;
        motion.advance(deltaSeconds, on ? dynamicsActivity : 0.0f, liftPreview, pressPreview, centroid);
        up.advanceVisuals(deltaSeconds, on, externalUp, spectrum, motion.lift);
        down.advanceVisuals(deltaSeconds, on, externalDown, spectrum, motion.press);
        // Keep the moving ripples at the presentation rate, but do not redraw
        // the whole analyser band after silence and readouts have settled.
        const std::array<double, 9> visualState {
            motion.lift, motion.press, motion.phase, motion.liftPreview, motion.pressPreview,
            up.getReadoutOpacity(), down.getReadoutOpacity(), up.getValue(), down.getValue()
        };
        if (visualState != lastVisualState || spectrum != lastSpectrum || on != lastProcessingState)
        {
            lastVisualState = visualState;
            lastSpectrum = spectrum;
            lastProcessingState = on;
            repaint();
        }
    }
    void dismiss()
    {
        const juce::Component::SafePointer<OttBandControls> safeThis(this);
        up.dismissTransientInteraction();
        if (! safeThis) return;
        down.dismissTransientInteraction();
        if (! safeThis) return;
        up.resetReadout(); down.resetReadout();
        motion.reset(); dynamicsActivity = 0.0f;
        externalUp = externalDown = false;
    }
    void setMeter(float dynamicsDb) { dynamicsActivity = std::isfinite(dynamicsDb) ? dynamicsDb : 0.0f; }
    void setSpectrum(const fire::ui::OttSpectrumProfile& profile, float frequencyCentroid)
    {
        spectrum = profile;
        centroid = frequencyCentroid;
    }
    const fire::ui::OttSpectrumProfile& getSpectrum() const noexcept { return spectrum; }
    float getCentroid() const noexcept { return centroid; }
    int getInteractionDirection() const { return (up.isInteracting() ? 1 : 0) | (down.isInteracting() ? 2 : 0); }
    void setExternalInteraction(bool upward, bool downward) { externalUp = upward; externalDown = downward; }
    void paint(juce::Graphics& g) override
    {
        const auto* look = dynamic_cast<const FireLookAndFeel*>(&getLookAndFeel());
        const auto scale = look ? look->scale : 1.0f;
        const auto area = getLocalBounds().toFloat().reduced(10.0f * scale, 0.0f).withTrimmedTop(24.0f * scale);
        fire::ui::drawOttRipples(g, area, up.lineY(), spectrum, motion.lift, false, motion.phase, scale,
                                 motion.liftPreview);
        fire::ui::drawOttRipples(g, area, down.lineY(), spectrum, motion.press, true, motion.phase, scale,
                                 motion.pressPreview);
    }
    ThresholdSlider up, down;
private:
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* bandEnabled = nullptr;
    std::atomic<float>* rawUp = nullptr;
    std::atomic<float>* rawDown = nullptr;
    float dynamicsActivity = 0.0f, centroid = 0.5f;
    bool externalUp = false, externalDown = false;
    fire::ui::OttSpectrumProfile spectrum {};
    fire::ui::OttRippleMotion motion;
    std::array<double, 9> lastVisualState {};
    fire::ui::OttSpectrumProfile lastSpectrum {};
    bool lastProcessingState = false;
};
