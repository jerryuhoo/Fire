/*
  ==============================================================================

    Multiband.cpp
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Multiband.h"

//==============================================================================
Multiband::Multiband()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
//    addAndMakeVisible(spectrum);
    
    limitLeft = 0.1f;
    limitRight = 1.0f - limitLeft;
    
    soloButton[0] = std::make_unique<SoloButton>();
    addAndMakeVisible(*soloButton[0]);
    
    enableButton[0] = std::make_unique<EnableButton>();
    addAndMakeVisible(*enableButton[0]);
    
    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup>();
        addAndMakeVisible(*freqDividerGroup[i]);

        soloButton[i + 1] = std::make_unique<SoloButton>();
        addAndMakeVisible(*soloButton[i + 1]);
        
        enableButton[i + 1] = std::make_unique<EnableButton>();
        addAndMakeVisible(*enableButton[i + 1]);
    }
    
//    margin = getHeight() / 20.0f;
//    size = freqDividerGroup[0]->verticalLine.getHeight() / 10.f;
//    width = freqDividerGroup[0]->verticalLine.getWidth() / 2.f;
//    updateLines("do nothing", -1);
//    spectrum.setInterceptsMouseClicks(false, false);

}

Multiband::~Multiband()
{
}

void Multiband::paint (juce::Graphics& g)
{
    if (isVisible())
    {
//        spectrum.prepareToPaintSpectrum(processeor.getFFTSize(), processor.getFFTData());
        
        // draw line that will be added next
        g.setColour(COLOUR1.withAlpha(0.2f));
        float startY = 0;
        float endY = getHeight();
        auto mousePos = getMouseXYRelative();
        float xPos = mousePos.getX();
        float yPos = mousePos.getY();
        
        if (yPos >= startY && yPos <= startY + getHeight() / 5 && lineNum < 3)
        {   
            bool canCreate = true;
            float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
            for (int i = 0; i < 3; i++)
            {
                if ((freqDividerGroup[i]->verticalLine.getState() == true && fabs(freqDividerGroup[i]->verticalLine.getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
                {
                    canCreate = false;
                    break;
                }
            }
            if (canCreate)
            {
                g.drawLine(xPos, startY, xPos, endY, 2);
            }
        }
 
       
        
//        if (isDeleted)
//        {
//            int changedIndex = getChangedIndex();
//            isDeleted = false;
//            updateLines("delete", changedIndex);
//        }

        // set black and focus masks
        int margin2 = getWidth() / 250; // 1/100 * 4 / 10
        int margin1 = getWidth() * 6 / 1000; // 1/100 * 6 / 10
        
        // delete line and reset deleted disabled band state
        for (int i = 0; i < 4; ++i)
        {
            if (!enableButton[i]->getState() && i > lineNum)
            {
                enableButton[i]->setState(true);
            }
        }
        
        if (lineNum > 0 && multibandFocus[0])
        {
//            g.setColour(COLOUR_MASK_RED);
            juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                      COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
            g.setGradientFill(grad);
            g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
        }
        
        if (lineNum > 0 && enableButton[0]->getState() == false)
        {
            g.setColour(COLOUR_MASK_BLACK);
            g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
        }
        
        for (int i = 1; i < lineNum; i++)
        {
            if (lineNum > 1 && multibandFocus[i])
            {
//                g.setColour(COLOUR_MASK_RED);
                juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                          COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
                g.setGradientFill(grad);
                g.fillRect(freqDividerGroup[sortedIndex[i - 1]]->getX() + margin1, 0, freqDividerGroup[sortedIndex[i]]->getX() - freqDividerGroup[sortedIndex[i - 1]]->getX(), getHeight());
            }
            if (lineNum > 1 && enableButton[i]->getState() == false)
            {
                g.setColour(COLOUR_MASK_BLACK);
                g.fillRect(freqDividerGroup[sortedIndex[i - 1]]->getX() + margin1, 0, freqDividerGroup[sortedIndex[i]]->getX() - freqDividerGroup[sortedIndex[i - 1]]->getX(), getHeight());
            }
        }

        if (lineNum > 0 && multibandFocus[lineNum])
        {
//            g.setColour(COLOUR_MASK_RED);
            juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                      COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
            g.setGradientFill(grad);
            g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
        }
        if (lineNum > 0 && enableButton[lineNum]->getState() == false)
        {
            g.setColour(COLOUR_MASK_BLACK);
            g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
        }
        
        float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
        dragLines(targetXPercent);
    }
}

void Multiband::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    margin = getHeight() / 20.0f;
    size = freqDividerGroup[0]->verticalLine.getHeight() / 10.0f;
    width = freqDividerGroup[0]->verticalLine.getWidth() / 2.0f;
    
    // temp method to fix
    if (size == 0)
    {
        size = 15.0f;
    }
    if (width == 0)
    {
        width = 5.0f;
    }
    
    updateLines("resize", -1);
}

int Multiband::getChangedIndex()
{
    // return changed line sorted index
    int idx = -1;
    for (int i = 0; i < 3; i++)
    {
//        if (sortedIndex[i] == -1)
//        {
//            break;
//        }
        if (lastLineState[i] != freqDividerGroup[i]->verticalLine.getState())
        {
            lastLineState[i] = freqDividerGroup[i]->verticalLine.getState();
            idx = i;
            break;
        }
    }
    return idx;
}

void Multiband::changeFocus(int changedIndex, juce::String option)
{
    // get current focus index
    int focusIndex = 0;
    for (int i = 0; i < 4; i++)
    {
        if (multibandFocus[i])
        {
            focusIndex = i;
            break;
        }
    }
    
    // get changed index -> position index
    int newLinePosIndex = 0; // (count from left to right in the window)
    newLinePosIndex = freqDividerGroup[changedIndex]->verticalLine.getIndex();
    
    if (option == "add")
    {
        // change focus index when line is added
        if (newLinePosIndex < focusIndex || (newLinePosIndex == focusIndex && getMouseXYRelative().getX() < enableButton[focusIndex]->getX()))
        {
            multibandFocus[focusIndex] = false;
            multibandFocus[focusIndex + 1] = true;
        }
    }
    else if (option == "delete")
    {
        // change focus index when line is deleted
        if (newLinePosIndex < focusIndex)
        {
            multibandFocus[focusIndex] = false;
            multibandFocus[focusIndex - 1] = true;
        }
    }
}

void Multiband::changeEnable(int changedIndex, juce::String option)
{
    // get changed index -> position index
    int newLinePosIndex = 0; // (count from left to right in the window)
    newLinePosIndex = freqDividerGroup[changedIndex]->verticalLine.getIndex();
    
    if (option == "add")
    {
        // change enable index when line is added
        for (int i = lineNum - 1; i >= 0; --i)
        {
            // new line is added on the left side of disabled button
            if ((!enableButton[i]->getState() && i > newLinePosIndex) ||
                (!enableButton[i]->getState() && getMouseXYRelative().getX() < enableButton[i]->getX()))
            {
                enableState[i] = true;
                enableButton[i]->setState(true);
//                if (i < lineNum)
//                {
                    enableState[i + 1] = false;
                    enableButton[i + 1]->setState(false);
//                }
            }
        }
    }
    else if (option == "delete")
    {
        // change enable index when line is deleted
        for (int i = 1; i <= lineNum + 1; ++i) // line is already deleted so +1
        {
            // for safety
            if (i > 3)
            {
                break;
            }
            
            // new line is deleted on the left side of disabled button
            if (!enableButton[i]->getState() && i > newLinePosIndex)
            {
                enableState[i] = true;
                enableButton[i]->setState(true);
                
                enableState[i - 1] = false;
                enableButton[i - 1]->setState(false);
            }
        }
        
        // special case: delete first line and only line number is 1 (after deleting it is 0), then delete enable[0] state
        if (newLinePosIndex == 0 && lineNum == 0)
        {
            enableState[0] = true;
            enableButton[0]->setState(true);
        }
    }
}

void Multiband::updateLines(juce::String option, int changedIndex)
{
    int count = 0;
    
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->verticalLine.getState())
        {
            freqDividerGroup[i]->verticalLine.setVisible(true);
            setLineRelatedBoundsByX(i);
            sortedIndex[count] = i;
            count++;
        }
    }
    
    lineNum = count;

    // sort
    for(int j = 1; j < lineNum; j++)
    {
        for(int k = 0; k < lineNum - j; k++)
        {
            if(freqDividerGroup[sortedIndex[k]]->verticalLine.getXPercent() > freqDividerGroup[sortedIndex[k + 1]]->verticalLine.getXPercent())
            {
                std::swap(sortedIndex[k], sortedIndex[k + 1]);
            }
        }
    }

    // clear sort index when vertical line state is false
    for(int i = lineNum; i < 3; i++)
    {
        sortedIndex[i] = -1;
    }

    // should set self index first, then set left and right index
    for (int i = 0; i < lineNum; i++)
    {
        freqDividerGroup[sortedIndex[i]]->verticalLine.setIndex(i); // this index is the No. you count the line from left to right
        //[delete] frequency[i] = freqDividerGroup[sortedIndex[i]]->getFrequency();
    }
    
    //[delete]reset other frequency to 0
//    for (int i = lineNum; i < 3; i++)
//    {
//        frequency[i] = 0;
//    }
    
    // set left right index
    for (int i = 0; i < lineNum; i++)
    {
        if (i == 0)
        {
            freqDividerGroup[sortedIndex[i]]->verticalLine.setLeft(-1); // this left index is the index that in verticalLines array
        }
        else
        {
            freqDividerGroup[sortedIndex[i]]->verticalLine.setLeft(freqDividerGroup[sortedIndex[i - 1]]->verticalLine.getIndex());
        }
        if (i == lineNum - 1)
        {
            freqDividerGroup[sortedIndex[i]]->verticalLine.setRight(lineNum);
        }
        else
        {
            freqDividerGroup[sortedIndex[i]]->verticalLine.setRight(freqDividerGroup[sortedIndex[i + 1]]->verticalLine.getIndex());
        }
    }
    
//    bool isMoving = false;
//    for (int i = 0; i < lineNum; i++)
//    {
//        if (freqDividerGroup[sortedIndex[i]]->verticalLine.isMoving())
//        {
//            isMoving = true;
//            break;
//        }
//    }

    if (changedIndex != -1) // if not resizing
    {
        // change focus position
        changeFocus(changedIndex, option);
        changeEnable(changedIndex, option);
    }
    
    // set soloButtons visibility
    for (int i = 0; i < 4; i++)
    {
        if (i <= lineNum && lineNum != 0)
        {
            soloButton[i]->setVisible(true);
            enableButton[i]->setVisible(true);
        }
        else
        {
            soloButton[i]->setVisible(false);
            enableButton[i]->setVisible(false);
        }
    }
    
    setSoloRelatedBounds();
//    // setBounds of soloButtons and enableButtons [duplicated]
//    if (lineNum >= 1)
//    {
//        enableButton[0]->setBounds(freqDividerGroup[sortedIndex[0]]->verticalLine.getX() / 2, margin, size, size);
//        for (int i = 1; i < lineNum; i++)
//        {
//            enableButton[i]->setBounds((freqDividerGroup[sortedIndex[i]]->verticalLine.getX() + freqDividerGroup[sortedIndex[i - 1]]->verticalLine.getX()) / 2, margin, size, size);
//        }
//        enableButton[lineNum]->setBounds((freqDividerGroup[sortedIndex[lineNum - 1]]->verticalLine.getX() + getWidth()) / 2, margin, size, size);
//    }
}

void Multiband::mouseUp(const juce::MouseEvent &e)
{
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->verticalLine.setMoving(false);
    }
}

void Multiband::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isLeftButtonDown() && e.y <= getHeight() / 5.0f) // create new lines
    {
        if (lineNum < 3)
        {
            bool canCreate = true;
            float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
            
            for (int i = 0; i < 3; i++)
            {
                // can't create near existed lines
                if ((freqDividerGroup[i]->verticalLine.getState() == true && fabs(freqDividerGroup[i]->verticalLine.getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
                {
                    canCreate = false;
                    break;
                }
            }
            if (canCreate)
            {
                /*int focusIndex = 0;
                for (int i = 0; i <= lineNum; i++)
                {
                    if(multibandFocus[i] == true)
                    {
                        focusIndex = i;
                        break;
                    }
                }*/
                for (int i = 0; i < 3; i++)
                {
                    // create lines and close buttons and then set state
                    if (freqDividerGroup[i]->verticalLine.getState() == false)
                    {
                        freqDividerGroup[i]->verticalLine.setState(true);
                        freqDividerGroup[i]->verticalLine.setXPercent(xPercent);
                        freqDividerGroup[i]->verticalLine.setMoving(true);
                        //closeButtons[i]->setVisible(true);
                        //freqTextLabel[i]->setVisible(true);
                        break;
                    }
                }

                int changedIndex = getChangedIndex();
                if (changedIndex != -1)
                {
                    updateLines("add", changedIndex);
                    isAdded = true;
                }
            }
        }
    }
    else if (e.mods.isLeftButtonDown() && e.y > getHeight() / 5.0f) // focus on one band
    {
        int num = lineNum;
        
        for (int i = 0; i < 4; i++)
        {
            multibandFocus[i] = false;
        }
        
        if (lineNum == 0)
        {
            multibandFocus[0] = true;
            return;
        }
        else
        {
            if (e.x >= 0 && e.x < freqDividerGroup[sortedIndex[0]]->getX())
            {
                multibandFocus[0] = true;
                return;
            }
            
            for (int i = 1; i < num; i++)
            {
                if (e.x >= freqDividerGroup[sortedIndex[i - 1]]->getX() && e.x < freqDividerGroup[sortedIndex[i]]->getX())
                {
                    multibandFocus[i] = true;
                    return;
                }
            }
            
            if (e.x >= freqDividerGroup[sortedIndex[num - 1]]->getX() && e.x <= getWidth())
            {
                multibandFocus[num] = true;
                return;
            }
        }
    }
}

