/*
  ==============================================================================

    FreqTextLabel.h
    Created: 2 Dec 2020 7:53:08pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../../GUI/FireTheme.h"
#include "../../GUI/PrimaryEditableLabel.h"
#include "VerticalLine.h"
#include "SpectrumComponent.h"
#include <functional>
//==============================================================================
/*
*/
class FreqTextLabel : public juce::Component
{
public:
    using FrequencyEditCallback = std::function<void(float)>;

    FreqTextLabel (VerticalLine& v);
    ~FreqTextLabel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void lookAndFeelChanged() override;
    void setFreq (int freq);
    int getFreq() const noexcept;
    void setScale (float scale);
    bool isMouseOverCustom() const;
    bool advanceAnimation(float deltaSeconds);
    void setFade (bool update, bool isFadeIn);
    void dismissImmediately();
    void setFrequencyEditCallback(FrequencyEditCallback callback);

private:
    void applyEditedText();
    void finishEditorGesture();
    void updateLabelText();

    VerticalLine& verticalLine;
    int mFrequency = -1;
    float mScale = 1.0f;
    fire::ui::DampedValue revealAnimation;
    fire::ui::DampedValue hoverAnimation;
    bool editorGestureOpen = false;

    PrimaryEditableLabel freqLabel;
    FrequencyEditCallback frequencyEditCallback;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqTextLabel)
};
