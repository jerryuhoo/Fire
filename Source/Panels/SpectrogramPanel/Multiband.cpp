/*
  ==============================================================================

    Multiband.cpp
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Multiband.h"
#include <algorithm>
//==============================================================================
Multiband::Multiband(FireAudioProcessor &p, state::StateComponent &sc) : processor(p), stateComponent(sc)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
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
    soloButton[0]->addListener(this);
    
    enableButton[0] = std::make_unique<EnableButton>();
    addAndMakeVisible(*enableButton[0]);
    enableButton[0]->addListener(this);
    
    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup>(processor, i);
//        freqDividerGroup[i]->setIndex(i);
        addAndMakeVisible(*freqDividerGroup[i]);
        (freqDividerGroup[i]->getVerticalLine()).addListener(this);
        (freqDividerGroup[i]->getCloseButton()).addListener(this);
        float freqValue = freqDividerGroup[i]->getVerticalLine().getValue();
        float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freqValue));
        freqDividerGroup[i]->setBounds(xPercent * getWidth(), 0, getWidth(), getHeight());
        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
        
        soloButton[i + 1] = std::make_unique<SoloButton>();
        addAndMakeVisible(*soloButton[i + 1]);
        soloButton[i + 1]->addListener(this);
        
        enableButton[i + 1] = std::make_unique<EnableButton>();
        addAndMakeVisible(*enableButton[i + 1]);
        enableButton[i + 1]->addListener(this);
    }
    
    multiEnableAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID1, *enableButton[0]);
    multiEnableAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID2, *enableButton[1]);
    multiEnableAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID3, *enableButton[2]);
    multiEnableAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_ENABLE_ID4, *enableButton[3]);

    multiSoloAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_SOLO_ID1, *soloButton[0]);
    multiSoloAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_SOLO_ID2, *soloButton[1]);
    multiSoloAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_SOLO_ID3, *soloButton[2]);
    multiSoloAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.treeState, BAND_SOLO_ID4, *soloButton[3]);
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
    // set graph buffer
    int focusIndex = 0;
    bool findFocus = false;
    for (int i = 0; i < lineNum + 1; i++)
    {
        if (multibandFocus[i])
        {
            focusIndex = i;
            findFocus = true;
            break;
        }
    }
    
    if (!findFocus)
    {
        focusIndex = lineNum;
        multibandFocus[lineNum] = true;
        for (int i = lineNum + 1; i < 4; i++)
        {
            multibandFocus[i] = false;
        }
    }

    if (isVisible()) processor.setHistoryArray(focusIndex);
    
    // set value only when line is deleted, added, moving
    if (/*getDeleteState() || getAddState() ||*/ getMovingState())
    {
//        if (getDeleteState())
//        {
//            updateLines();
//            setDeleteState(false);
//        }
//        else if (getAddState())
//        {
//            updateLines();
//            setAddState(false);
//        }
//        if (getMovingState())
//        {
            updateLines(0);
            setMovingState(false);
//        }
    }
    
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
            if ((freqDividerGroup[i]->getCloseButton().getToggleState() && fabs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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

    if (lineNum == 0 && shouldSetBlackMask(0))
    {
        g.setColour(COLOUR_MASK_BLACK);
        g.fillRect(0, 0, getWidth() + margin2, getHeight());
    }
    
    if (lineNum > 0 && multibandFocus[0])
    {
        juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                  COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
        g.setGradientFill(grad);
        g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
    }
    
    int mouseX = getMouseXYRelative().getX();
    int mouseY = getMouseXYRelative().getY();
    
    // set mouse enter white
    if (!multibandFocus[0] && lineNum > 0 && mouseX > 0 && mouseX < freqDividerGroup[sortedIndex[0]]->getX() + margin2 && mouseY > 0 && mouseY < getHeight())
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
    }
    
    if (lineNum > 0 && shouldSetBlackMask(0))
    {
        g.setColour(COLOUR_MASK_BLACK);
        g.fillRect(0, 0, freqDividerGroup[sortedIndex[0]]->getX() + margin2, getHeight());
    }
    
    for (int i = 1; i < lineNum; i++)
    {
        int startX = freqDividerGroup[sortedIndex[i - 1]]->getX() + margin1;
        int bandWidth = freqDividerGroup[sortedIndex[i]]->getX() - freqDividerGroup[sortedIndex[i - 1]]->getX();
        
        if (lineNum > 1 && multibandFocus[i])
        {
            juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                      COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
            g.setGradientFill(grad);
            g.fillRect(startX, 0, bandWidth, getHeight());
        }
        
        if (lineNum > 1 && shouldSetBlackMask(i))
        {
            g.setColour(COLOUR_MASK_BLACK);
            g.fillRect(startX, 0, bandWidth, getHeight());
        }
        
        if (!multibandFocus[i] && lineNum > 1 && mouseX > startX && mouseX < startX + bandWidth && mouseY > 0 && mouseY < getHeight())
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.fillRect(startX, 0, bandWidth, getHeight());
        }
    }

    if (lineNum > 0 && multibandFocus[lineNum])
    {
        juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, 0,
                                  COLOUR1.withAlpha(0.2f), getLocalBounds().getWidth(), 0, false);
        g.setGradientFill(grad);
        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
    }
    
    // set mouse enter white
    if (!multibandFocus[lineNum] && lineNum > 0 && mouseX > freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1 && mouseX < getWidth() && mouseY > 0 && mouseY < getHeight())
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
    }
    
    if (lineNum > 0 && shouldSetBlackMask(lineNum))
    {
        g.setColour(COLOUR_MASK_BLACK);
        g.fillRect(freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + margin1, 0, getWidth() - freqDividerGroup[sortedIndex[lineNum - 1]]->getX() - margin1, getHeight());
    }
    