int Multiband::getLineNum()
{
    return lineNum;
}

void Multiband::getFocusArray(bool (&input)[4])
{
    for (int i = 0; i < 4; i++)
    {
        input[i] = multibandFocus[i];
    }
}

void Multiband::getEnableArray(bool (&input)[4])
{
    for (int i = 0; i < 4; i++)
    {
        input[i] = enableButton[i]->getState();
    }
}

void Multiband::getFreqArray(int (&input)[3])
{
    for (int i = 0; i < lineNum; i++)
    {
        input[i] = freqDividerGroup[sortedIndex[i]]->getFrequency();
    }
    for (int i = lineNum; i < 3; i++)
    {
        input[i] = 0;
    }
}

void Multiband::reset()
{
    for (int i = 0; i < 4; i++)
    {
        enableButton[i]->setVisible(false);
        soloButton[i]->setVisible(false);
    }
    multibandFocus[0] = true;
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->verticalLine.setState(false);
//        closeButtons[i]->setVisible(false);
        multibandFocus[i + 1] = false;
        sortedIndex[i] = 0;
        //[delete] frequency[i] = 0;
        lastLineState[i] = freqDividerGroup[i]->verticalLine.getState();
    }
    lineNum = 0;
}

// 3 ways to change frequency: 1. change freqLabel; 2. drag; 3.change Ableton slider
void Multiband::dragLines(float xPercent)
{
    // drag
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->verticalLine.isMoving())
        {
            freqDividerGroup[i]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup, sortedIndex);
            isMoving = true;
        }
    }

    if (isMoving) // drag
    {
        for (int i = 0; i < lineNum; i++)
        {
            if (freqDividerGroup[sortedIndex[i]]->verticalLine.getState())
            {
                /// see line 303 repeated?
                //setLineRelatedBoundsByX(sortedIndex[i]);
                //[delete]frequency[sortedIndex[i]] = freqDividerGroup[sortedIndex[i]]->getFrequency();
            }
        }
        setSoloRelatedBounds();
       
    }
    else // change freq text
    {
//        for (int i = 0; i < lineNum; i++)
//        {
//            if (freqDividerGroup[sortedIndex[i]]->verticalLine.getState())
//            {
//                setLineRelatedBoundsByFreq(sortedIndex[i]);
//                frequency[sortedIndex[i]] = freqDividerGroup[sortedIndex[i]]->getFrequency();
//
//            }
//        }
//        setSoloRelatedBounds();
        
        for (int i = 0; i < lineNum; i++)
        {
            if (freqDividerGroup[sortedIndex[i]]->getChangeState())
            {
                dragLinesByFreq(freqDividerGroup[sortedIndex[i]]->getFrequency(), sortedIndex[i]);
                freqDividerGroup[sortedIndex[i]]->setChangeState(false);
            }
        }
    }
}

