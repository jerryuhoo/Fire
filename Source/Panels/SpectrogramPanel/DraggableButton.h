/*
  ==============================================================================

    DraggableButton.h
    Created: 7 Oct 2021 11:42:21pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "../../GUI/FireTheme.h"
#include "juce_gui_basics/juce_gui_basics.h"

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct DraggableButtonPointerTestAccess;
#endif

//==============================================================================
/*
*/
class DraggableButton : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    DraggableButton();
    ~DraggableButton() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void setState(const bool state);
    void dismissTransientInteraction();
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    std::function<void(float)> onQValueChanged;
    std::function<void(DraggableButton&, const juce::MouseEvent&)> onDrag;
    std::function<void()> onDragFinished;

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct DraggableButtonPointerTestAccess;
#endif

    bool isEntered = false;
    bool primaryDragActive = false;
    bool keyboardFocusVisible = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    bool performKeyboardMove(juce::Point<float> localPosition);
    bool setAccessibleFrequency(double frequency);
    double getAccessibleFrequency() const noexcept;
    bool canAcceptKeyboardOrAccessibilityInput() const noexcept;
    void timerCallback() override;
    void updateAnimationTargets() noexcept;
    void startAnimationIfNeeded() noexcept;
    bool advanceAnimation(float deltaSeconds) noexcept;
    bool animationsSettled() const noexcept;
    bool hasPresentedInteraction() const noexcept;
    void clearInteractionPresentation() noexcept;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;
    void enablementChanged() override;
    void visibilityChanged() override;
    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;
    juce::Colour getColour() const;
    bool mState = true;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;
    fire::ui::DampedValue focusAnimation;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraggableButton)
};
