/*
  ==============================================================================

    VerticalLine.h
    Created: 25 Oct 2020 7:54:46am
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

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
    bool getState();
    void setState(bool state);
    void setXPercent(float x);
    float getXPercent();
    void setIndex(int index);
    float getIndex();
    void setLeft(int leftIndex);
    float getLeft();
    void setRight(int rightIndex);
    float getRight();
    void moveToX(int lineNum, float newXPercent, float margin, std::unique_ptr<VerticalLine> verticalLines[]);
    
private:
    bool isEntered = false;
    
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    
    bool move = false;
    bool state = false;
    
    float xPercent = 0.f;
    int leftIndex; // left index
    int rightIndex; // right index
    int index;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerticalLine)
};
