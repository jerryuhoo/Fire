#pragma once

#include "FireTheme.h"
#include "Skin.h"
#include "PrimaryButton.h"

#include <cmath>
#include <functional>

namespace fire::ui
{
class LoudnessMatchControls final : public juce::Component
{
public:
    struct ViewState
    {
        bool enabled = false;
        bool measuring = false;
        bool ready = false;
        bool limited = false;
        bool noSignal = false;
        bool bypassed = false;
        int side = 0;
        float gainDb = 0.0f;
        float progress = 0.0f;
    };

    LoudnessMatchControls()
    {
        addAndMakeVisible(matchButton);
        addAndMakeVisible(learnButton);
        matchButton.setComponentID("loudnessMatchToggle");
        learnButton.setComponentID("loudnessMatchLearn");
        matchButton.setTitle("Loudness matching");
        matchButton.setToggleable(true);
        matchButton.setClickingTogglesState(false);
        matchButton.setTooltip("Match the listening level of A and B. Learn each side for 3 seconds "
                               "using the same loop. Compensation stays fixed until you learn again.");

        const juce::Component::SafePointer<LoudnessMatchControls> safeThis(this);
        matchButton.onClick = [safeThis]
        {
            if (safeThis == nullptr || ! safeThis->canInteract())
                return;
            const bool nextEnabled = ! safeThis->state.enabled;
            auto callback = safeThis->onEnabledChanged;
            if (callback)
                callback(nextEnabled);
        };
        learnButton.onClick = [safeThis]
        {
            if (safeThis == nullptr || ! safeThis->canInteract()
                || ! safeThis->state.enabled || safeThis->state.bypassed)
                return;
            auto callback = safeThis->onLearn;
            if (callback)
                callback();
        };

        setSize(164, 32);
        setState({});
    }

    ViewState getState() const noexcept { return state; }

    void setState(const ViewState& requestedState)
    {
        auto next = requestedState;
        next.side = next.side == 1 ? 1 : 0;
        next.gainDb = std::isfinite(next.gainDb)
                          ? juce::jlimit(-18.0f, 18.0f, next.gainDb) : 0.0f;
        next.progress = std::isfinite(next.progress)
                            ? juce::jlimit(0.0f, 1.0f, next.progress) : 0.0f;
        if (hasState && statesMatch(state, next))
            return;

        const bool sessionChanged = hasState
            && (state.side != next.side || state.enabled != next.enabled
                || state.measuring != next.measuring || state.bypassed != next.bypassed);
        state = next;
        hasState = true;
        const juce::Component::SafePointer<LoudnessMatchControls> safeThis(this);
        if (sessionChanged)
            dismiss();
        if (safeThis == nullptr)
            return;

        matchButton.setToggleState(state.enabled, juce::dontSendNotification);
        if (safeThis == nullptr)
            return;
        matchButton.setPresentation("Match", state.enabled ? colours::positive : colours::textMuted);
        if (safeThis == nullptr)
            return;
        learnButton.setEnabled(state.enabled && ! state.bypassed);
        if (safeThis == nullptr)
            return;

        const auto side = juce::String(state.side == 0 ? "A" : "B");
        juce::String label;
        juce::String action;
        auto accent = colours::textSecondary;
        if (! state.enabled)
        {
            label = side + " · Off";
            action = "Turn Match on to learn side " + side + ".";
        }
        else if (state.bypassed)
        {
            label = side + " · Bypassed";
            action = "Matching is paused while the plug-in is bypassed.";
        }
        else if (state.noSignal)
        {
            label = side + " · Play audio";
            action = state.measuring ? "Play audio to learn side " + side + ". Click to cancel."
                                     : "Play audio, then click to learn side " + side + ".";
            accent = colours::gold;
        }
        else if (state.measuring)
        {
            label = side + " · Learn " + juce::String(juce::roundToInt(state.progress * 100.0f)) + "%";
            action = "Learning side " + side + ". Click to cancel.";
            accent = colours::gold;
        }
        else if (state.ready)
        {
            const float displayedGain = std::abs(state.gainDb) < 0.05f ? 0.0f : state.gainDb;
            label = side + " " + (displayedGain >= 0.0f ? "+" : "")
                    + juce::String(displayedGain, 1) + " dB";
            if (state.limited)
                label += " LIMIT";
            action = "Side " + side + " uses fixed compensation. Click to learn again.";
            if (state.limited)
                action += " Compensation has reached the ±18 dB limit.";
            accent = state.limited ? colours::gold : colours::positive;
        }
        else
        {
            label = side + " · Learn";
            action = "Click to learn side " + side + " for 3 seconds.";
        }

        learnButton.setTitle("Loudness match: " + label);
        if (safeThis == nullptr)
            return;
        learnButton.setTooltip(action + " Use the same loop when learning A and B. "
                               "Compensation stays fixed until you learn again.");
        if (safeThis == nullptr)
            return;
        learnButton.setPresentation(label, accent,
            state.enabled && state.measuring && ! state.bypassed, state.progress);
    }

