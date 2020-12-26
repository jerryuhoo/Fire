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
    
    soloButton[0] = std::make_unique<SoloButton>();
    addAndMakeVisible(*soloButton[0]);
    
    enableButton[0] = std::make_unique<EnableButton>();
    addAndMakeVisible(*enableButton[0]);
    
    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        verticalLines[i] = std::make_unique<VerticalLine>();
        addAndMakeVisible(*verticalLines[i]);
        verticalLines[i]->setState(false); // make it invisible, this should bu put after addAndMakeVisible
        
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
        
        float limitLeft = 0.1f;
        float limitRight = 0.9f;
        
        
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
        
        g.drawLine(0, startY + getHeight() / 5, getWidth(), startY + getHeight() / 5, 1);

        
        // create
        float margin = getHeight() / 20.f;
        float size = verticalLines[0]->getHeight() / 10.f;
        float width = verticalLines[0]->getWidth() / 2.f;
        
        // freqLabel
        for (int i = 0; i < 3; i++)
        {
            if (verticalLines[i]->getState())
            {
                freqTextLabel[i]->setVisible(verticalLines[i]->isMoving() || verticalLines[i]->isMouseOver());
            }
        }
        
        
        //TODO: this may cause high CPU usage! because this is used only when the mouse clicks (add and delete)
        int changedIndex = getChangedIndex();
        if (changedIndex != -1)
        {
            updateLines(margin, size, width, false, changedIndex);
        }
        

        // set black and focus masks
        int margin2 = getWidth() / 250; // 1/100 * 4 / 10
        int margin1 = getWidth() * 6 / 1000; // 1/100 * 6 / 10
        
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
        
        
        dragLines(margin, size, width, limitLeft);
 
    }
    
}

void Multiband::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    float margin = getHeight() / 20.f;
    float size = verticalLines[0]->getHeight() / 10.f;
    float width = verticalLines[0]->getWidth() / 2.f;
    updateLines(margin, size, width, false, -1);
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

void Multiband::changeFocus(int changedIndex, bool isAdd)
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
    
    if (isAdd)
    {
        // change focus index when line is added
        if (newLinePosIndex < focusIndex || (newLinePosIndex == focusIndex && getMouseXYRelative().getX() < enableButton[focusIndex]->getX()))
        {
            multibandFocus[focusIndex] = false;
            multibandFocus[focusIndex + 1] = true;
        }
    }
    else // if (!isAdd)
    {
        // change focus index when line is deleted
        if (newLinePosIndex < focusIndex)
        {
            multibandFocus[focusIndex] = false;
            multibandFocus[focusIndex - 1] = true;
        }
    }
    
}

void Multiband::updateLines(float margin, float size, float width, bool isAdd, int changedIndex)
{
//    setLineRelatedBounds(margin, size, width);
    int count = 0;
    
    for (int i = 0; i < 3; i++)
    {
        if (verticalLines[i]->getState())
        {
            verticalLines[i]->setVisible(true);
            
            verticalLines[i]->setBounds(verticalLines[i]->getXPercent() * getWidth() - getWidth() / 200, 0, getWidth() / 100, getHeight());
            closeButtons[i]->setBounds(verticalLines[i]->getX() + width + margin, margin, size, size);

            freqTextLabel[i]->setBounds(verticalLines[i]->getX() + width + margin, getHeight() / 5 + margin, size * 4, size);
            int freq = static_cast<int>(SpectrumComponent::transformFromLog(verticalLines[i]->getXPercent()) * (44100 / 2.0));
            freqTextLabel[i]->setFreq(freq);
//            freqTextLabel[i]->setVisible(verticalLines[i]->isMoving() || verticalLines[i]->isMouseOver());
            frequency[i] = freq;
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

    
    // reset all disabled parameters (maybe HIGH CPU!!!)

    
    // should set self index first, then set left and right index
    for (int i = 0; i < lineNum; i++)
    {
        verticalLines[sortedIndex[i]]->setIndex(i); // this index is the No. you count the line from left to right
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
//
    if (changedIndex != -1) // if not resizing
    {
        // change focus position
        changeFocus(changedIndex, isAdd);
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
    
    setSoloRelatedBounds(margin, size, width);
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
    if (e.mods.isLeftButtonDown() && e.y <= getHeight() / 5.f) // create new lines
    {
        
        if (lineNum < 3)
        {
            bool canCreate = true;
            float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
            float limitLeft = 0.1f;
            float limitRight = 0.9f;
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
                int newLineIndex = 0; // the index of new created line(self index)
                int focusIndex = 0;
                for (int i = 0; i <= lineNum; i++)
                {
                    if(multibandFocus[i] == true)
                    {
                        focusIndex = i;
                        break;
                    }
                }
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
                        
                        newLineIndex = i;
                        
                        break;
                    }
                    
                }
                
                
//                int newLinePosIndex = 0; // (count from left to right in the window)
//                for (int i = 0; i < 3; i++)
//                {
//                    // find newLinePosIndex
//                    if (verticalLines[i]->getIndex() == newLineIndex)
//                    {
//                        newLinePosIndex = i;
//                        break;
//                    }
//                }
                
                float margin = getHeight() / 20.f;
                float size = verticalLines[0]->getHeight() / 10.f;
                float width = verticalLines[0]->getWidth() / 2.f;
//                updateLines(margin, size, width, true, newLinePosIndex);
                int changedIndex = getChangedIndex();
                if (changedIndex != -1)
                {
                    updateLines(margin, size, width, true, changedIndex);
                }

                
            }
        }
        
    }
    else if (e.mods.isLeftButtonDown() && e.y > getHeight() / 5.f) // focus on one band
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

        
//        for (int i = 0; i < 4; i++) {
//            if (multibandFocus[i]) {
//                DBG(i);
//            }
//        }
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

void Multiband::getStateArray(bool (&input)[4])
{
    for (int i = 0; i < 4; i++)
    {
        input[i] = enableButton[i]->getState();
    }
}

void Multiband::getFreqArray(int (&input)[3])
{
    for (int i = 0; i < 3; i++)
    {
        input[i] = frequency[i];
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
    }
    lineNum = 0;
    
}


void Multiband::dragLines(float margin, float size, float width, float limitLeft)
{
    bool isMoving = false;
    // drag
    for (int i = 0; i < 3; i++)
    {
        if (verticalLines[i]->isMoving())
        {
            float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
            verticalLines[i]->moveToX(lineNum, xPercent, limitLeft, verticalLines, sortedIndex);
            isMoving = true;
        }
    }
    
    if (isMoving)
    {
        setLineRelatedBounds(margin, size, width);
        setSoloRelatedBounds(margin, size, width);
    }
}

void Multiband::setLineRelatedBounds(float margin, float size, float width)
{
    for (int i = 0; i < 3; i++)
    {
        verticalLines[i]->setBounds(verticalLines[i]->getXPercent() * getWidth() - getWidth() / 200, 0, getWidth() / 100, getHeight());
        closeButtons[i]->setBounds(verticalLines[i]->getX() + width + margin, margin, size, size);

        freqTextLabel[i]->setBounds(verticalLines[i]->getX() + width + margin, getHeight() / 5 + margin, size * 4, size);
        int freq = static_cast<int>(SpectrumComponent::transformFromLog(verticalLines[i]->getXPercent()) * (44100 / 2.0));
        freqTextLabel[i]->setFreq(freq);
        frequency[i] = freq;
    }
}

void Multiband::setSoloRelatedBounds(float margin, float size, float width)
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