// In Ableton, move the slider in the control bar, the lines should move.
void Multiband::dragLinesByFreq(int freq, int index)
{
    if (!isMoving && freqDividerGroup[index]->verticalLine.getState())
    {
        float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freq / (44100 / 2.0)));
        freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup, sortedIndex);
        
        // keep distance limit between lines
        for (int i = 0; i < lineNum; i++)
        {
            if (freqDividerGroup[sortedIndex[i]]->verticalLine.getState())
            {
                // get the correct xPercent if the line cannot move.
                xPercent = freqDividerGroup[sortedIndex[i]]->verticalLine.getXPercent();

                freqDividerGroup[sortedIndex[i]]->verticalLine.setXPercent(xPercent);
                freqDividerGroup[sortedIndex[i]]->setBounds(xPercent * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
                // get the correct freq if the line cannot move.
                //freq = freqDividerGroup[sortedIndex[i]]->getFrequency();
            }
        }
        
//        if (freqDividerGroup[index]->verticalLine.getState())
//        {
//            freqDividerGroup[index]->setFrequency(freq);
//            frequency[index] = freq;
//        }

        setSoloRelatedBounds();
    }
}

void Multiband::setLineRelatedBoundsByX(int i)
{
    freqDividerGroup[i]->setBounds(freqDividerGroup[i]->verticalLine.getXPercent() * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
    //closeButtons[i]->setBounds(freqDividerGroup[i]->verticalLine.getX() + width + margin, margin, size, size);

    //freqTextLabel[i]->setBounds(freqDividerGroup[i]->verticalLine.getX() + width + margin - width * 7, getHeight() / 5 + margin, width * 15, size);
    //freqTextLabel[i]->setScale(juce::jmin(width / 5.0f, size / 15.0f));
    int freq = static_cast<int>(SpectrumComponent::transformFromLog(freqDividerGroup[i]->verticalLine.getXPercent()) * (44100 / 2.0));
    freqDividerGroup[i]->setFrequency(freq);
}

//void Multiband::setLineRelatedBoundsByFreq(int i)
//{
//    int freq = freqDividerGroup[i]->getFrequency();
//    float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freq / (44100 / 2.0)));
//    freqDividerGroup[i]->setBounds(xPercent * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
//}

void Multiband::setSoloRelatedBounds()
{
    // setBounds of soloButtons and enableButtons
    if (lineNum >= 1)
    {
        enableButton[0]->setBounds(freqDividerGroup[sortedIndex[0]]->getX() / 2, margin, size, size);
        for (int i = 1; i < lineNum; i++)
        {
            enableButton[i]->setBounds((freqDividerGroup[sortedIndex[i]]->getX() + freqDividerGroup[sortedIndex[i - 1]]->getX()) / 2, margin, size, size);
        }
        enableButton[lineNum]->setBounds((freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + getWidth()) / 2, margin, size, size);
    }
}

//void Multiband::setLineNum(int lineNum)
//{
//    this->lineNum = lineNum;
//}

void Multiband::setFocus(bool focus1, bool focus2, bool focus3, bool focus4)
{
    multibandFocus[0] = focus1;
    multibandFocus[1] = focus2;
    multibandFocus[2] = focus3;
    multibandFocus[3] = focus4;
}

void Multiband::setLineState(bool state1, bool state2, bool state3)
{
//    lineState[0] = state1;
//    lineState[1] = state2;
//    lineState[2] = state3;
    freqDividerGroup[0]->verticalLine.setState(state1);
    freqDividerGroup[1]->verticalLine.setState(state2);
    freqDividerGroup[2]->verticalLine.setState(state3);
    lastLineState[0] = state1;
    lastLineState[1] = state2;
    lastLineState[2] = state3;
}

void Multiband::getLineState(bool (&input)[3])
{
    for (int i = 0; i < 3; i++)
    {
        if (sortedIndex[i] < 0)
        {
            input[i] = false;
        }
        else
        {
            input[i] = freqDividerGroup[sortedIndex[i]]->verticalLine.getState();
        }
    }
}

void Multiband::setLinePos(float pos1, float pos2, float pos3)
{
    freqDividerGroup[0]->verticalLine.setXPercent(pos1);
    freqDividerGroup[1]->verticalLine.setXPercent(pos2);
    freqDividerGroup[2]->verticalLine.setXPercent(pos3);
}

void Multiband::getLinePos(float (&input)[3])
{
    for (int i = 0; i < 3; i++)
    {
        input[i] = freqDividerGroup[i]->verticalLine.getXPercent();
    }
}

void Multiband::setFrequency(int freq1, int freq2, int freq3)
{
    //[delete]
//    frequency[0] = freq1;
//    frequency[1] = freq2;
//    frequency[2] = freq3;
    freqDividerGroup[0]->setFrequency(freq1);
    freqDividerGroup[1]->setFrequency(freq2);
    freqDividerGroup[2]->setFrequency(freq3);
}



void Multiband::setEnableState(bool state1, bool state2, bool state3, bool state4)
{
    enableState[0] = state1;
    enableState[1] = state2;
    enableState[2] = state3;
    enableState[3] = state4;
    enableButton[0]->setState(state1);
    enableButton[1]->setState(state2);
    enableButton[2]->setState(state3);
    enableButton[3]->setState(state4);
}

void Multiband::setCloseButtonState()
{
//    for (int i = 0; i < 3; i++)
//    {
//        // create lines and close buttons and then set state
//        if (freqDividerGroup[i]->verticalLine.getState())
//        {
//            closeButtons[i]->setVisible(true);
//        }
//        else
//        {
//            closeButtons[i]->setVisible(false);
//        }
//    }
}

void Multiband::setFocus()
{
    multibandFocus[0] = true;
    for (int i = 1; i < 4; i++)
    {
        multibandFocus[i] = false;
    }
}

void Multiband::lineDeleted()
{
    isDeleted = true;
}

bool Multiband::getAddState()
{
    return isAdded;
}

void Multiband::setAddState(bool state)
{
    isAdded = state;
}

bool Multiband::getMovingState()
{
    return isMoving;
}

void Multiband::setMovingState(bool state)
{
    isMoving = state;
}

bool Multiband::getDeleteState()
{
    return isDeleted;
}

void Multiband::setDeleteState(bool state)
{
    isDeleted = state;
}

int Multiband::getSortedIndex(int index)
{
    return sortedIndex[index];
}
