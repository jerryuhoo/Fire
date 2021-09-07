/*
  ==============================================================================

    VerticalLine.h
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../GUI/LookAndFeel.h"

//==============================================================================
/*
*/
class VerticalLine  : public juce::Component
{
public:
    VerticalLine();
    ~VerticalLine() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    bool isMoving();
    void setMoving(bool move);
    bool getState();
    void setState(bool state);
    void setXPercent(float x);
    float getXPercent();
    void setIndex(int index);
    int getIndex();
    void setLeft(int leftIndex);
    int getLeft();
    void setRight(int rightIndex);
    int getRight();
    void moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[], int sortedIndex[]);
    
private:
    bool isEntered = false;
    
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    
    bool move = false;
    bool state = false;
    
    float xPercent = 0.f;
    int leftIndex = -1; // left index
    int rightIndex = -1; // right index
    int index = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerticalLine)
};
