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
        verticalLines[i]->setState(false);
        addAndMakeVisible(*verticalLines[i]);
        
        closeButtons[i] = std::make_unique<CloseButton>(*verticalLines[i]);
        addAndMakeVisible(*closeButtons[i]);
        
        freqTextLabel[i] = std::make_unique<FreqTextLabel>(*verticalLines[i]);
        addAndMakeVisible(*freqTextLabel[i]);
        
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
        
        //TODO: this may cause high CPU usage! because this is used only when the mouse clicks (add and delete)
        updateLines(margin, size, width);

        // set black masks
        int margin2 = getWidth() / 250; // 1/100 * 4 / 10
        int margin1 = getWidth() * 6 / 1000; // 1/100 * 6 / 10
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        if (lineNum > 0 && enableButton[0]->getState() == false)
        {
            g.fillRect(0, 0, verticalLines[sortedIndex[0]]->getX() + margin2, getHeight());
        }
        for (int i = 1; i < lineNum; i++)
        {
            if (lineNum > 1 && enableButton[i]->getState() == false)
            {
                g.fillRect(verticalLines[sortedIndex[i - 1]]->getX() + margin1, 0, verticalLines[sortedIndex[i]]->getX() - verticalLines[sortedIndex[i - 1]]->getX(), getHeight());
            }
        }
        if (lineNum > 0 && enableButton[lineNum]->getState() == false)
        {
            g.fillRect(verticalLines[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - verticalLines[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
        }
        
        // drag
        for (int i = 0; i < 3; i++)
        {
            if (verticalLines[i]->isMoving())
            {
                float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
                verticalLines[i]->moveToX(lineNum, xPercent, limitLeft, verticalLines, sortedIndex);
            }
        }
    }
    
}

void Multiband::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    
}

void Multiband::updateLines(float margin, float size, float width)
{
    // create
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
            freqTextLabel[i]->setVisible(verticalLines[i]->isMoving() || verticalLines[i]->isMouseOver());
            frequency[i] = freq;
            sortedIndex[count] = i;
            count++;
        }
    }
    lineNum = count;

    // sort
    for(int j = 1; j < count; j++)
    {
        for(int k = 0; k < count - j; k++)
        {
            if(verticalLines[sortedIndex[k]]->getXPercent() > verticalLines[sortedIndex[k + 1]]->getXPercent())
            {
                std::swap(sortedIndex[k], sortedIndex[k + 1]);
            }
        }
    }
    
    // set soloButtons visibility
    for (int i = 0; i < 4; i++)
    {
        if (i <= count && count != 0)
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
    
    // setBounds of soloButtons and enableButtons
    if (count >= 1)
    {
        enableButton[0]->setBounds(verticalLines[sortedIndex[0]]->getX() / 2, margin, size, size);
        for (int i = 1; i < count; i++)
        {
            enableButton[i]->setBounds((verticalLines[sortedIndex[i]]->getX() + verticalLines[sortedIndex[i - 1]]->getX()) / 2, margin, size, size);
        }
        enableButton[count]->setBounds((verticalLines[sortedIndex[count - 1]]->getX() + getWidth()) / 2, margin, size, size);
    }
    
    // should set self index first, then set left and right index
    for (int i = 0; i < count; i++)
    {
        verticalLines[sortedIndex[i]]->setIndex(i); // this index is the No. you count the line from left to right
    }
    
    // set left right index
    for (int i = 0; i < count; i++)
    {
        if (i == 0)
        {
            verticalLines[sortedIndex[i]]->setLeft(-1); // this left index is the index that in verticalLines array
        }
        else
        {
            verticalLines[sortedIndex[i]]->setLeft(verticalLines[sortedIndex[i - 1]]->getIndex());
        }
        if (i == count - 1)
        {
            verticalLines[sortedIndex[i]]->setRight(count);
        }
        else
        {
            verticalLines[sortedIndex[i]]->setRight(verticalLines[sortedIndex[i + 1]]->getIndex());
        }
        
    }

}

void Multiband::mouseUp(const juce::MouseEvent &e)
{
    if (e.mods.isLeftButtonDown())
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
                for (int i = 0; i < 3; i++)
                {
                    // create lines and close buttons and then set state
                    if (verticalLines[i]->getState() == false)
                    {
                        verticalLines[i]->setState(true);
                        verticalLines[i]->setXPercent(xPercent);
                        closeButtons[i]->setVisible(true);
                        freqTextLabel[i]->setVisible(true);
                        break;
                    }
                }
            }
        }
    }
}

int Multiband::getLineNum()
{
    return lineNum;
}