//    float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
//    dragLines(targetXPercent);
    
    // if preset is changed
    if (stateComponent.getChangedState())
    {
        stateComponent.setChangedState(false);
        updateLines(1); // 1 means setBounds by setting frequency
        setSoloRelatedBounds();
        processor.setLineNum();
    }
}

void Multiband::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    margin = getHeight() / 20.0f;
    size = getWidth() / 1000.0f * 15.0f;
    width = freqDividerGroup[0]->getVerticalLine().getWidth() / 2.0f;
    
    // temp method to fix
    if (size == 0)
    {
        size = 15.0f;
    }
    if (width == 0)
    {
        width = 5.0f;
    }

    updateLines(0);
    setSoloRelatedBounds();
}

bool Multiband::shouldSetBlackMask(int index)
{
    bool otherBandSoloIsOn = false;
    for (int i = 0; i <= lineNum; i++)
    {
        if (i == index) continue;
        if (soloButton[i]->getToggleState())
        {
            otherBandSoloIsOn = true;
            break;
        }
    }
    return (!soloButton[index]->getToggleState() && otherBandSoloIsOn);
}

void Multiband::setParametersToAFromB(int toIndex, int fromIndex)
{
    
    //if (toIndex == 0)
    std::unique_ptr<std::vector<juce::String>> fromArray;
    std::unique_ptr<std::vector<juce::String>> toArray;
    
    if (fromIndex == 0) fromArray = std::make_unique<std::vector<juce::String>>(paramsArray1);
    if (fromIndex == 1) fromArray = std::make_unique<std::vector<juce::String>>(paramsArray2);
    if (fromIndex == 2) fromArray = std::make_unique<std::vector<juce::String>>(paramsArray3);
    if (fromIndex == 3) fromArray = std::make_unique<std::vector<juce::String>>(paramsArray4);
    
    if (toIndex == 0) toArray = std::make_unique<std::vector<juce::String>>(paramsArray1);
    if (toIndex == 1) toArray = std::make_unique<std::vector<juce::String>>(paramsArray2);
    if (toIndex == 2) toArray = std::make_unique<std::vector<juce::String>>(paramsArray3);
    if (toIndex == 3) toArray = std::make_unique<std::vector<juce::String>>(paramsArray4);
    
    for (const auto &param : processor.getParameters())
    {
        if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param))
        {
            // from array
            float paramFromValue = p->getValue();
            juce::String paramFromName = p->name;
            
            for (int i = 0; i < fromArray->size(); i++)
            {
                if (fromArray.get()[0][i] == paramFromName)
                {
                    // to array
                    for (const auto &paramTo : processor.getParameters())
                    {
                        if (auto *pTo = dynamic_cast<juce::AudioProcessorParameterWithID *>(paramTo))
                        {
                            juce::String paramToName = pTo->name;
                            
                            if (toArray.get()[0][i] == paramToName)
                            {
                                pTo->setValueNotifyingHost(paramFromValue);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

bool Multiband::isParamInArray(juce::String paramName, std::vector<juce::String> paramArray)
{
    bool isInArray =false;
    
    for (int i = 0; i < paramArray.size(); i++)
    {
        if(paramName == paramArray[i])
        {
            isInArray = true;
            break;
        }
    }
    
    return isInArray;
}

void Multiband::initParameters(int bandindex)
{
    for (const auto &param : processor.getParameters())
        if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param))
        {
            if (bandindex == 0 && isParamInArray(p->name, paramsArray1))
            {
                p->setValueNotifyingHost(p->getDefaultValue());
            }
            if (bandindex == 1 && isParamInArray(p->name, paramsArray2))
            {
                p->setValueNotifyingHost(p->getDefaultValue());
            }
            if (bandindex == 2 && isParamInArray(p->name, paramsArray3))
            {
                p->setValueNotifyingHost(p->getDefaultValue());
            }
            if (bandindex == 3 && isParamInArray(p->name, paramsArray4))
            {
                p->setValueNotifyingHost(p->getDefaultValue());
            }
        }
}

void Multiband::setStatesWhenAddOrDelete(int changedIndex, juce::String option)
{
    /**
     set enable buttons states, solo button states, focus band, and parameters for band processing
     */
    
    // get current focus index
    int focusIndex = 0;
    bool findFocus = false;
    for (int i = 0; i < lineNum + 1; i++)
    {
        if (multibandFocus[i])
        {
            focusIndex = i;
            findFocus = true;
            break;
        }
    }
    
    if (!findFocus)
    {
        focusIndex = lineNum;
        multibandFocus[lineNum] = true;
        for (int i = lineNum + 1; i < 4; i++)
        {
            multibandFocus[i] = false;
        }
    }
    
    // get changed index -> position index
    int newLinePosIndex = 0; // (count from left to right in the window)
    newLinePosIndex = freqDividerGroup[changedIndex]->getVerticalLine().getIndex();
//    for (int i = 0; i < lineNum; i++)
//    {
//        if (changedIndex == sortedIndex[i])
//        {
//            newLinePosIndex = i;
//            break;
//        }
//    }
    
    /**
     When adding or deleting lines, this function is to keep enable and solo buttons in the right order and states.
     For example, you have [e1 | e2 | e3 | e4], If you delete line1 [e1 | e2 | <---- delete here!   e3 | e4]  we should only change e2/e3 state(in the middle),
     not affecting e1 and e4.
     */
    
    if (option == "add")
    {
        // change focus index when line is added
        if (newLinePosIndex < focusIndex || (newLinePosIndex == focusIndex && getMouseXYRelative().getX() < (enableButton[focusIndex]->getX() + soloButton[focusIndex]->getX()) / 2.0f ))
        {
            multibandFocus[focusIndex] = false;
            multibandFocus[focusIndex + 1] = true;
            
        }

        // change enable/solo index when line is added
        for (int i = lineNum - 1; i >= 0; --i)
        {
            // add new line, move the olds to the right
            if (i >= newLinePosIndex)
            {
                // add when mouse click is on the left side of buttons, move i to i + 1
                if (getMouseXYRelative().getX() < (enableButton[i]->getX() + soloButton[i]->getX()) / 2.0f)
                {
                    // old button
                    enableButton[i + 1]->setToggleState(enableButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                    // new added button
                    enableButton[i]->setToggleState(true, juce::NotificationType::sendNotification);
                    
                    soloButton[i + 1]->setToggleState(soloButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                    soloButton[i]->setToggleState(false, juce::NotificationType::sendNotification);
                    
                    // move parameters
                    setParametersToAFromB(i + 1, i);
                    initParameters(i);
                }
                else // mouse click is on the right side of buttons, keep i not change, i + 1 is new
                {
                    enableButton[i + 1]->setToggleState(true, juce::NotificationType::sendNotification);
                    soloButton[i + 1]->setToggleState(false, juce::NotificationType::sendNotification);
                    initParameters(i + 1);
                }
            }
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
        
        // change enable/solo index when line is deleted
        for (int i = 1; i <= lineNum + 1; ++i) // line is already deleted so +1
        {
            // for safety
            if (i > 3)
            {
                break;
            }
            
            // move the right side of the deleted line to the left
            if (i > newLinePosIndex)
            {
                enableButton[i - 1]->setToggleState(enableButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                soloButton[i - 1]->setToggleState(soloButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                setParametersToAFromB(i - 1, i);
            }
        }
    }
    setSoloRelatedBounds();
}

void Multiband::updateLines(int option)
{
    updateLineNumAndSortedIndex(option);
    updateLineLeftRightIndex();
}

void Multiband::updateLineNumAndSortedIndex(int option)
{
    int count = 0;
    
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getCloseButton().getToggleState())
        {
            if (option == 0)
            {
                setLineRelatedBoundsByX(i);
            }
            else
            {
                setLineRelatedBoundsByFreq(i);
            }
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
            if(freqDividerGroup[sortedIndex[k]]->getVerticalLine().getXPercent() > freqDividerGroup[sortedIndex[k + 1]]->getVerticalLine().getXPercent())
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
}

void Multiband::updateLineLeftRightIndex()
{
    // should set self index first, then set left and right index
    for (int i = 0; i < lineNum; i++)
    {
        freqDividerGroup[sortedIndex[i]]->getVerticalLine().setIndex(i); // this index is the No. you count the line from left to right
    }
    
    // set left right index
    for (int i = 0; i < lineNum; i++)
    {
        if (i == 0)
        {
            freqDividerGroup[sortedIndex[i]]->getVerticalLine().setLeft(-1); // this left index is the index that in verticalLines array
        }
        else
        {
            freqDividerGroup[sortedIndex[i]]->getVerticalLine().setLeft(freqDividerGroup[sortedIndex[i - 1]]->getVerticalLine().getIndex());
        }
        if (i == lineNum - 1)
        {
            freqDividerGroup[sortedIndex[i]]->getVerticalLine().setRight(lineNum);
        }
        else
        {
            freqDividerGroup[sortedIndex[i]]->getVerticalLine().setRight(freqDividerGroup[sortedIndex[i + 1]]->getVerticalLine().getIndex());
        }
    }
    
//    bool isMoving = false;
//    for (int i = 0; i < lineNum; i++)
//    {
//        if (freqDividerGroup[sortedIndex[i]]->getVerticalLine().isMoving())
//        {
//            isMoving = true;
//            break;
//        }
//    }
}

void Multiband::mouseUp(const juce::MouseEvent &e)
{
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->getVerticalLine().setMoving(false);
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
                if ((freqDividerGroup[i]->getCloseButton().getToggleState() && fabs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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
                    if (!freqDividerGroup[i]->getCloseButton().getToggleState())
                    {
                        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
                        freqDividerGroup[i]->getVerticalLine().setMoving(true);
                        freqDividerGroup[i]->setCloseButtonValue(true);
                        updateLines(0);
                        processor.setLineNum();
                        setStatesWhenAddOrDelete(i, "add");
                        break;
                    }
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

void Multiband::getEnableArray(bool (&input)[4])
{
    for (int i = 0; i < 4; i++)
    {
        input[i] = enableButton[i]->getToggleState();
    }
}

void Multiband::reset()
{
    for (int i = 1; i < 4; i++)
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
    }
    lineNum = 0;
}

// 3 ways to change frequency: 1. change freqLabel; 2. drag; 3.change Ableton slider
void Multiband::dragLines(float xPercent)
{
    // drag
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getVerticalLine().isMoving())
//        if (freqDividerGroup[i]->getVerticalLine().isMouseButtonDown())
        {
            freqDividerGroup[i]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup, sortedIndex);
            isMoving = true;
        }
    }

    if (isMoving) // drag
    {
        setSoloRelatedBounds();
    }
}

// In Ableton, move the slider in the control bar, the lines should move.
void Multiband::dragLinesByFreq(int freq, int index)
{
    if (index < 0) return;
    if (!isMoving && freqDividerGroup[index]->getCloseButton().getToggleState())
    {
        float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freq));
        freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup, sortedIndex);
        
        // keep distance limit between lines
        for (int i = 0; i < lineNum; i++)
        {
            if (freqDividerGroup[sortedIndex[i]]->getCloseButton().getToggleState())
            {
                // get the correct xPercent if the line cannot move.
                xPercent = freqDividerGroup[sortedIndex[i]]->getVerticalLine().getXPercent();

                freqDividerGroup[sortedIndex[i]]->getVerticalLine().setXPercent(xPercent);
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
        freqDividerGroup[i]->setBounds(freqDividerGroup[i]->getVerticalLine().getXPercent() * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
        int freq = static_cast<int>(SpectrumComponent::transformFromLog(freqDividerGroup[i]->getVerticalLine().getXPercent()));
        freqDividerGroup[i]->getVerticalLine().setValue(freq);
    }
}

void Multiband::setLineRelatedBoundsByFreq(int i)
{
    int freq = freqDividerGroup[i]->getVerticalLine().getValue();
    float xPercent = static_cast<float>(SpectrumComponent::transformToLog(freq));
    freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
    freqDividerGroup[i]->setBounds(xPercent * getWidth() - getWidth() / 200, 0, getWidth(), getHeight());
}

void Multiband::setSoloRelatedBounds()
{
    for (int i = 0; i < 4; i++)
    {
        if (i <= lineNum)
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
    if (lineNum >= 1)
    {
        enableButton[0]->setBounds(freqDividerGroup[sortedIndex[0]]->getX() / 2 - size, margin, size, size);
        soloButton[0]->setBounds(freqDividerGroup[sortedIndex[0]]->getX() / 2 + size, margin, size, size);
        for (int i = 1; i < lineNum; i++)
        {
            enableButton[i]->setBounds((freqDividerGroup[sortedIndex[i]]->getX() + freqDividerGroup[sortedIndex[i - 1]]->getX()) / 2 - size, margin, size, size);
            soloButton[i]->setBounds((freqDividerGroup[sortedIndex[i]]->getX() + freqDividerGroup[sortedIndex[i - 1]]->getX()) / 2 + size, margin, size, size);
        }
        enableButton[lineNum]->setBounds((freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + getWidth()) / 2 - size, margin, size, size);
        soloButton[lineNum]->setBounds((freqDividerGroup[sortedIndex[lineNum - 1]]->getX() + getWidth()) / 2 + size, margin, size, size);
    }
    else if (lineNum == 0)
    {
        enableButton[0]->setBounds(getWidth() / 2 - size, margin, size, size);
        soloButton[0]->setBounds(getWidth() / 2 + size, margin, size, size);
    }
}

void Multiband::setFocus(bool focus1, bool focus2, bool focus3, bool focus4)
{
    multibandFocus[0] = focus1;
    multibandFocus[1] = focus2;
    multibandFocus[2] = focus3;
    multibandFocus[3] = focus4;
}

void Multiband::setLineState(bool state1, bool state2, bool state3)
{
    freqDividerGroup[0]->getCloseButton().setToggleState(state1, juce::NotificationType::dontSendNotification);
    freqDividerGroup[1]->getCloseButton().setToggleState(state2, juce::NotificationType::dontSendNotification);
    freqDividerGroup[2]->getCloseButton().setToggleState(state3, juce::NotificationType::dontSendNotification);
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
    freqDividerGroup[0]->getVerticalLine().setXPercent(pos1);
    freqDividerGroup[1]->getVerticalLine().setXPercent(pos2);
    freqDividerGroup[2]->getVerticalLine().setXPercent(pos3);
}

void Multiband::getLinePos(float (&input)[3])
{
    for (int i = 0; i < 3; i++)
    {
        input[i] = freqDividerGroup[i]->getVerticalLine().getXPercent();
    }
}

void Multiband::setFrequency(int freq1, int freq2, int freq3)
{
    if (freqDividerGroup[0]->getCloseButton().getToggleState())
    {
        //freqDividerGroup[0]->getVerticalLine().setValue(freq1);
        freqDividerGroup[0]->setFreq(freq1);
    }
    if (freqDividerGroup[1]->getCloseButton().getToggleState())
    {
        //freqDividerGroup[1]->getVerticalLine().setValue(freq2);
        freqDividerGroup[1]->setFreq(freq2);
    }
    if(freqDividerGroup[2]->getCloseButton().getToggleState())
    {
        //freqDividerGroup[2]->getVerticalLine().setValue(freq3);
        freqDividerGroup[2]->setFreq(freq3);
    }
}

void Multiband::setEnableState(bool state1, bool state2, bool state3, bool state4)
{
    enableButton[0]->setToggleState(state1, juce::NotificationType::sendNotification);
    enableButton[1]->setToggleState(state2, juce::NotificationType::sendNotification);
    enableButton[2]->setToggleState(state3, juce::NotificationType::sendNotification);
    enableButton[3]->setToggleState(state4, juce::NotificationType::sendNotification);
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
            freqDividerGroup[i]->getVerticalLine().setValue(21); //why FreqDividerGroup.cpp line 56 doesn't work???
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

void Multiband::sliderValueChanged(juce::Slider *slider)
{
    for (int i = 0; i < 3; i++)
    {
        if (slider == &freqDividerGroup[sortedIndex[i]]->getVerticalLine())
        {
            dragLinesByFreq(freqDividerGroup[sortedIndex[i]]->getVerticalLine().getValue(), sortedIndex[i]);
        }
    }
}

void Multiband::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void Multiband::buttonClicked(juce::Button* button)
{
    // click closebutton, if the togglestate is false, means delete line.
    for (int i = 0; i < 3; i++)
    {
        if (button == &freqDividerGroup[i]->getCloseButton() && !freqDividerGroup[i]->getCloseButton().getToggleState())
        {
            setStatesWhenAddOrDelete(i, "delete");
            updateLines(0);
            setSoloRelatedBounds();
            processor.setLineNum();
        }
    }
}

EnableButton& Multiband::getEnableButton(const int index)
{
    return *enableButton[index];
}

void Multiband::setScale(float scale)
{
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->setScale(scale);
    }
}

void Multiband::setBandBypassStates(int index, bool state)
{
    // TODO: check delete insert is right?
    enableButton[index]->setToggleState(state, juce::NotificationType::dontSendNotification);
}
