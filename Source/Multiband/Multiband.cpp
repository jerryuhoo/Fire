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
        verticalLines[i] = std::make_unique<VerticalLine>();
        addAndMakeVisible(*verticalLines[i]);
        
        closeButtons[i] = std::make_unique<CloseButton>(*verticalLines[i]);
        addAndMakeVisible(*closeButtons[i]);
        
        freqTextLabel[i] = std::make_unique<FreqTextLabel>(*verticalLines[i]);
        addAndMakeVisible(*freqTextLabel[i]);
        freqTextLabel[i]->setVisible(false);
        
        soloButton[i + 1] = std::make_unique<SoloButton>();
        addAndMakeVisible(*soloButton[i + 1]);
        
        enableButton[i + 1] = std::make_unique<EnableButton>();
        addAndMakeVisible(*enableButton[i + 1]);
    }
    
    margin = getHeight() / 20.0f;
//    size = verticalLines[0]->getHeight() / 10.f;
//    width = verticalLines[0]->getWidth() / 2.f;
//    updateLines(false, -1);
//    spectrum.setInterceptsMouseClicks(false, false);
}

Multiband::~Multiband()
{
}

void Multiband::paint (juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */
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
                if ((verticalLines[i]->getState() == true && fabs(verticalLines[i]->getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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
 
        // freqLabel
        for (int i = 0; i < 3; i++)
        {
            if (verticalLines[i]->getState())
            {
                freqTextLabel[i]->setVisible(verticalLines[i]->isMoving() || verticalLines[i]->isMouseOver());
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
        
        if (lineNum > 0 && enableButton[0]->getState() == false)
        {
            g.setColour(COLOUR_MASK_BLACK);
            g.fillRect(0, 0, verticalLines[sortedIndex[0]]->getX() + margin2, getHeight());
        }
        if (lineNum > 0 && multibandFocus[0])
        {
            g.setColour(COLOUR_MASK_RED);
            g.fillRect(0, 0, verticalLines[sortedIndex[0]]->getX() + margin2, getHeight());
        }
        
        for (int i = 1; i < lineNum; i++)
        {
            if (lineNum > 1 && enableButton[i]->getState() == false)
            {
                g.setColour(COLOUR_MASK_BLACK);
                g.fillRect(verticalLines[sortedIndex[i - 1]]->getX() + margin1, 0, verticalLines[sortedIndex[i]]->getX() - verticalLines[sortedIndex[i - 1]]->getX(), getHeight());
            }
            if (lineNum > 1 && multibandFocus[i])
            {
                g.setColour(COLOUR_MASK_RED);
                g.fillRect(verticalLines[sortedIndex[i - 1]]->getX() + margin1, 0, verticalLines[sortedIndex[i]]->getX() - verticalLines[sortedIndex[i - 1]]->getX(), getHeight());
            }
        }
        if (lineNum > 0 && enableButton[lineNum]->getState() == false)
        {
            g.setColour(COLOUR_MASK_BLACK);
            g.fillRect(verticalLines[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - verticalLines[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
        }
        if (lineNum > 0 && multibandFocus[lineNum])
        {
            g.setColour(COLOUR_MASK_RED);
            g.fillRect(verticalLines[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - verticalLines[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
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
    size = verticalLines[0]->getHeight() / 10.0f;
    width = verticalLines[0]->getWidth() / 2.0f;
    
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
        if (lastLineState[i] != verticalLines[i]->getState())
        {
            lastLineState[i] = verticalLines[i]->getState();
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
    newLinePosIndex = verticalLines[changedIndex]->getIndex();
    
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
    newLinePosIndex = verticalLines[changedIndex]->getIndex();
    
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
        if (verticalLines[i]->getState())
        {
            verticalLines[i]->setVisible(true);
            
            setLineRelatedBounds(i);
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
            if(verticalLines[sortedIndex[k]]->getXPercent() > verticalLines[sortedIndex[k + 1]]->getXPercent())
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
        verticalLines[sortedIndex[i]]->setIndex(i); // this index is the No. you count the line from left to right
        frequency[i] = freqTextLabel[sortedIndex[i]]->getFreq();
    }
    
    // reset other frequency to 0
    for (int i = lineNum; i < 3; i++)
    {
        frequency[i] = 0;
    }
    
    // set left right index
    for (int i = 0; i < lineNum; i++)
    {
        if (i == 0)
        {
            verticalLines[sortedIndex[i]]->setLeft(-1); // this left index is the index that in verticalLines array
        }
        else
        {
            verticalLines[sortedIndex[i]]->setLeft(verticalLines[sortedIndex[i - 1]]->getIndex());
        }
        if (i == lineNum - 1)
        {
            verticalLines[sortedIndex[i]]->setRight(lineNum);
        }
        else
        {
            verticalLines[sortedIndex[i]]->setRight(verticalLines[sortedIndex[i + 1]]->getIndex());
        }
    }
    
//    bool isMoving = false;
//    for (int i = 0; i < lineNum; i++)
//    {
//        if (verticalLines[sortedIndex[i]]->isMoving())
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
//        enableButton[0]->setBounds(verticalLines[sortedIndex[0]]->getX() / 2, margin, size, size);
//        for (int i = 1; i < lineNum; i++)
//        {
//            enableButton[i]->setBounds((verticalLines[sortedIndex[i]]->getX() + verticalLines[sortedIndex[i - 1]]->getX()) / 2, margin, size, size);
//        }
//        enableButton[lineNum]->setBounds((verticalLines[sortedIndex[lineNum - 1]]->getX() + getWidth()) / 2, margin, size, size);
//    }
}

void Multiband::mouseUp(const juce::MouseEvent &e)
{
    for (int i = 0; i < 3; i++)
    {
        verticalLines[i]->setMoving(false);
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
                if ((verticalLines[i]->getState() == true && fabs(verticalLines[i]->getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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
                    if (verticalLines[i]->getState() == false)
                    {
                        verticalLines[i]->setState(true);
                        verticalLines[i]->setXPercent(xPercent);
                        verticalLines[i]->setMoving(true);
                        closeButtons[i]->setVisible(true);
                        freqTextLabel[i]->setVisible(true);
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
            if (e.x >= 0 && e.x < verticalLines[sortedIndex[0]]->getX())
            {
                multibandFocus[0] = true;
                return;
            }
            
            for (int i = 1; i < num; i++)
            {
                if (e.x >= verticalLines[sortedIndex[i - 1]]->getX() && e.x < verticalLines[sortedIndex[i]]->getX())
                {
                    multibandFocus[i] = true;
                    return;
                }
            }
            
            if (e.x >= verticalLines[sortedIndex[num - 1]]->getX() && e.x <= getWidth())
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
        input[i] = frequency[i];
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
        verticalLines[i]->setState(false);
        closeButtons[i]->setVisible(false);
        multibandFocus[i + 1] = false;
        sortedIndex[i] = 0;
        frequency[i] = 0;
        lastLineState[i] = verticalLines[i]->getState();
    }
    lineNum = 0;
}

void Multiband::dragLines(float xPercent)
{
    // drag
    for (int i = 0; i < 3; i++)
    {
        if (verticalLines[i]->isMoving())
        {
            verticalLines[i]->moveToX(lineNum, xPercent, limitLeft, verticalLines, sortedIndex);
            isMoving = true;
        }
    }
    
    if (isMoving)
    {
        for (int i = 0; i < 3; i++)
        {
            setLineRelatedBounds(i);
            frequency[i] = freqTextLabel[sortedIndex[i]]->getFreq();
        }
        setSoloRelatedBounds();
    }
}

void Multiband::setLineRelatedBounds(int i)
{
    verticalLines[i]->setBounds(verticalLines[i]->getXPercent() * getWidth() - getWidth() / 200, 0, getWidth() / 100, getHeight());
    closeButtons[i]->setBounds(verticalLines[i]->getX() + width + margin, margin, size, size);

    freqTextLabel[i]->setBounds(verticalLines[i]->getX() + width + margin, getHeight() / 5 + margin, width * 15, size);
    freqTextLabel[i]->setScale(juce::jmin(width / 5.0f, size / 15.0f));
    int freq = static_cast<int>(SpectrumComponent::transformFromLog(verticalLines[i]->getXPercent()) * (44100 / 2.0));
    freqTextLabel[i]->setFreq(freq);
}

void Multiband::setSoloRelatedBounds()
{
    // setBounds of soloButtons and enableButtons
    if (lineNum >= 1)
    {
        enableButton[0]->setBounds(verticalLines[sortedIndex[0]]->getX() / 2, margin, size, size);
        for (int i = 1; i < lineNum; i++)
        {
            enableButton[i]->setBounds((verticalLines[sortedIndex[i]]->getX() + verticalLines[sortedIndex[i - 1]]->getX()) / 2, margin, size, size);
        }
        enableButton[lineNum]->setBounds((verticalLines[sortedIndex[lineNum - 1]]->getX() + getWidth()) / 2, margin, size, size);
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
    lineState[0] = state1;
    lineState[1] = state2;
    lineState[2] = state3;
    verticalLines[0]->setState(state1);
    verticalLines[1]->setState(state2);
    verticalLines[2]->setState(state3);
    lastLineState[0] = state1;
    lastLineState[1] = state2;
    lastLineState[2] = state3;
}

void Multiband::setLinePos(float pos1, float pos2, float pos3)
{
    verticalLines[0]->setXPercent(pos1);
    verticalLines[1]->setXPercent(pos2);
    verticalLines[2]->setXPercent(pos3);
}

void Multiband::getLinePos(float (&input)[3])
{
    for (int i = 0; i < 3; i++)
    {
        input[i] = verticalLines[i]->getXPercent();
    }
}


void Multiband::setFrequency(int freq1, int freq2, int freq3)
{
    frequency[0] = freq1;
    frequency[1] = freq2;
    frequency[2] = freq3;
    freqTextLabel[0]->setFreq(freq1);
    freqTextLabel[1]->setFreq(freq2);
    freqTextLabel[2]->setFreq(freq3);
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
            input[i] = verticalLines[sortedIndex[i]]->getState();
        }
    }
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
    for (int i = 0; i < 3; i++)
    {
        // create lines and close buttons and then set state
        if (verticalLines[i]->getState())
        {
            closeButtons[i]->setVisible(true);
        }
        else
        {
            closeButtons[i]->setVisible(false);
        }
    }
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
