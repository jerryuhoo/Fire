/*
  ==============================================================================

    CloseButton.h
    Created: 8 Nov 2020 7:57:32pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/FireTheme.h"

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct CloseButtonPointerTestAccess;
#endif

//==============================================================================
/*
*/
class CloseButton : public juce::Button
{
public:
    static constexpr int minimumHitTargetSize = 24;

    CloseButton();
    ~CloseButton() override = default;

    // A hidden target remains paintable while it fades out, but stops
    // intercepting input immediately. Non-animated presentation is used when
    // synchronising the initial layout before the editor is shown.
    void setPresented(bool shouldBePresented, bool animate = true);
    bool advanceAnimation(float deltaSeconds);

    float getVisibilityAnimation() const noexcept { return visibilityAnimation.current; }
    float getHoverAnimation() const noexcept { return hoverAnimation.current; }
    float getPressAnimation() const noexcept { return pressAnimation.current; }
    bool isPresented() const noexcept { return presentationTarget; }

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct CloseButtonPointerTestAccess;
#endif

    void paintButton(juce::Graphics&, bool, bool) override;
    void buttonStateChanged() override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void visibilityChanged() override;
    void enablementChanged() override;
    void updateInteractionTargets() noexcept;
    void dismissPointerGesture() noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent&);
    bool isPointerSource(const juce::MouseEvent&) const noexcept;

    bool presentationTarget = false;
    bool primaryPointerDown = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    fire::ui::DampedValue visibilityAnimation;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloseButton)
};
