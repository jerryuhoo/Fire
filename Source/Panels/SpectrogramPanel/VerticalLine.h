/*
  ==============================================================================

    VerticalLine.h
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../GUI/FireTheme.h"
#include <cstdint>
#include <functional>
#include <memory>

#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
struct VerticalLinePointerTestAccess;
#endif
class FreqDividerGroup;

//==============================================================================
/*
*/
class VerticalLine : public juce::Slider
{
public:
    VerticalLine();
    ~VerticalLine() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool getState();
    void setState (bool state);
    void setDeleteState (bool deleteState);
    void setXPercent (float x);
    float getXPercent();
    void setIndex (int index);
    int getIndex();
    int getLeft();
    int getRight();
    void moveToX (int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[]);

    using ParameterGestureCallback = std::function<void()>;
    using ParameterGestureToken = std::shared_ptr<void>;
    using ParameterChangeCallback = std::function<ParameterGestureToken()>;
    using PointerGestureAdmissionCallback = std::function<bool(
        juce::MouseInputSource::InputSourceType, int)>;
    using UserPositionChangeCallback = std::function<void(float)>;
    void setParameterGestureCallbacks (ParameterGestureCallback gestureBegin,
                                       ParameterChangeCallback parameterChange,
                                       ParameterGestureCallback gestureEnd);
    void setPointerGestureAdmissionCallback(
        PointerGestureAdmissionCallback callback);
    void setUserPositionChangeCallback(UserPositionChangeCallback callback);
    void beginParameterGesture();
    void endParameterGesture();
    void setValueAsPartOfGesture (double newValue,
                                  juce::NotificationType notification);
    bool advanceAnimation(float deltaSeconds) noexcept;
    void dismissPrimaryPointerGesture();
    void dismissTransientInteraction();
    float getHoverAnimation() const noexcept { return hoverAnimation.current; }
    float getPressAnimation() const noexcept { return pressAnimation.current; }
    bool hasVisibleKeyboardFocus() const noexcept
    {
        return shouldShowKeyboardFocus();
    }
    bool keyPressed(const juce::KeyPress& key) override;

private:
    friend class FreqDividerGroup;
#if defined(RUN_PAMPLEJUCE_TESTS) && RUN_PAMPLEJUCE_TESTS
    friend struct VerticalLinePointerTestAccess;
#endif

    bool isEntered = false;
    bool isPointerSource(const juce::MouseEvent& event) const noexcept;
    bool canAcceptKeyboardOrAccessibilityInput() const noexcept;
    bool shouldShowKeyboardFocus() const noexcept;
    bool setValueFromUserInput(double newValue);
    std::uint64_t getParameterGestureGeneration() const noexcept;
    bool isParameterGestureActiveForGeneration(
        std::uint64_t expectedGeneration) const noexcept;
    void updateAnimationTargets() noexcept;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;
    void enablementChanged() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    std::unique_ptr<juce::AccessibilityHandler>
    createAccessibilityHandler() override;

    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    bool mDeleteState = false;
    float xPercent = 0.0f;
    int leftIndex = -1; // left index
    int rightIndex = -1; // right index
    int index = -1;
    ParameterGestureCallback parameterGestureBegin;
    ParameterChangeCallback parameterChange;
    ParameterGestureCallback parameterGestureEnd;
    PointerGestureAdmissionCallback pointerGestureAdmission;
    UserPositionChangeCallback userPositionChange;
    int parameterGestureDepth = 0;
    std::uint64_t parameterGestureGeneration = 0;
    bool primaryDragActive = false;
    bool keyboardFocusVisible = false;
    juce::MouseInputSource::InputSourceType pointerSourceType =
        juce::MouseInputSource::mouse;
    int pointerSourceIndex = -1;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerticalLine)
};
