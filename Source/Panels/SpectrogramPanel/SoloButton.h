/*
  ==============================================================================

    SoloButton.h
    Created: 3 Dec 2020 8:18:19pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../GUI/FireTheme.h"
#include "../../GUI/PrimaryButton.h"

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct BandToggleButtonPointerTestAccess;
#endif

//==============================================================================
/*
*/
class SoloButton : public juce::ToggleButton,
                   private juce::Timer
{
public:
    SoloButton();
    ~SoloButton() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void triggerClick() override;
    void dismissPointerGesture() noexcept;

private:
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct BandToggleButtonPointerTestAccess;
#endif

    void visibilityChanged() override;
    void enablementChanged() override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    void buttonStateChanged() override;
    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;
    void timerCallback() override;
    void updateAnimationTargets() noexcept;
    void startAnimationIfNeeded() noexcept;
    bool advanceAnimation(float deltaSeconds) noexcept;
    void recoverMissingPointerUp(const juce::MouseEvent& event);
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;

    bool isEntered = false;
    bool primaryPointerDown = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;
    fire::ui::DampedValue focusAnimation;
    fire::ui::DampedValue enabledAnimation;
    juce::Colour getColour() const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloButton)
};
