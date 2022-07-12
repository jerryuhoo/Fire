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
#include "../TopPanel/Preset.h"
//==============================================================================
/*
*/

class Multiband  : public juce::Component, juce::Timer, juce::Slider::Listener, juce::AudioProcessorParameter::Listener, juce::Button::Listener
{
public:
    Multiband(FireAudioProcessor &, state::StateComponent &);
    ~Multiband() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
    int getLineNum();
    void getFocusArray(bool (&input)[4]);
    void setFrequency(int freq1, int freq2, int freq3);
    void setFocus(bool focus1, bool focus2, bool focus3, bool focus4);
    void setLineState(bool state1, bool state2, bool state3);
    void getLineState(bool (&input)[3]);
    void setEnableState(bool state1, bool state2, bool state3, bool state4);
    void getEnableArray(bool(&input)[4]);

    void reset();
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
    int getSortedIndex(int index);
    void dragLines(float xPercent);
    void dragLinesByFreq(int freq, int index);
    int getFocusBand();
    void updateLines(int option);
    void setSoloRelatedBounds();
    EnableButton& getEnableButton(int index);
    
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { }
    void setScale(float scale);
    void setBandBypassStates(int index, bool state);
    
private:
    FireAudioProcessor &processor;
    state::StateComponent &stateComponent;
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
    int changePresetLineCount = 0; // only for preset change count
    
    
    std::vector<juce::String> paramsArray1 = {MODE_NAME1, LINKED_NAME1, SAFE_NAME1, DRIVE_NAME1, COMP_RATIO_NAME1, COMP_THRESH_NAME1, WIDTH_NAME1, OUTPUT_NAME1, MIX_NAME1, BIAS_NAME1, REC_NAME1, COMP_BYPASS_NAME1, WIDTH_BYPASS_NAME1};
    std::vector<juce::String> paramsArray2 = {MODE_NAME2, LINKED_NAME2, SAFE_NAME2, DRIVE_NAME2, COMP_RATIO_NAME2, COMP_THRESH_NAME2, WIDTH_NAME2, OUTPUT_NAME2, MIX_NAME2, BIAS_NAME2, REC_NAME2, COMP_BYPASS_NAME2, WIDTH_BYPASS_NAME2};
    std::vector<juce::String> paramsArray3 = {MODE_NAME3, LINKED_NAME3, SAFE_NAME3, DRIVE_NAME3, COMP_RATIO_NAME3, COMP_THRESH_NAME3, WIDTH_NAME3, OUTPUT_NAME3, MIX_NAME3, BIAS_NAME3, REC_NAME3, COMP_BYPASS_NAME3, WIDTH_BYPASS_NAME3};
    std::vector<juce::String> paramsArray4 = {MODE_NAME4, LINKED_NAME4, SAFE_NAME4, DRIVE_NAME4, COMP_RATIO_NAME4, COMP_THRESH_NAME4, WIDTH_NAME4, OUTPUT_NAME4, MIX_NAME4, BIAS_NAME4, REC_NAME4, COMP_BYPASS_NAME4, WIDTH_BYPASS_NAME4};
    bool isParamInArray(juce::String paramName, std::vector<juce::String> paramArray);
    void setParametersToAFromB(int toIndex, int fromIndex);
    void initParameters(int bandindex);
    void setStatesWhenAddOrDelete(int changedIndex, juce::String option);
    
    void updateLineLeftRightIndex();
    void updateLineNumAndSortedIndex(int option);
    void setLineRelatedBoundsByX(int i);
    void setLineRelatedBoundsByFreq(int i);
    
    void sliderValueChanged(juce::Slider *slider) override;
    void buttonClicked (juce::Button* button) override;

    bool shouldSetBlackMask(int index);
    
    int sortedIndex[3] = { -1, -1, -1 }; // input pos output line index
    
    std::unique_ptr<FreqDividerGroup> freqDividerGroup[3];
    std::unique_ptr<SoloButton> soloButton[4];
    std::unique_ptr<EnableButton> enableButton[4];
    
    bool multibandFocus[4] = { true, false, false, false };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiEnableAttachment1, multiEnableAttachment2, multiEnableAttachment3, multiEnableAttachment4;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> multiSoloAttachment1, multiSoloAttachment2, multiSoloAttachment3, multiSoloAttachment4;
    
    juce::Atomic<bool> parametersChanged {false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Multiband)
};
