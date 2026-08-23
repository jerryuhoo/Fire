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
#include <functional>

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
    void setParameterGestureCallbacks (ParameterGestureCallback gestureBegin,
                                       ParameterGestureCallback parameterChange,
                                       ParameterGestureCallback gestureEnd);
    void beginParameterGesture();
    void endParameterGesture();
    void setValueAsPartOfGesture (double newValue,
                                  juce::NotificationType notification);
    bool advanceAnimation(float deltaSeconds) noexcept;
    void dismissTransientInteraction();
    float getHoverAnimation() const noexcept { return hoverAnimation.current; }
    float getPressAnimation() const noexcept { return pressAnimation.current; }

private:
    bool isEntered = false;
    void updateAnimationTargets() noexcept;

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
    ParameterGestureCallback parameterChange;
    ParameterGestureCallback parameterGestureEnd;
    int parameterGestureDepth = 0;
    bool primaryDragActive = false;
    fire::ui::DampedValue hoverAnimation;
    fire::ui::DampedValue pressAnimation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerticalLine)
};
