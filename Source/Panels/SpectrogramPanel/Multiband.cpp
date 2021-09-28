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
Multiband::Multiband(FireAudioProcessor &p) : processor(p)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
//    addAndMakeVisible(spectrum);
    
    const auto& params = processor.getParameters();
    for( auto param : params )
    {
        param->addListener(this);
    }
    
    
    startTimerHz(60);
    
    limitLeft = 0.1f;
    limitRight = 1.0f - limitLeft;
    
    soloButton[0] = std::make_unique<SoloButton>();
    addAndMakeVisible(*soloButton[0]);
    
    enableButton[0] = std::make_unique<EnableButton>();
    addAndMakeVisible(*enableButton[0]);
    
    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup>(processor, i);
//        freqDividerGroup[i]->setIndex(i);
        freqDividerGroup[i]->setRange(21, 11040);
        addAndMakeVisible(*freqDividerGroup[i]);
        (*freqDividerGroup[i]).addListener(this);
        
        soloButton[i + 1] = std::make_unique<SoloButton>();
        addAndMakeVisible(*soloButton[i + 1]);
        
        enableButton[i + 1] = std::make_unique<EnableButton>();
        addAndMakeVisible(*enableButton[i + 1]);
    }
    
//    multiFreqSlider1.addListener(this);
//    multiFreqSlider2.addListener(this);
//    multiFreqSlider3.addListener(this);
    
//    multiFocusAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID1, multiFocusSlider1);
//    multiFocusAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID2, multiFocusSlider2);
//    multiFocusAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID3, multiFocusSlider3);
//    multiFocusAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, BAND_FOCUS_ID4, multiFocusSlider4);
    
    multiEnableAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID1, *enableButton[0]);
    multiEnableAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID2, *enableButton[1]);
    multiEnableAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID3, *enableButton[2]);
    multiEnableAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID4, *enableButton[3]);
    
//    multiFreqAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID1, multiFreqSlider1);
//    multiFreqAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID2, multiFreqSlider2);
//    multiFreqAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, FREQ_ID3, multiFreqSlider3);
    
//    lineStateSliderAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID1, lineStateSlider1);
//    lineStateSliderAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID2, lineStateSlider2);
//    lineStateSliderAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.treeState, LINE_STATE_ID3, lineStateSlider3);

    
//    margin = getHeight() / 20.0f;
//    size = freqDividerGroup[0]->verticalLine.getHeight() / 10.f;
//    width = freqDividerGroup[0]->verticalLine.getWidth() / 2.f;
//    updateLines("do nothing", -1);
//    spectrum.setInterceptsMouseClicks(false, false);

}

Multiband::~Multiband()
{
    const auto& params = processor.getParameters();
    for( auto param : params )
    {
        param->removeListener(this);
    }
}

