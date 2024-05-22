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
Multiband::Multiband (FireAudioProcessor& p, state::StateComponent& sc) : processor (p), stateComponent (sc)
{
    startTimerHz (60);

    soloButton[0] = std::make_unique<SoloButton>();
    addAndMakeVisible (*soloButton[0]);
    soloButton[0]->addListener (this);

    enableButton[0] = std::make_unique<EnableButton>();
    addAndMakeVisible (*enableButton[0]);
    enableButton[0]->addListener (this);

    closeButton[0] = std::make_unique<CloseButton>();
    addAndMakeVisible (*closeButton[0]);
    closeButton[0]->addListener (this);

    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup> (processor, i); // set index
        addAndMakeVisible (*freqDividerGroup[i]);
        (freqDividerGroup[i]->getVerticalLine()).addListener (this);
        (freqDividerGroup[i]->getVerticalLine()).addMouseListener (this, true);
        float freqValue = freqDividerGroup[i]->getVerticalLine().getValue();
        float xPercent = static_cast<float> (SpectrumComponent::transformToLog (freqValue));
        freqDividerGroup[i]->getVerticalLine().setXPercent (xPercent);

        soloButton[i + 1] = std::make_unique<SoloButton>();
        addAndMakeVisible (*soloButton[i + 1]);
        soloButton[i + 1]->addListener (this);

        enableButton[i + 1] = std::make_unique<EnableButton>();
        addAndMakeVisible (*enableButton[i + 1]);
        enableButton[i + 1]->addListener (this);

        closeButton[i + 1] = std::make_unique<CloseButton>();
        addAndMakeVisible (*closeButton[i + 1]);
        closeButton[i + 1]->addListener (this);
    }

    multiEnableAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_ENABLE_ID1, *enableButton[0]);
    multiEnableAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_ENABLE_ID2, *enableButton[1]);
    multiEnableAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_ENABLE_ID3, *enableButton[2]);
    multiEnableAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_ENABLE_ID4, *enableButton[3]);

    multiSoloAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_SOLO_ID1, *soloButton[0]);
    multiSoloAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_SOLO_ID2, *soloButton[1]);
    multiSoloAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_SOLO_ID3, *soloButton[2]);
    multiSoloAttachment4 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, BAND_SOLO_ID4, *soloButton[3]);

    freqDividerGroupAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, LINE_STATE_ID1, *freqDividerGroup[0]);
    freqDividerGroupAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, LINE_STATE_ID2, *freqDividerGroup[1]);
    freqDividerGroupAttachment3 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.treeState, LINE_STATE_ID3, *freqDividerGroup[2]);
}

Multiband::~Multiband()
{
}