    void dismiss()
    {
        const juce::Component::SafePointer<LoudnessMatchControls> safeThis(this);
        matchButton.dismissPointerGesture();
        if (safeThis != nullptr)
            learnButton.dismissPointerGesture();
    }

    void resized() override
    {
        const auto gap = juce::jlimit(0, getWidth() / 8,
                                      juce::roundToInt(getHeight() * 3.0f / 32.0f));
        const auto availableWidth = juce::jmax(0, getWidth() - gap);
        const auto matchWidth = juce::roundToInt(availableWidth * 0.34f);
        matchButton.setBounds(0, 0, matchWidth, getHeight());
        learnButton.setBounds(matchWidth + gap, 0, availableWidth - matchWidth, getHeight());
    }

    std::function<void(bool)> onEnabledChanged;
    std::function<void()> onLearn;

private:
    class StateButton final : public PrimaryTextButton
    {
    public:
        void triggerClick() override
        {
            if (isEnabled() && isShowing())
                PrimaryTextButton::triggerClick();
        }

        void setPresentation(const juce::String& text, juce::Colour colour,
                             bool showProgress = false, float newProgress = 0.0f)
        {
            const bool changed = accent != colour || progressVisible != showProgress
                                 || ! juce::exactlyEqual(progress, newProgress);
            accent = colour;
            progressVisible = showProgress;
            progress = newProgress;
            if (getButtonText() != text)
                setButtonText(text);
            else if (changed)
                repaint();
        }

    private:
        void paintButton(juce::Graphics& g, bool, bool) override
        {
            if (getWidth() <= 1 || getHeight() <= 1)
                return;
            const auto scale = juce::jlimit(0.4f, 3.0f, getHeight() / 32.0f);
            const auto& palette = paletteFor(*this);
            const auto currentAccent = accent == colours::textMuted ? palette.textMuted
                : accent == colours::textSecondary ? palette.textSecondary : accent;
            const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            const auto radius = juce::jmin(Metrics::radiusSmall * scale, bounds.getHeight() * 0.5f);
            const auto hover = getHoverAnimation();
            const auto press = getPressAnimation();
            const auto focus = getFocusAnimation();
            const auto disabled = getDisabledAnimation();
            const auto alpha = 1.0f - disabled * 0.38f;
            const auto washAlpha = juce::jlimit(0.0f, 1.0f,
                (getToggleState() ? 1.0f : 0.56f * hover + 0.72f * press + 0.34f * focus)
                    * (1.0f - disabled));

            // Match shares the header's quiet idle chrome. Selection and
            // keyboard focus use short rails, leaving the learning track clear.
            if (washAlpha > 0.001f)
            {
                const auto wash = getToggleState() ? palette.raised
                    : palette.surface2.interpolatedWith(palette.raised, press);
                g.setColour(wash.withAlpha(washAlpha));
                g.fillRoundedRectangle(bounds, radius);
            }
            if (getToggleState())
            {
                const auto rail = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.42f, 2.0f * scale)
                                        .withBottomY(bounds.getBottom());
                g.setColour(currentAccent.withAlpha(0.72f * (1.0f - disabled)));
                g.fillRect(rail);
            }
            if (focus > 0.001f)
            {
                const auto rail = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.55f, scale)
                                        .withY(bounds.getY());
                g.setColour(colours::gold.withAlpha(focus * 0.65f * alpha));
                g.fillRect(rail);
            }
            if (progressVisible)
            {
                auto track = bounds.reduced(3.0f * scale, 0.0f).removeFromBottom(2.0f * scale);
                g.setColour(currentAccent.withAlpha(0.18f));
                g.fillRect(track);
                g.setColour(currentAccent.withAlpha(0.72f));
                g.fillRect(track.withWidth(track.getWidth() * progress));
            }
            g.setColour(currentAccent.brighter(0.14f * hover).darker(0.08f * press).withMultipliedAlpha(alpha));
            g.setFont(bodyFont(10.5f * scale));
            g.drawFittedText(getButtonText(), getLocalBounds().reduced(3, 2),
                             juce::Justification::centred, 1);
        }

        juce::Colour accent = colours::textSecondary;
        bool progressVisible = false;
        float progress = 0.0f;
    };

    bool canInteract() const noexcept { return isEnabled() && isShowing(); }
    void visibilityChanged() override { if (! isShowing()) dismiss(); }
    void enablementChanged() override { if (! isEnabled()) dismiss(); }
    void parentHierarchyChanged() override { if (! isShowing()) dismiss(); }

    static bool statesMatch(const ViewState& a, const ViewState& b) noexcept
    {
        return a.enabled == b.enabled && a.measuring == b.measuring
            && a.ready == b.ready && a.limited == b.limited
            && a.noSignal == b.noSignal && a.bypassed == b.bypassed
            && a.side == b.side && juce::exactlyEqual(a.gainDb, b.gainDb)
            && juce::exactlyEqual(a.progress, b.progress);
    }

    ViewState state;
    bool hasState = false;
    StateButton matchButton, learnButton;
};
} // namespace fire::ui
