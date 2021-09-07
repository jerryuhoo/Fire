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

//==============================================================================
/*
*/

class Multiband  : public juce::Component
{
public:
    Multiband();
    ~Multiband() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    int getLineNum();
    
    void getFocusArray(bool (&input)[4]);
    
    void getFreqArray(int (&input)[3]);
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
    void updateLines(juce::String option, int changedIndex);
    void setCloseButtonState();
    void setFocus();
    static void lineDeleted();
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
private:
    inline static bool isDeleted = false;
    float margin;
    float size = 15.0f;
    float width = 5.0f;
    float limitLeft;
    float limitRight;
    bool isAdded = false;
    bool isMoving = false;
    // bool isEnableChanged = false;
//    // Spectrum
//    SpectrumComponent spectrum;
    
    // multi-band
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    int lineNum = 0;
    int lastLineNum = 0;
    
    void changeFocus(int changedIndex, juce::String option);
    void changeEnable(int changedIndex, juce::String option);
    
    void setLineRelatedBoundsByX(int i);
    //void setLineRelatedBoundsByFreq(int i);
    void setSoloRelatedBounds();

    int sortedIndex[3] = { -1, -1, -1 };
//    int frequency[3] = { 0, 0, 0 };
    
    std::unique_ptr<FreqDividerGroup> freqDividerGroup[3];
    std::unique_ptr<SoloButton> soloButton[4];
    std::unique_ptr<EnableButton> enableButton[4];
    
    bool multibandFocus[4] = { true, false, false, false };
    //bool lineState[3] = { false, false, false };
    bool lastLineState[3] = { false, false, false };
    bool enableState[4] = { true, true, true, true };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Multiband)
};
