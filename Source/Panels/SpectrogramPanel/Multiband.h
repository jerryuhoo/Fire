/*
  ==============================================================================

    Multiband.h
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SpectrumComponent.h"
#include "SoloButton.h"
#include "EnableButton.h"
#include <vector>
#include "FreqDividerGroup.h"
#include "../../PluginProcessor.h"
//==============================================================================
/*
*/

class Multiband  : public juce::Component, juce::Timer, juce::Slider::Listener, juce::AudioProcessorParameter::Listener, juce::Button::Listener
{
public:
    Multiband(FireAudioProcessor &);
    ~Multiband() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
    int getLineNum();
    
    void getFocusArray(bool (&input)[4]);
    
//    void getFreqArray(int (&input)[3]);
    void setFrequency(int freq1, int freq2, int freq3);
    void setFocus(bool focus1, bool focus2, bool focus3, bool focus4);
    void setLineState(bool state1, bool state2, bool state3);
    void getLineState(bool (&input)[3]);
    void setEnableState(bool state1, bool state2, bool state3, bool state4);
    void getEnableArray(bool(&input)[4]);

    void reset();
    //void setLineNum(int lineNum);
    void setLinePos(float pos1, float pos2, float pos3);
    void getLinePos(float (&input)[3]);
    
    void setCloseButtonState();
    void setFocus();
    bool getAddState();
    void setAddState(bool state);
    bool getMovingState();
    void setMovingState(bool state);
    bool getDeleteState();
    void setDeleteState(bool state);
    int getChangedIndex();
    int getSortedIndex(int index);
    void dragLines(float xPercent);
    void dragLinesByFreq(int freq, int index);
    int getFocusBand();
    
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { }
private:
    FireAudioProcessor &processor;
    float margin;
    float size = 15.0f;
    float width = 5.0f;
    float limitLeft;
    float limitRight;
    bool isAdded = false;
    bool isMoving = false;
    
    // multi-band
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    int lineNum = 0;
    
    void changeFocus(int changedIndex, juce::String option);
    void changeEnable(int changedIndex, juce::String option);
    
    void updateLineLeftRightIndex();
    void updateLineNumAndSortedIndex();
    void setLineRelatedBoundsByX(int i);
    //void setLineRelatedBoundsByFreq(int i);
    void setSoloRelatedBounds();
    void sliderValueChanged(juce::Slider *slider) override;
    void buttonClicked (juce::Button* button) override;
//    void updateFreqArray();
    int sortedIndex[3] = { -1, -1, -1 }; // input pos output line index
    
    std::unique_ptr<FreqDividerGroup> freqDividerGroup[3];
    std::unique_ptr<SoloButton> soloButton[4];
    std::unique_ptr<EnableButton> enableButton[4];
    
    bool multibandFocus[4] = { true, false, false, false };
    bool lastLineState[3] = { false, false, false };
    //bool enableState[4] = { true, true, true, true };
    
    
    // new added
    
//    juce::Slider multiFocusSlider1, multiFocusSlider2, multiFocusSlider3, multiFocusSlider4;
//    juce::Slider multiFreqSlider1, multiFreqSlider2, multiFreqSlider3;
    
//    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFocusAttachment1, multiFocusAttachment2, multiFocusAttachment3, multiFocusAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiEnableAttachment1, multiEnableAttachment2, multiEnableAttachment3, multiEnableAttachment4;
//    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFreqAttachment1, multiFreqAttachment2, multiFreqAttachment3;

    int multibandFreq[3] = { 0, 0, 0 };
    
    juce::Atomic<bool> parametersChanged {false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Multiband)
};
