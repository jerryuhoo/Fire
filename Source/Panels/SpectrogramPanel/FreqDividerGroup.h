/*
  ==============================================================================

    FreqDividerGroup.h
    Created: 4 Sep 2021 1:14:37pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FreqTextLabel.h"
#include "CloseButton.h"
#include "VerticalLine.h"
#include "SpectrumComponent.h"
#include "../../PluginProcessor.h"
//==============================================================================
/*
*/
class FreqDividerGroup  : public juce::Component, /*juce::Slider,*/ juce::Slider::Listener, juce::Button::Listener
{
public:
    FreqDividerGroup(FireAudioProcessor &, int index);
    ~FreqDividerGroup() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    VerticalLine verticalLine;
    
    int getFrequency();
    void setFrequency(int frequency);
    
    void moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[], int sortedIndex[]);
    bool getChangeState();
    void setChangeState(bool changeState);
    
//    bool getState();
//    void setState(bool state);
    
    bool getDeleteState();
    void setDeleteState(bool deleteState);
    
    juce::ToggleButton& getCloseButton();
    void setCloseButtonValue(bool value);
  
private:
    FireAudioProcessor &processor;
    float margin = 7.5f;
    float size = 15.0f;
    float width = 5.0f;
    juce::String lineStatelId = "";
    
    void sliderValueChanged(juce::Slider *slider) override;
    void buttonClicked (juce::Button* button) override;
    void updateCloseButtonState();
    CloseButton closeButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> closeButtonAttachment;
    
    FreqTextLabel freqTextLabel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqDividerGroup)
};