void Multiband::paint (juce::Graphics& g)
{
    // send band buffer to graphs
    if (isVisible())
        processor.setHistoryArray (focusIndex);

    // draw line that will be added next
    float startY = 0;
    float endY = getHeight();
    auto mousePos = getMouseXYRelative();
    float xPos = mousePos.getX();
    float yPos = mousePos.getY();

    if (yPos >= startY && yPos <= startY + getHeight() / 5 && lineNum < 3)
    {
        bool canCreate = true;
        float xPercent = getMouseXYRelative().getX() / static_cast<float> (getWidth());
        for (int i = 0; i < 3; i++)
        {
            if ((freqDividerGroup[i]->getToggleState() && fabs (freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
            {
                canCreate = false;
                break;
            }
        }
        if (canCreate)
        {
            g.setColour (COLOUR1.withAlpha (0.2f));
            g.drawLine (xPos, startY, xPos, endY, 2);
        }
    }

    // set black and focus masks
    int margin2 = getWidth() / 250; // leftside of line: 1/100(line width) * 4 / 10
    int margin1 = getWidth() * 6 / 1000; // rightside of line: 1/100(line width) * 6 / 10
    int mouseX = getMouseXYRelative().getX();
    int mouseY = getMouseXYRelative().getY();

    // set closebuttons visibility to false
    for (int i = 0; i < lineNum + 1; i++)
    {
        closeButton[i]->setVisible (false);
    }

    // set dragging state
    isDragging = false;
    for (int i = 0; i < lineNum; i++)
    {
        if (freqDividerGroup[i]->getVerticalLine().isMouseOverOrDragging())
        {
            isDragging = true;
            break;
        }
    }

    // set leftmost mask
    setMasks (g, 0, 0, 0, 0, freqDividerGroup[0]->getX() + margin2, getHeight(), mouseX, mouseY);

    // set middle masks
    for (int i = 1; i < lineNum; i++)
    {
        int startX = freqDividerGroup[i - 1]->getX() + margin1;
        int bandWidth = freqDividerGroup[i]->getX() - freqDividerGroup[i - 1]->getX();
        setMasks (g, i, 1, startX, 0, bandWidth, getHeight(), mouseX, mouseY);
    }

    // set rightmost mask
    setMasks (g, lineNum, 0, freqDividerGroup[lineNum - 1]->getX() + margin1, 0, getWidth() - freqDividerGroup[lineNum - 1]->getX() - margin1, getHeight(), mouseX, mouseY);
}

void Multiband::resized()
{
    margin = getHeight() / 20.0f;
    size = juce::jmax (15.0f, getWidth() / 1000.0f * 15.0f);
    width = juce::jmax (5.0f, freqDividerGroup[0]->getVerticalLine().getWidth() / 2.0f);
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
}

bool Multiband::shouldSetBlackMask (int index)
{
    bool otherBandSoloIsOn = false;
    for (int i = 0; i <= lineNum; i++)
    {
        if (i == index)
            continue;
        if (soloButton[i]->getToggleState())
        {
            otherBandSoloIsOn = true;
            break;
        }
    }
    return (! soloButton[index]->getToggleState() && otherBandSoloIsOn);
}

void Multiband::setParametersToAFromB (int toIndex, int fromIndex)
{
    //if (toIndex == 0)
    std::unique_ptr<std::vector<juce::String>> fromArray;
    std::unique_ptr<std::vector<juce::String>> toArray;

    if (fromIndex == 0)
        fromArray = std::make_unique<std::vector<juce::String>> (paramsArray1);
    if (fromIndex == 1)
        fromArray = std::make_unique<std::vector<juce::String>> (paramsArray2);
    if (fromIndex == 2)
        fromArray = std::make_unique<std::vector<juce::String>> (paramsArray3);
    if (fromIndex == 3)
        fromArray = std::make_unique<std::vector<juce::String>> (paramsArray4);

    if (toIndex == 0)
        toArray = std::make_unique<std::vector<juce::String>> (paramsArray1);
    if (toIndex == 1)
        toArray = std::make_unique<std::vector<juce::String>> (paramsArray2);
    if (toIndex == 2)
        toArray = std::make_unique<std::vector<juce::String>> (paramsArray3);
    if (toIndex == 3)
        toArray = std::make_unique<std::vector<juce::String>> (paramsArray4);

    for (const auto& param : processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            // from array
            float paramFromValue = p->getValue();
            juce::String paramFromName = p->name;

            for (int i = 0; i < fromArray->size(); i++)
            {
                if (fromArray.get()[0][i] == paramFromName)
                {
                    // to array
                    for (const auto& paramTo : processor.getParameters())
                    {
                        if (auto* pTo = dynamic_cast<juce::AudioProcessorParameterWithID*> (paramTo))
                        {
                            juce::String paramToName = pTo->name;

                            if (toArray.get()[0][i] == paramToName)
                            {
                                pTo->setValueNotifyingHost (paramFromValue);
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

bool Multiband::isParamInArray (juce::String paramName, std::vector<juce::String> paramArray)
{
    bool isInArray = false;

    for (int i = 0; i < paramArray.size(); i++)
    {
        if (paramName == paramArray[i])
        {
            isInArray = true;
            break;
        }
    }

    return isInArray;
}

void Multiband::initParameters (int bandindex)
{
    for (const auto& param : processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            if (bandindex == 0 && isParamInArray (p->name, paramsArray1))
            {
                p->setValueNotifyingHost (p->getDefaultValue());
            }
            if (bandindex == 1 && isParamInArray (p->name, paramsArray2))
            {
                p->setValueNotifyingHost (p->getDefaultValue());
            }
            if (bandindex == 2 && isParamInArray (p->name, paramsArray3))
            {
                p->setValueNotifyingHost (p->getDefaultValue());
            }
            if (bandindex == 3 && isParamInArray (p->name, paramsArray4))
            {
                p->setValueNotifyingHost (p->getDefaultValue());
            }
        }
    }
}

void Multiband::setStatesWhenAdd (int changedIndex)
{
    // change focus index when line is added
    if (changedIndex < focusIndex || (changedIndex == focusIndex && getMouseXYRelative().getX() < (enableButton[focusIndex]->getX() + soloButton[focusIndex]->getX()) / 2.0f))
    {
        focusIndex += 1;
    }

    // change enable/solo index when line is added
    for (int i = lineNum - 1; i >= 0; --i)
    {
        // add new line, move the olds to the right
        if (i >= changedIndex)
        {
            // add when mouse click is on the left side of buttons, move i to i + 1
            if (getMouseXYRelative().getX() < (enableButton[i]->getX() + soloButton[i]->getX()) / 2.0f)
            {
                // old button
                enableButton[i + 1]->setToggleState (enableButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                // new added button
                enableButton[i]->setToggleState (true, juce::NotificationType::sendNotification);

                soloButton[i + 1]->setToggleState (soloButton[i]->getToggleState(), juce::NotificationType::sendNotification);
                soloButton[i]->setToggleState (false, juce::NotificationType::sendNotification);

                // move parameters
                setParametersToAFromB (i + 1, i);
                initParameters (i);
            }
            else // mouse click is on the right side of buttons, keep i not change, i + 1 is new
            {
                enableButton[i + 1]->setToggleState (true, juce::NotificationType::sendNotification);
                soloButton[i + 1]->setToggleState (false, juce::NotificationType::sendNotification);
                initParameters (i + 1);
            }
        }
    }
    setSoloRelatedBounds();
}

void Multiband::setStatesWhenDelete (int changedIndex)
{
    /**
     set enable buttons states, solo button states, focus band, and parameters for band processing
     */

    // delete band (not the last one), delete lines and closebuttons
    if (changedIndex != lineNum)
    {
        freqDividerGroup[changedIndex]->setToggleState (false, juce::sendNotificationSync);
    }
    else // delete last band
    {
        freqDividerGroup[lineNum - 1]->setToggleState (false, juce::sendNotificationSync);
    }

    /**
     When adding or deleting lines, this function is to keep enable and solo buttons in the right order and states.
     For example, you have [e1 | e2 | e3 | e4], If you delete line1 [e1 | e2 | <---- delete here!   e3 | e4]  we should only change e2/e3 state(in the middle),
     not affecting e1 and e4.
     */

    // change focus index when line is deleted
    if (changedIndex < focusIndex)
    {
        focusIndex -= 1;
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
        if (i > changedIndex)
        {
            enableButton[i - 1]->setToggleState (enableButton[i]->getToggleState(), juce::NotificationType::sendNotification);
            soloButton[i - 1]->setToggleState (soloButton[i]->getToggleState(), juce::NotificationType::sendNotification);
            setParametersToAFromB (i - 1, i);
        }
    }

    setSoloRelatedBounds();
}

int Multiband::countLines()
{
    int count = 0;
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getToggleState())
        {
            count++;
        }
    }
    return count;
}

int Multiband::sortLines()
{
    // clear disabled lines and sort lines by frequency
    int newFreq;
    std::vector<int> freqVector;
    for (int i = 0; i < 3; i++)
    {
        if (freqDividerGroup[i]->getToggleState())
        {
            newFreq = freqDividerGroup[i]->getFreq();
            freqVector.push_back (newFreq);
        }
    }
    std::sort (freqVector.begin(), freqVector.end());
    int changeIndex = static_cast<int> (std::find (freqVector.begin(), freqVector.end(), newFreq) - freqVector.begin());
    lineNum = static_cast<int> (freqVector.size());

    for (int i = 0; i < freqVector.size(); i++)
    {
        freqDividerGroup[i]->setFreq (freqVector[i]);
        freqDividerGroup[i]->setToggleState (true, juce::sendNotificationSync);
    }
    for (auto i = freqVector.size(); i < 3; i++)
    {
        freqDividerGroup[i]->setFreq (-1);
        freqDividerGroup[i]->setToggleState (false, juce::sendNotificationSync);
    }
    setLineIndex();
    return changeIndex;
}

void Multiband::setLineIndex()
{
    // set position index, used in moveToX
    for (int i = 0; i < lineNum; i++)
    {
        freqDividerGroup[i]->getVerticalLine().setIndex (i);
    }
}

void Multiband::mouseUp (const juce::MouseEvent& e)
{
}

void Multiband::mouseDrag (const juce::MouseEvent& e)
{
    // moving lines by dragging mouse
    if (e.mods.isLeftButtonDown())
    {
        for (int i = 0; i < lineNum; i++)
        {
            if (e.eventComponent == &freqDividerGroup[i]->getVerticalLine())
            {
                float targetXPercent = getMouseXYRelative().getX() / static_cast<float> (getWidth());
                dragLines (targetXPercent, i);
            }
        }
    }
}

void Multiband::mouseDown (const juce::MouseEvent& e)
{
    if (! isDragging && e.mods.isLeftButtonDown() && e.y <= getHeight() / 5.0f) // create new lines
    {
        float xPercent = getMouseXYRelative().getX() / static_cast<float> (getWidth());
        if (lineNum < 3)
        {
            bool canCreate = true;

            int i = 0;
            for (; i < lineNum; i++)
            {
                // can't create near existed lines
                if ((freqDividerGroup[i]->getToggleState() && fabs (freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
                {
                    canCreate = false;
                    break;
                }
            }
            if (canCreate)
            {
                for (; i < 3; i++)
                {
                    // create lines and close buttons and then set state
                    if (! freqDividerGroup[i]->getToggleState())
                    {
                        freqDividerGroup[i]->getVerticalLine().setXPercent (xPercent);
                        int freq = static_cast<int> (SpectrumComponent::transformFromLog (xPercent));
                        freqDividerGroup[i]->setFreq (freq);
                        freqDividerGroup[i]->setToggleState (true, juce::sendNotificationSync);
                        int changeIndex = sortLines();
                        processor.setLineNum();
                        setStatesWhenAdd (changeIndex);
                        break;
                    }
                }
                setLineRelatedBoundsByX(); // TODO: dont use this, only set freq
                setSoloRelatedBounds();
            }
        }
    }
    else if (! isDragging && e.mods.isLeftButtonDown() && e.y > getHeight() / 5.0f) // focus on one band
    {
        int num = lineNum;
        if (lineNum == 0)
        {
            focusIndex = 0;
            return;
        }
        else
        {
            if (e.x >= 0 && e.x < freqDividerGroup[0]->getX())
            {
                focusIndex = 0;
                return;
            }

            for (int i = 1; i < num; i++)
            {
                if (e.x >= freqDividerGroup[i - 1]->getX() && e.x < freqDividerGroup[i]->getX())
                {
                    focusIndex = i;
                    return;
                }
            }

            if (e.x >= freqDividerGroup[num - 1]->getX() && e.x <= getWidth())
            {
                focusIndex = num;
                return;
            }
        }
    }
}

// 3 ways to change frequency: 1. change freqLabel; 2. drag; 3.change Ableton slider
void Multiband::dragLines (float xPercent, int index)
{
    // moving lines by dragging mouse
    freqDividerGroup[index]->moveToX (lineNum, xPercent, limitLeft, freqDividerGroup);
}

void Multiband::setLineRelatedBoundsByX()
{
    // set line frequecny and position according to current x percentage of the width
    for (int i = 0; i < lineNum; i++)
    {
        if (freqDividerGroup[i]->getToggleState())
        {
            float xPercent = freqDividerGroup[i]->getVerticalLine().getXPercent();
            freqDividerGroup[i]->setBounds (xPercent * getWidth() - getWidth() / 200, 0, getWidth() / 10.0f, getHeight());
        }
    }
}

void Multiband::setSoloRelatedBounds()
{
    for (int i = 0; i < 4; i++)
    {
        if (i <= lineNum)
        {
            soloButton[i]->setVisible (true);
            enableButton[i]->setVisible (true);
            closeButton[i]->setVisible (true);
        }
        else
        {
            soloButton[i]->setVisible (false);
            enableButton[i]->setVisible (false);
            closeButton[i]->setVisible (false);
        }
    }
    // setBounds of soloButtons and enableButtons
    if (lineNum >= 1)
    {
        enableButton[0]->setBounds (freqDividerGroup[0]->getX() / 2 - size, margin, size, size);
        soloButton[0]->setBounds (freqDividerGroup[0]->getX() / 2 + size, margin, size, size);
        closeButton[0]->setBounds (freqDividerGroup[0]->getX() / 2, getHeight() - size * 2, size, size);
        for (int i = 1; i < lineNum; i++)
        {
            enableButton[i]->setBounds ((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2 - size, margin, size, size);
            soloButton[i]->setBounds ((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2 + size, margin, size, size);
            closeButton[i]->setBounds ((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2, getHeight() - size * 2, size, size);
        }
        enableButton[lineNum]->setBounds ((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2 - size, margin, size, size);
        soloButton[lineNum]->setBounds ((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2 + size, margin, size, size);
        closeButton[lineNum]->setBounds ((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2, getHeight() - size * 2, size, size);
    }
    else if (lineNum == 0)
    {
        enableButton[0]->setBounds (getWidth() / 2 - size, margin, size, size);
        soloButton[0]->setBounds (getWidth() / 2 + size, margin, size, size);
        closeButton[0]->setVisible (false);
    }
}

void Multiband::timerCallback()
{
    repaint();
}

int Multiband::getFocusIndex()
{
    return focusIndex;
}

void Multiband::sliderValueChanged (juce::Slider* slider)
{
    lineNum = countLines();
    setLineIndex();
    for (int i = 0; i < lineNum; i++)
    {
        if (slider == &freqDividerGroup[i]->getVerticalLine())
        {
            int freq = slider->getValue();
            freqDividerGroup[i]->setFreq (freq);
            freqDividerGroup[i]->moveToX (lineNum, freqDividerGroup[i]->getVerticalLine().getXPercent(), limitLeft, freqDividerGroup);
        }
    }
    // set focus index if focus is larger than max line num
    if (focusIndex > lineNum)
    {
        focusIndex = lineNum;
    }
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
}

void Multiband::buttonClicked (juce::Button* button)
{
    // click closebutton and delete line.
    for (int i = 0; i < 4; i++)
    {
        if (button == &*closeButton[i])
        {
            setStatesWhenDelete (i);
            sortLines();
            setLineRelatedBoundsByX();
            setSoloRelatedBounds();
            processor.setLineNum();
        }
    }
}

EnableButton& Multiband::getEnableButton (const int index)
{
    return *enableButton[index];
}

void Multiband::setScale (float scale)
{
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i]->setScale (scale);
    }
}

void Multiband::setBandBypassStates (int index, bool state)
{
    enableButton[index]->setToggleState (state, juce::NotificationType::dontSendNotification);
}

state::StateComponent& Multiband::getStateComponent()
{
    return stateComponent;
}

void Multiband::setMasks (juce::Graphics& g, int index, int lineNumLimit, int x, int y, int width, int height, int mouseX, int mouseY)
{
    // set focus mask
    if (lineNum > lineNumLimit && focusIndex == index)
    {
        juce::ColourGradient grad(COLOUR5.withAlpha(0.2f), 0, getLocalBounds().getHeight(), COLOUR1.withAlpha(0.0f), 0, getLocalBounds().getHeight() / 10.0f * 9.0f, false);
        g.setGradientFill (grad);
        g.fillRect (x, y, width, height);
    }

    // set mouse enter white mask
    if (! isDragging && lineNum > lineNumLimit && mouseX > x && mouseX < x + width && mouseY > y && mouseY < height)
    {
        if (focusIndex != index)
        {
            g.setColour (COLOUR_MASK_WHITE);
            g.fillRect (x, y, width, height);
        }
        closeButton[index]->setVisible (true);
    }

    // set solo black mask
    if (lineNum > lineNumLimit && shouldSetBlackMask (index))
    {
        g.setColour (COLOUR_MASK_BLACK);
        g.fillRect (x, y, width, height);
    }
}
