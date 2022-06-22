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
class FreqDividerGroup  : public juce::Component, juce::Slider::Listener, juce::Button::Listener
{
public:
    FreqDividerGroup(FireAudioProcessor &, int index);
    ~FreqDividerGroup() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<FreqDividerGroup> freqDividerGroup[], int sortedIndex[]);
    
    bool getDeleteState();
    void setDeleteState(bool deleteState);
    
    CloseButton& getCloseButton();
    void setCloseButtonValue(bool value);
    VerticalLine& getVerticalLine();

    void setFreq(float f);
    void setScale(float scale);
private:
    FireAudioProcessor &processor;
    VerticalLine verticalLine;
    float margin = 7.5f;
    float size = 15.0f;
    float width = 5.0f;
    juce::String lineStatelId = "";
    juce::String sliderFreqId = "";
    
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    
    void mouseDoubleClick(const juce::MouseEvent &e) override;
    void sliderValueChanged(juce::Slider *slider) override;
    void buttonClicked (juce::Button* button) override;
    void updateCloseButtonState();
    CloseButton closeButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> closeButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> multiFreqAttachment;
    
    FreqTextLabel freqTextLabel;
    OtherLookAndFeel otherLookAndFeel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreqDividerGroup)
};
