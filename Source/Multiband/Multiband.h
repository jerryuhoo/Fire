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
#include "VerticalLine.h"
#include "CloseButton.h"
#include "FreqTextLabel.h"
#include "SoloButton.h"
#include "EnableButton.h"
#include <vector>

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
    void getStateArray(bool (&input)[4]);
    void getFreqArray(int (&input)[3]);
    void reset();
    
private:
    
//    // Spectrum
//    SpectrumComponent spectrum;
    
    // multi-band
    void mouseUp(const juce::MouseEvent &e) override;
    int lineNum = 0;
    int lastLineNum = 0;
    int getChangedIndex();
    void changeFocus(int changedIndex, bool isAdd);
    void updateLines(float margin, float size, float width, bool isAdd, int changedIndex);
    
    int sortedIndex[3] = {-1, -1, -1};
    int frequency[3] = {0, 0, 0};
    
    std::unique_ptr<VerticalLine> verticalLines[3];
    std::unique_ptr<CloseButton> closeButtons[3];
    std::unique_ptr<FreqTextLabel> freqTextLabel[3];
    std::unique_ptr<SoloButton> soloButton[4];
    std::unique_ptr<EnableButton> enableButton[4];
    
    bool multibandFocus[4] = {true, false, false, false};
    bool lastLineState[3] = {false, false, false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Multiband)
};