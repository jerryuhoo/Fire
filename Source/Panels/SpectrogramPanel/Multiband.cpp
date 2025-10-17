/*
  ==============================================================================

    Multiband.cpp
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Multiband.h"
#include <algorithm>
//==============================================================================
Multiband::Multiband(FireAudioProcessor& p, state::StateComponent& sc) : processor(p), stateComponent(sc)
{
    bandUIs.resize(4);
    for (int i = 0; i < 4; ++i)
    {
        bandUIs[i].id = i;

        bandUIs[i].soloButton = std::make_unique<SoloButton>();
        addAndMakeVisible(*bandUIs[i].soloButton);
        bandUIs[i].soloButton->addListener(this);

        bandUIs[i].enableButton = std::make_unique<EnableButton>();
        addAndMakeVisible(*bandUIs[i].enableButton);
        bandUIs[i].enableButton->addListener(this);

        bandUIs[i].closeButton = std::make_unique<CloseButton>();
        addAndMakeVisible(*bandUIs[i].closeButton);
        bandUIs[i].closeButton->addListener(this);
    }

    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup>(processor, i); // set index
        addAndMakeVisible(*freqDividerGroup[i]);
        (freqDividerGroup[i]->getVerticalLine()).addListener(this);
        (freqDividerGroup[i]->getVerticalLine()).addMouseListener(this, true);
        float freqValue = freqDividerGroup[i]->getVerticalLine().getValue();
        float xPercent = static_cast<float>(transformToLog(freqValue));
        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
    }

    // Initialize parameter arrays for each band
    paramsArrays.resize(4);
    const auto& bandParams = ParameterIDAndName::getBandParameterInfo();
    for (int i = 0; i < 4; ++i)
    {
        paramsArrays[i].clear();
        for (const auto& paramInfo : bandParams)
        {
            paramsArrays[i].push_back(ParameterIDAndName::getIDString(paramInfo.idBase, i));
        }
    }

    // Initialize attachments using loops
    multiEnableAttachments.resize(4);
    multiSoloAttachments.resize(4);
    for (int i = 0; i < 4; ++i)
    {
        multiEnableAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.treeState, ParameterIDAndName::getIDString(BAND_ENABLE_ID, i), *bandUIs[i].enableButton);

        multiSoloAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.treeState, ParameterIDAndName::getIDString(BAND_SOLO_ID, i), *bandUIs[i].soloButton);
    }

    freqDividerGroupAttachments.resize(3);
    for (int i = 0; i < 3; ++i)
    {
        freqDividerGroupAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.treeState, ParameterIDAndName::getIDString(LINE_STATE_ID, i), *freqDividerGroup[i]);
    }
}

Multiband::~Multiband()
{
    for (int i = 0; i < 4; ++i)
    {
        if (bandUIs[i].soloButton)
            bandUIs[i].soloButton->removeListener(this);

        if (bandUIs[i].enableButton)
            bandUIs[i].enableButton->removeListener(this);

        if (bandUIs[i].closeButton)
            bandUIs[i].closeButton->removeListener(this);
    }

    for (int i = 0; i < 3; ++i)
    {
        if (freqDividerGroup[i])
        {
            // FreqDividerGroup 自身也是一个 Button (Toggle)
            // freqDividerGroup[i]->removeListener(this); // 如果它也加了监听器，也需要移除

            // 它内部的 VerticalLine 滑块
            freqDividerGroup[i]->getVerticalLine().removeListener(this);
        }
    }
}

void Multiband::paint(juce::Graphics& g)
{
    // send band buffer to graphs
    if (isVisible())
        processor.setHistoryArray(focusIndex);

    // draw line that will be added next
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
            if ((freqDividerGroup[i]->getToggleState() && fabs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) < limitLeft) || xPercent < limitLeft || xPercent > limitRight)
            {
                canCreate = false;
                break;
            }
        }
        if (canCreate)
        {
            g.setColour(COLOUR1.withAlpha(0.2f));
            g.drawLine(xPos, startY, xPos, endY, 2);
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
        bandUIs[i].closeButton->setVisible(false); // <--- MODIFIED
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
    setMasks(g, 0, 0, 0, 0, freqDividerGroup[0]->getX() + margin2, getHeight(), mouseX, mouseY);

    // set middle masks
    for (int i = 1; i < lineNum; i++)
    {
        int startX = freqDividerGroup[i - 1]->getX() + margin1;
        int bandWidth = freqDividerGroup[i]->getX() - freqDividerGroup[i - 1]->getX();
        setMasks(g, i, 1, startX, 0, bandWidth, getHeight(), mouseX, mouseY);
    }

    // set rightmost mask
    setMasks(g, lineNum, 0, freqDividerGroup[lineNum - 1]->getX() + margin1, 0, getWidth() - freqDividerGroup[lineNum - 1]->getX() - margin1, getHeight(), mouseX, mouseY);
}

void Multiband::resized()
{
    margin = getHeight() / 20.0f;
    size = juce::jmax(15.0f, getWidth() / 1000.0f * 15.0f);
    width = juce::jmax(5.0f, freqDividerGroup[0]->getVerticalLine().getWidth() / 2.0f);
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
}

bool Multiband::shouldSetBlackMask(int index)
{
    bool otherBandSoloIsOn = false;
    for (int i = 0; i <= lineNum; i++)
    {
        if (i == index)
            continue;
        if (bandUIs[i].soloButton->getToggleState()) // <--- MODIFIED
        {
            otherBandSoloIsOn = true;
            break;
        }
    }
    return (! bandUIs[index].soloButton->getToggleState() && otherBandSoloIsOn); // <--- MODIFIED
}

void Multiband::setParametersToAFromB(int toIndex, int fromIndex)
{
    const auto& fromArray = paramsArrays[fromIndex];
    const auto& toArray = paramsArrays[toIndex];

    for (const auto& param : processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            juce::String paramFromID = p->getParameterID();
            float paramFromValue = p->getValue();

            for (size_t i = 0; i < fromArray.size(); ++i)
            {
                if (fromArray[i] == paramFromID)
                {
                    for (const auto& paramTo : processor.getParameters())
                    {
                        if (auto* pTo = dynamic_cast<juce::AudioProcessorParameterWithID*>(paramTo))
                        {
                            if (toArray[i] == pTo->getParameterID())
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

bool Multiband::isParamInArray(juce::String paramName, const std::vector<juce::String>& paramArray)
{
    for (const auto& name : paramArray)
    {
        if (paramName == name)
        {
            return true;
        }
    }
    return false;
}

void Multiband::initParameters(int bandindex)
{
    if (bandindex < 0 || bandindex >= paramsArrays.size())
        return;

    const auto& paramArray = paramsArrays[bandindex];
    for (const auto& param : processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            if (isParamInArray(p->getParameterID(), paramArray))
            {
                p->setValueNotifyingHost(p->getDefaultValue());
            }
        }
    }
}

void Multiband::setStatesWhenAdd(int insertionIndex)
{
    // 1. Update the visual focus.
    // If a band is inserted at or before the current focus,
    // the focus index must be shifted to the right.
    if (insertionIndex <= focusIndex)
    {
        focusIndex += 1;
    }

    // 2. Determine if the user clicked on the left or right side of the band being split.
    // We use the center point between the enable and solo buttons as the reference.
    float bandCenterline = (bandUIs[insertionIndex].enableButton->getRight() + bandUIs[insertionIndex].soloButton->getX()) / 2.0f;
    bool clickedOnTheLeft = getMouseXYRelative().getX() < bandCenterline;

    // 3. Make space by shifting all bands from the insertion point onwards one step to the right.
    // We loop backwards from the end to avoid overwriting the data we need to copy.
    // After this loop, the settings of the original band at `insertionIndex` are now temporarily stored at `insertionIndex + 1`.
    for (int i = 2; i >= insertionIndex; --i)
    {
        copyBandSettings(i + 1, i);
    }
    // Also shift LFO targets for the same range of bands
    processor.shiftLfoModulationTargets(insertionIndex, 2, 1);

    // 4. Place the old and new bands correctly based on the exact user-defined logic.
    if (clickedOnTheLeft)
    {
        // USER ACTION: Clicked on the LEFT side of the band.
        // EXPECTED RESULT: The NEW band appears on the LEFT, OLD band is shifted to the RIGHT.

        // The "Make Space" step has already moved the OLD band's settings to insertionIndex + 1. This is perfect.
        // We just need to reset the band at the original insertionIndex to its default state, creating the NEW band on the LEFT.
        resetBandToDefault(insertionIndex);
        processor.clearLfoModulationForBand(insertionIndex); // Clear LFOs for the new default band
    }
    else // Clicked on the RIGHT side
    {
        // USER ACTION: Clicked on the RIGHT side of the band.
        // EXPECTED RESULT: The OLD band stays on the LEFT, NEW band appears on the RIGHT.

        // The "Make Space" step moved the OLD settings to insertionIndex + 1. We need them back.
        // So, we copy the temporarily stored settings from (insertionIndex + 1) back to the original position.
        copyBandSettings(insertionIndex, insertionIndex + 1);
        processor.shiftLfoModulationTargets(insertionIndex + 1, insertionIndex + 1, -1);

        // Now, we reset the band to the right to be a new, default band.
        resetBandToDefault(insertionIndex + 1);
        processor.clearLfoModulationForBand(insertionIndex + 1); // Clear LFOs for the new default band
    }
}

void Multiband::setStatesWhenDelete(int deletedIndex)
{
    // 1. Based on the deleted band's index, update the state of the divider line.
    // If the last band is deleted (e.g., Band 4), the line to its left (Line 3) must be disabled.
    if (deletedIndex == lineNum)
    {
        if (lineNum > 0)
            freqDividerGroup[lineNum - 1]->setToggleState(false, juce::sendNotificationSync);
    }
    else // Otherwise, just disable the divider line corresponding to the index.
    {
        freqDividerGroup[deletedIndex]->setToggleState(false, juce::sendNotificationSync);
    }

    // 2. Update the visual focus.
    if (deletedIndex < focusIndex)
    {
        focusIndex -= 1;
    }
    else if (deletedIndex == focusIndex && focusIndex == lineNum)
    {
        focusIndex -= 1;
    }

    // 3. Shift all bands that came after the deleted one forward.
    // e.g., if index 1 is deleted, copy settings from 2 to 1, and from 3 to 2.
    for (int i = deletedIndex; i < 3; ++i)
    {
        copyBandSettings(i, i + 1);
    }
    // Also shift LFO targets for the same range
    processor.shiftLfoModulationTargets(deletedIndex + 1, 3, -1);

    // 4. Clean up the state of the last band, as it is now redundant.
    resetBandToDefault(3);
    setBandState(3, { true, false }, juce::NotificationType::dontSendNotification);

    processor.clearLfoModulationForBand(3);

    // NOTE: We no longer need to call setSoloRelatedBounds() manually here,
    // as it will be handled automatically later in the call chain
    // (sortLines() -> sliderValueChanged() -> resized()).
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
            freqVector.push_back(newFreq);
        }
    }
    std::sort(freqVector.begin(), freqVector.end());
    int changeIndex = static_cast<int>(std::find(freqVector.begin(), freqVector.end(), newFreq) - freqVector.begin());
    lineNum = static_cast<int>(freqVector.size());

    for (int i = 0; i < freqVector.size(); i++)
    {
        freqDividerGroup[i]->setFreq(freqVector[i]);
        freqDividerGroup[i]->setToggleState(true, juce::sendNotificationSync);
    }
    for (auto i = freqVector.size(); i < 3; i++)
    {
        freqDividerGroup[i]->setFreq(-1);
        freqDividerGroup[i]->setToggleState(false, juce::sendNotificationSync);
    }
    setLineIndex();
    return changeIndex;
}

void Multiband::setLineIndex()
{
    // set position index, used in moveToX
    for (int i = 0; i < lineNum; i++)
    {
        freqDividerGroup[i]->getVerticalLine().setIndex(i);
    }
}

void Multiband::mouseUp(const juce::MouseEvent& e)
{
}

void Multiband::mouseDrag(const juce::MouseEvent& e)
{
    // moving lines by dragging mouse
    if (e.mods.isLeftButtonDown())
    {
        for (int i = 0; i < lineNum; i++)
        {
            if (e.eventComponent == &freqDividerGroup[i]->getVerticalLine())
            {
                float targetXPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
                dragLines(targetXPercent, i);

                sortLines();
                setLineRelatedBoundsByX();
                setSoloRelatedBounds();
            }
        }
    }
}

void Multiband::mouseDown(const juce::MouseEvent& e)
{
    if (! isDragging && e.mods.isLeftButtonDown() && e.y <= getHeight() / 5.0f) // create new lines
    {
        float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
        if (lineNum < 3)
        {
            bool canCreate = true;

            int i = 0;
            for (; i < lineNum; i++)
            {
                // can't create near existed lines
                if ((freqDividerGroup[i]->getToggleState() && fabs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) <= limitLeft) || xPercent < limitLeft || xPercent > limitRight)
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
                        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
                        int freq = static_cast<int>(transformFromLog(xPercent));
                        freqDividerGroup[i]->setFreq(freq);
                        freqDividerGroup[i]->setToggleState(true, juce::sendNotificationSync);
                        int changeIndex = sortLines();
                        if (auto* param = processor.treeState.getParameter(NUM_BANDS_ID))
                        {
                            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(lineNum + 1));
                        }
                        setStatesWhenAdd(changeIndex);
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
void Multiband::dragLines(float xPercent, int index)
{
    // moving lines by dragging mouse
    freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup);
}

void Multiband::setLineRelatedBoundsByX()
{
    // set line frequecny and position according to current x percentage of the width
    for (int i = 0; i < lineNum; i++)
    {
        if (freqDividerGroup[i]->getToggleState())
        {
            float xPercent = freqDividerGroup[i]->getVerticalLine().getXPercent();
            freqDividerGroup[i]->setBounds(xPercent * getWidth() - getWidth() / 200, 0, getWidth() / 10.0f, getHeight());
        }
    }
}

void Multiband::setSoloRelatedBounds()
{
    for (int i = 0; i < 4; i++)
    {
        if (i <= lineNum)
        {
            bandUIs[i].soloButton->setVisible(true); // <--- MODIFIED
            bandUIs[i].enableButton->setVisible(true); // <--- MODIFIED
            bandUIs[i].closeButton->setVisible(true); // <--- MODIFIED
        }
        else
        {
            bandUIs[i].soloButton->setVisible(false); // <--- MODIFIED
            bandUIs[i].enableButton->setVisible(false); // <--- MODIFIED
            bandUIs[i].closeButton->setVisible(false); // <--- MODIFIED
        }
    }
    // setBounds of soloButtons and enableButtons
    if (lineNum >= 1)
    {
        bandUIs[0].enableButton->setBounds(freqDividerGroup[0]->getX() / 2 - size, margin, size, size); // <--- MODIFIED
        bandUIs[0].soloButton->setBounds(freqDividerGroup[0]->getX() / 2 + size, margin, size, size); // <--- MODIFIED
        bandUIs[0].closeButton->setBounds(freqDividerGroup[0]->getX() / 2, getHeight() - size * 2, size, size); // <--- MODIFIED
        for (int i = 1; i < lineNum; i++)
        {
            bandUIs[i].enableButton->setBounds((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2 - size, margin, size, size); // <--- MODIFIED
            bandUIs[i].soloButton->setBounds((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2 + size, margin, size, size); // <--- MODIFIED
            bandUIs[i].closeButton->setBounds((freqDividerGroup[i]->getX() + freqDividerGroup[i - 1]->getX()) / 2, getHeight() - size * 2, size, size); // <--- MODIFIED
        }
        bandUIs[lineNum].enableButton->setBounds((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2 - size, margin, size, size); // <--- MODIFIED
        bandUIs[lineNum].soloButton->setBounds((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2 + size, margin, size, size); // <--- MODIFIED
        bandUIs[lineNum].closeButton->setBounds((freqDividerGroup[lineNum - 1]->getX() + getWidth()) / 2, getHeight() - size * 2, size, size); // <--- MODIFIED
    }
    else if (lineNum == 0)
    {
        bandUIs[0].enableButton->setBounds(getWidth() / 2 - size, margin, size, size); // <--- MODIFIED
        bandUIs[0].soloButton->setBounds(getWidth() / 2 + size, margin, size, size); // <--- MODIFIED
        bandUIs[0].closeButton->setVisible(false); // <--- MODIFIED
    }
}

int Multiband::getFocusIndex()
{
    return focusIndex;
}

void Multiband::setFocusIndex(int index)
{
    focusIndex = index;
}

void Multiband::sliderValueChanged(juce::Slider* slider)
{
    lineNum = countLines();
    setLineIndex();
    for (int i = 0; i < lineNum; i++)
    {
        if (slider == &freqDividerGroup[i]->getVerticalLine())
        {
            int freq = slider->getValue();
            freqDividerGroup[i]->setFreq(freq);
            freqDividerGroup[i]->moveToX(lineNum, freqDividerGroup[i]->getVerticalLine().getXPercent(), limitLeft, freqDividerGroup);
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

void Multiband::buttonClicked(juce::Button* button)
{
    // click closebutton and delete line.
    for (int i = 0; i < 4; i++)
    {
        if (button == bandUIs[i].closeButton.get()) // <--- MODIFIED
        {
            setStatesWhenDelete(i);
            sortLines();
            setLineRelatedBoundsByX();
            setSoloRelatedBounds();
            if (auto* param = processor.treeState.getParameter(NUM_BANDS_ID))
            {
                param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(lineNum + 1));
            }
        }
    }
}

EnableButton& Multiband::getEnableButton(const int index)
{
    return *bandUIs[index].enableButton; // <--- MODIFIED
}

void Multiband::setBandBypassStates(int index, bool state)
{
    bandUIs[index].enableButton->setToggleState(state, juce::NotificationType::dontSendNotification); // <--- MODIFIED
}

state::StateComponent& Multiband::getStateComponent()
{
    return stateComponent;
}

void Multiband::setMasks(juce::Graphics& g, int index, int lineNumLimit, int x, int y, int width, int height, int mouseX, int mouseY)
{
    // set focus mask
    if (lineNum > lineNumLimit && focusIndex == index)
    {
        // MODIFIED: Changed gradient to be top-down and relative to the band's rectangle.
        juce::ColourGradient grad(COLOUR1.withAlpha(0.5f),
                                  (float) x,
                                  (float) y,
                                  COLOUR1.withAlpha(0.0f),
                                  (float) x,
                                  (float) (y + height * 0.1f),
                                  false);
        g.setGradientFill(grad);
        g.fillRect(x, y, juce::jmax(0, width), juce::jmax(0, height));
    }

    // set mouse enter white mask
    if (! isDragging && lineNum > lineNumLimit && mouseX > x && mouseX < x + width && mouseY > y && mouseY < height)
    {
        if (focusIndex != index)
        {
            g.setColour(COLOUR_MASK_WHITE);
            g.fillRect(x, y, juce::jmax(0, width), juce::jmax(0, height));
        }
        bandUIs[index].closeButton->setVisible(true);
    }

    // set solo black mask
    if (lineNum > lineNumLimit && shouldSetBlackMask(index))
    {
        g.setColour(COLOUR_MASK_BLACK);
        g.fillRect(x, y, juce::jmax(0, width), juce::jmax(0, height));
    }
}

// Gets the enable and solo state for a specific band.
Multiband::BandState Multiband::getBandState(int bandIndex)
{
    // Ensure the index is valid.
    jassert(bandIndex >= 0 && bandIndex < bandUIs.size());
    return { bandUIs[bandIndex].enableButton->getToggleState(), bandUIs[bandIndex].soloButton->getToggleState() };
}

// Sets the enable and solo state for a specific band.
void Multiband::setBandState(int bandIndex, BandState state, juce::NotificationType notification)
{
    // Ensure the index is valid.
    jassert(bandIndex >= 0 && bandIndex < bandUIs.size());
    bandUIs[bandIndex].enableButton->setToggleState(state.isEnabled, notification);
    bandUIs[bandIndex].soloButton->setToggleState(state.isSoloed, notification);
}

// Copies the complete settings (state and parameters) from one band to another.
void Multiband::copyBandSettings(int targetIndex, int sourceIndex)
{
    // Ensure indices are valid.
    jassert(targetIndex >= 0 && targetIndex < 4);
    jassert(sourceIndex >= 0 && sourceIndex < 4);

    // 1. Copy the button states.
    setBandState(targetIndex, getBandState(sourceIndex));

    // 2. Copy all related audio parameters.
    setParametersToAFromB(targetIndex, sourceIndex);
}

// Resets a specific band to its default settings.
void Multiband::resetBandToDefault(int bandIndex)
{
    // Ensure the index is valid.
    jassert(bandIndex >= 0 && bandIndex < 4);

    // 1. Set button states to default (enabled, not soloed).
    setBandState(bandIndex, { true, false });

    // 2. Reset all related audio parameters to their default values.
    initParameters(bandIndex);
}

void Multiband::mouseMove(const juce::MouseEvent& event)
{
    // This function is called continuously ONLY when the mouse is moving over this component.
    // It's perfect for updating hover-dependent graphics like your preview line and masks.
    repaint();
}

void Multiband::mouseExit(const juce::MouseEvent& event)
{
    // This function is called once when the mouse leaves the component's bounds.
    // This ensures that any lingering hover effects (like the preview line) are erased.
    repaint();
}

void Multiband::resortAndRedrawLines()
{
    // This sequence correctly sorts the underlying parameters and
    // ensures the UI is updated to reflect the new, canonical state.
    sortLines();
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    repaint();
}