void Multiband::paint (juce::Graphics& g)
{
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
            if ((freqDividerGroup[i]->getCloseButton().getToggleState() && fabs(freqDividerGroup[i]->verticalLine.getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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

    float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
    dragLines(targetXPercent);

    // set black and focus masks
    int margin2 = getWidth() / 250; // 1/100 * 4 / 10
    int margin1 = getWidth() * 6 / 1000; // 1/100 * 6 / 10
    
    // delete line and reset deleted disabled band state
    for (int i = 0; i < 4; ++i)
    {
        if (!enableButton[i]->getToggleState() && i > lineNum)
        {
            enableButton[i]->setToggleState(true, juce::NotificationType::sendNotification);
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
    
    if (lineNum > 0 && enableButton[0]->getToggleState() == false)
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
        if (lineNum > 1 && enableButton[i]->getToggleState() == false)
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
    if (lineNum > 0 && enableButton[lineNum]->getToggleState() == false)
    {
        g.setColour(COLOUR_MASK_BLACK);
        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
    }
    
//    float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
//    dragLines(targetXPercent);

    
    // set value only when line is deleted, added, moving
    if (getDeleteState() || getAddState() || getMovingState())
    {
        if (getDeleteState())
        {
            updateLines("delete", getChangedIndex());
            setDeleteState(false);
        }
        else if (getAddState())
        {
            updateLines("add", getChangedIndex());
            setAddState(false);
        }
        else if (getMovingState())
        {
            updateLines("moving", getChangedIndex());
            setMovingState(false);
        }
        updateFreqArray();

//        getLineState(lineState);
//        lineStateSlider1.setValue(lineState[0]);
//        lineStateSlider2.setValue(lineState[1]);
//        lineStateSlider3.setValue(lineState[2]);
        
        processor.setLineNum(getLineNum());
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
        if (lastLineState[i] != freqDividerGroup[i]->getCloseButton().getToggleState())
        {
            lastLineState[i] = freqDividerGroup[i]->getCloseButton().getToggleState();
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
            if ((!enableButton[i]->getToggleState() && i > newLinePosIndex) ||
                (!enableButton[i]->getToggleState() && getMouseXYRelative().getX() < enableButton[i]->getX()))
            {
                //enableState[i] = true;
                enableButton[i]->setToggleState(true, juce::NotificationType::sendNotification);;
//                if (i < lineNum)
//                {
                    //enableState[i + 1] = false;
                    enableButton[i + 1]->setToggleState(false, juce::NotificationType::sendNotification);
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
            if (!enableButton[i]->getToggleState() && i > newLinePosIndex)
            {
                //enableState[i] = true;
                enableButton[i]->setToggleState(true, juce::NotificationType::sendNotification);;
                
                //enableState[i - 1] = false;
                enableButton[i - 1]->setToggleState(false, juce::NotificationType::sendNotification);
            }
        }
        
        // special case: delete first line and only line number is 1 (after deleting it is 0), then delete enable[0] state
        if (newLinePosIndex == 0 && lineNum == 0)
        {
            //enableState[0] = true;
            enableButton[0]->setToggleState(true, juce::NotificationType::sendNotification);;
        }
    }
}

void Multiband::updateLines(juce::String option, int changedIndex)
{
    int count = 0;
    
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getCloseButton().getToggleState())
        {
//            freqDividerGroup[i]->verticalLine.setVisible(true);
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
    
//    DBG("========");
//    if(freqDividerGroup[0]->getCloseButton().getToggleState())DBG("[0] true");
//    if(freqDividerGroup[1]->getCloseButton().getToggleState())DBG("[1] true");
//    if(freqDividerGroup[2]->getCloseButton().getToggleState())DBG("[2] true");
//    DBG("-sorted index-");
//    DBG(sortedIndex[0]);
//    DBG(sortedIndex[1]);
//    DBG(sortedIndex[2]);
//    DBG("-freq");
//    DBG(freqDividerGroup[0]->getFrequency());
//    DBG(freqDividerGroup[1]->getFrequency());
//    DBG(freqDividerGroup[2]->getFrequency());
    
}

void Multiband::mouseUp(const juce::MouseEvent &e)
{
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->verticalLine.setMoving(false);
    }
}

void Multiband::mouseDrag(const juce::MouseEvent &e)
{
//    float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
//    dragLines(targetXPercent);
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
                if ((freqDividerGroup[i]->getCloseButton().getToggleState() && fabs(freqDividerGroup[i]->verticalLine.getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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
                    if (!freqDividerGroup[i]->getCloseButton().getToggleState())
                    {
                        freqDividerGroup[i]->setCloseButtonValue(true);
                        freqDividerGroup[i]->verticalLine.setXPercent(xPercent);
                        freqDividerGroup[i]->verticalLine.setMoving(true);
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
//    getFocusArray(multibandFocus);
//    multiFocusSlider1.setValue(multibandFocus[0]);
//    multiFocusSlider2.setValue(multibandFocus[1]);
//    multiFocusSlider3.setValue(multibandFocus[2]);
//    multiFocusSlider4.setValue(multibandFocus[3]);
}

int Multiband::getLineNum()
{
    return lineNum;
}

//void Multiband::getFocusArray(bool (&input)[4])
//{
//    for (int i = 0; i < 4; i++)
//    {
//        input[i] = multibandFocus[i];
//    }
//}

void Multiband::getEnableArray(bool (&input)[4])
{
    for (int i = 0; i < 4; i++)
    {
        input[i] = enableButton[i]->getToggleState();
    }
}

//void Multiband::getFreqArray(int (&input)[3])
//{
//    for (int i = 0; i < lineNum; i++)
//    {
//        input[i] = freqDividerGroup[sortedIndex[i]]->getFrequency();
//    }
//    for (int i = lineNum; i < 3; i++)
//    {
//        input[i] = 0;
//    }
//}

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
        freqDividerGroup[i]->getCloseButton().setToggleState(false, juce::NotificationType::dontSendNotification);
        multibandFocus[i + 1] = false;
        sortedIndex[i] = 0;
        lastLineState[i] = freqDividerGroup[i]->getCloseButton().getToggleState();
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
            if (freqDividerGroup[sortedIndex[i]]->getCloseButton().getToggleState())
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
//            if (freqDividerGroup[sortedIndex[i]]->getCloseButton().getToggleState())
//            {
//                setLineRelatedBoundsByFreq(sortedIndex[i]);
//                frequency[sortedIndex[i]] = freqDividerGroup[sortedIndex[i]]->getFrequency();
//
//            }
//        }
//        setSoloRelatedBounds();
        
        for (int i = 0; i < lineNum; i++)
        {
//            if (freqDividerGroup[sortedIndex[i]]->getChangeState())
//            {
//                dragLinesByFreq(freqDividerGroup[sortedIndex[i]]->verticalLine.getValue(), sortedIndex[i]);
//                freqDividerGroup[sortedIndex[i]]->setChangeState(false);
//            }

        }
    }
}

// In Ableton, move the slider in the control bar, the lines should move.
void Multiband::dragLinesByFreq(int freq, int index)
{
    if (index < 0) return;
    if (!isMoving && freqDividerGroup[index]->getCloseButton().getToggleState())
    {
        float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freq / (44100 / 2.0)));
        freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup, sortedIndex);
        
        // keep distance limit between lines
        for (int i = 0; i < lineNum; i++)
        {
            if (freqDividerGroup[sortedIndex[i]]->getCloseButton().getToggleState())
            {
                // get the correct xPercent if the line cannot move.
                xPercent = freqDividerGroup[sortedIndex[i]]->verticalLine.getXPercent();

                freqDividerGroup[sortedIndex[i]]->verticalLine.setXPercent(xPercent);
                freqDividerGroup[sortedIndex[i]]->setBounds(xPercent * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
                // get the correct freq if the line cannot move.
                //freq = freqDividerGroup[sortedIndex[i]]->getFrequency();
            }
        }
        
//        if (freqDividerGroup[index]->getCloseButton().getToggleState())
//        {
//            freqDividerGroup[index]->setFrequency(freq);
//            frequency[index] = freq;
//        }

        setSoloRelatedBounds();
    }
}

void Multiband::setLineRelatedBoundsByX(int i)
{
    if(freqDividerGroup[i]->getCloseButton().getToggleState())
    {
        freqDividerGroup[i]->setBounds(freqDividerGroup[i]->verticalLine.getXPercent() * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());

        int freq = static_cast<int>(SpectrumComponent::transformFromLog(freqDividerGroup[i]->verticalLine.getXPercent()) * (44100 / 2.0));
//        freqDividerGroup[i]->setFrequency(freq);
        freqDividerGroup[i]->verticalLine.setValue(freq);
    }
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
//    freqDividerGroup[0]->verticalLine.setState(state1);
//    freqDividerGroup[1]->verticalLine.setState(state2);
//    freqDividerGroup[2]->verticalLine.setState(state3);
    freqDividerGroup[0]->getCloseButton().setToggleState(state1, juce::NotificationType::dontSendNotification);
    freqDividerGroup[1]->getCloseButton().setToggleState(state2, juce::NotificationType::dontSendNotification);
    freqDividerGroup[2]->getCloseButton().setToggleState(state3, juce::NotificationType::dontSendNotification);
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
            input[i] = freqDividerGroup[sortedIndex[i]]->getCloseButton().getToggleState();
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
    if(freqDividerGroup[0]->getCloseButton().getToggleState()) freqDividerGroup[0]->verticalLine.setValue(freq1);//freqDividerGroup[0]->setFrequency(freq1);
    if(freqDividerGroup[1]->getCloseButton().getToggleState())
        freqDividerGroup[1]->verticalLine.setValue(freq2);//freqDividerGroup[1]->setFrequency(freq2);
    if(freqDividerGroup[2]->getCloseButton().getToggleState())
        freqDividerGroup[2]->verticalLine.setValue(freq3);//freqDividerGroup[2]->setFrequency(freq3);
}

void Multiband::setEnableState(bool state1, bool state2, bool state3, bool state4)
{
//    enableState[0] = state1;
//    enableState[1] = state2;
//    enableState[2] = state3;
//    enableState[3] = state4;
    enableButton[0]->setToggleState(state1, juce::NotificationType::sendNotification);
    enableButton[1]->setToggleState(state2, juce::NotificationType::sendNotification);
    enableButton[2]->setToggleState(state3, juce::NotificationType::sendNotification);
    enableButton[3]->setToggleState(state4, juce::NotificationType::sendNotification);
}

void Multiband::setCloseButtonState()
{
//    for (int i = 0; i < 3; i++)
//    {
//        // create lines and close buttons and then set state
//        if (freqDividerGroup[i]->getCloseButton().getToggleState())
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
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getDeleteState())
        {
            freqDividerGroup[i]->verticalLine.setValue(21); //why FreqDividerGroup.cpp line 56 doesn't work???
            return true;
        }
    }
    return false;
}

void Multiband::setDeleteState(bool state)
{
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getDeleteState())
        {
            freqDividerGroup[i]->setDeleteState(state);
        }
    }
}

int Multiband::getSortedIndex(int index)
{
    return sortedIndex[index];
}

void Multiband::timerCallback()
{
    if( parametersChanged.compareAndSetBool(false, true) )
    {
//        if (freqDividerGroup[0]->verticalLine.is) dragLinesByFreq(freqDividerGroup[0]->verticalLine.getValue(), getSortedIndex(0));
//        if (freqDividerGroup[1]->verticalLine.valueChanged()) dragLinesByFreq(freqDividerGroup[1]->verticalLine.getValue(), getSortedIndex(1));
//        if (freqDividerGroup[2]->verticalLine.valueChanged()) dragLinesByFreq(freqDividerGroup[2]->verticalLine.getValue(), getSortedIndex(2));
    }
    repaint();
}

int Multiband::getFocusBand()
{
    for (int i = 0; i < 4; i++)
    {
        if (multibandFocus[i])
        {
            return i;
        }
    }
    return 0;
}

void Multiband::updateFreqArray()
{
//    getFreqArray(multibandFreq);
//    multiFreqSlider1.setValue(multibandFreq[0]);
//    multiFreqSlider2.setValue(multibandFreq[1]);
//    multiFreqSlider3.setValue(multibandFreq[2]);
}


void Multiband::sliderValueChanged(juce::Slider *slider)
{
    for (int i = 0; i < 3; i++)
    {
        if (slider == &*freqDividerGroup[sortedIndex[i]])
        {
            dragLinesByFreq(freqDividerGroup[sortedIndex[i]]->verticalLine.getValue(), sortedIndex[i]);
        }
    }
}

void Multiband::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void Multiband::buttonClicked(juce::Button* button)
{
//    // set black and focus masks
//    int margin2 = getWidth() / 250; // 1/100 * 4 / 10
//    int margin1 = getWidth() * 6 / 1000; // 1/100 * 6 / 10
//    
//    // delete line and reset deleted disabled band state
//    for (int i = 0; i < 4; ++i)
//    {
//        if (!enableButton[i]->getToggleState() && i > lineNum)
//        {
//            enableButton[i]->setToggleState(true, juce::NotificationType::sendNotification);
//        }
//    }
//    
//    if (lineNum > 0 && multibandFocus[0])
//    {
////            g.setColour(COLOUR_MASK_RED);
//        juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
//                                  COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
//        g.setGradientFill(grad);
//        g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
//    }
//    
//    if (lineNum > 0 && enableButton[0]->getToggleState() == false)
//    {
//        g.setColour(COLOUR_MASK_BLACK);
//        g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
//    }
//    
//    for (int i = 1; i < lineNum; i++)
//    {
//        if (lineNum > 1 && multibandFocus[i])
//        {
////                g.setColour(COLOUR_MASK_RED);
//            juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
//                                      COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
//            g.setGradientFill(grad);
//            g.fillRect(freqDividerGroup[sortedIndex[i - 1]]->getX() + margin1, 0, freqDividerGroup[sortedIndex[i]]->getX() - freqDividerGroup[sortedIndex[i - 1]]->getX(), getHeight());
//        }
//        if (lineNum > 1 && enableButton[i]->getToggleState() == false)
//        {
//            g.setColour(COLOUR_MASK_BLACK);
//            g.fillRect(freqDividerGroup[sortedIndex[i - 1]]->getX() + margin1, 0, freqDividerGroup[sortedIndex[i]]->getX() - freqDividerGroup[sortedIndex[i - 1]]->getX(), getHeight());
//        }
//    }
//
//    if (lineNum > 0 && multibandFocus[lineNum])
//    {
////            g.setColour(COLOUR_MASK_RED);
//        juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
//                                  COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
//        g.setGradientFill(grad);
//        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
//    }
//    if (lineNum > 0 && enableButton[lineNum]->getToggleState() == false)
//    {
//        g.setColour(COLOUR_MASK_BLACK);
//        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
//    }
}
