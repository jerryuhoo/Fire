/*
  ==============================================================================

    Multiband.cpp
    Created: 3 Dec 2020 4:57:48pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include "Multiband.h"
#include <algorithm>
#include <cmath>
//==============================================================================
Multiband::Multiband(FireAudioProcessor& p, state::StateComponent& sc) : processor(p), stateComponent(sc)
{
    setOpaque(false);
    bandUIs.resize(4);
    for (int i = 0; i < 4; ++i)
    {
        bandUIs[i].soloButton = std::make_unique<SoloButton>();
        addAndMakeVisible(*bandUIs[i].soloButton);
        bandUIs[i].soloButton->addListener(this);
        bandUIs[i].soloButton->addMouseListener(this, false);

        bandUIs[i].enableButton = std::make_unique<EnableButton>();
        addAndMakeVisible(*bandUIs[i].enableButton);
        bandUIs[i].enableButton->addListener(this);
        bandUIs[i].enableButton->addMouseListener(this, false);

        bandUIs[i].closeButton = std::make_unique<CloseButton>();
        addAndMakeVisible(*bandUIs[i].closeButton);
        bandUIs[i].closeButton->addListener(this);
        bandUIs[i].closeButton->addMouseListener(this, false);
    }

    // Init Vertical Lines
    for (int i = 0; i < 3; i++)
    {
        freqDividerGroup[i] = std::make_unique<FreqDividerGroup>(processor, i); // set index
        crossoverParameters[static_cast<size_t>(i)] = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(FREQ_ID, i));
        freqDividerGroup[i]->getVerticalLine().setParameterGestureCallbacks(
            [this] { beginCrossoverGesture(); },
            [this, i] { touchCrossoverParameter(i); },
            [this] { endCrossoverGesture(); });
        addAndMakeVisible(*freqDividerGroup[i]);
        (freqDividerGroup[i]->getVerticalLine()).addListener(this);
        // Listen recursively so moving between the divider, its value label and
        // the Multiband background cannot leave the band hover state stale.
        freqDividerGroup[i]->addMouseListener(this, true);
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

    // NUM_BANDS is the single topology authority. LINE_STATE remains attached
    // as a compatibility mirror, but no longer drives the presentation.
    synchroniseBandCountFromParameter();
}

Multiband::~Multiband()
{
    for (int i = 0; i < 4; ++i)
    {
        if (bandUIs[i].soloButton)
        {
            bandUIs[i].soloButton->removeMouseListener(this);
            bandUIs[i].soloButton->removeListener(this);
        }

        if (bandUIs[i].enableButton)
        {
            bandUIs[i].enableButton->removeMouseListener(this);
            bandUIs[i].enableButton->removeListener(this);
        }

        if (bandUIs[i].closeButton)
        {
            bandUIs[i].closeButton->removeMouseListener(this);
            bandUIs[i].closeButton->removeListener(this);
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        if (freqDividerGroup[i])
        {
            // FreqDividerGroup 自身也是一个 Button (Toggle)
            // freqDividerGroup[i]->removeListener(this); // 如果它也加了监听器，也需要移除

            // 它内部的 VerticalLine 滑块
            freqDividerGroup[i]->getVerticalLine().removeListener(this);
            freqDividerGroup[i]->removeMouseListener(this);
        }
    }
}

void Multiband::paint(juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // draw line that will be added next
    const float startY = 0.0f;
    const float endY = static_cast<float>(getHeight());
    const auto mousePos = getMouseXYRelative().toFloat();
    const float xPos = mousePos.getX();
    const float yPos = mousePos.getY();

    if (yPos >= startY && yPos <= startY + getHeight() / 5 && lineNum < 3)
    {
        bool canCreate = true;
        float xPercent = getMouseXYRelative().getX() / static_cast<float>(getWidth());
        for (int i = 0; i < 3; i++)
        {
            if ((freqDividerGroup[i]->getToggleState()
                 && std::abs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) < limitLeft)
                || xPercent < limitLeft
                || xPercent > limitRight)
            {
                canCreate = false;
                break;
            }
        }
        if (canCreate)
        {
            const auto physicalScale = juce::jmax(1.0f,
                g.getInternalContext().getPhysicalPixelScaleFactor());
            const auto previewX = fire::ui::pixelAligned(xPos, physicalScale);
            const auto strokeWidth = 1.0f / physicalScale;

            g.setColour(fire::ui::colours::flame.withAlpha(0.64f));
            g.fillRect(previewX - strokeWidth * 0.5f,
                       startY,
                       strokeWidth,
                       endY - startY);
            g.fillEllipse(previewX - 2.5f, startY + 3.0f, 5.0f, 5.0f);
        }
    }

    // Hit testing, focus changes and painting share the exact centre of each
    // divider, so the selected rail cannot stop short or spill into a neighbour.
    for (int band = 0; band <= lineNum; ++band)
        paintBandOverlay(g, band, getBandBounds(band), mousePos);
}

void Multiband::resized()
{
    margin = getHeight() / 20.0f;
    size = juce::jmax(15.0f, getWidth() / 1000.0f * 15.0f);
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
    if (! juce::isPositiveAndBelow(toIndex, static_cast<int>(paramsArrays.size()))
        || ! juce::isPositiveAndBelow(fromIndex, static_cast<int>(paramsArrays.size())))
        return;

    const auto& fromArray = paramsArrays[static_cast<size_t>(fromIndex)];
    const auto& toArray = paramsArrays[static_cast<size_t>(toIndex)];
    jassert(fromArray.size() == toArray.size());

    const auto parameterCount = juce::jmin(fromArray.size(), toArray.size());
    for (size_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
    {
        auto* source = processor.treeState.getParameter(fromArray[parameterIndex]);
        auto* target = processor.treeState.getParameter(toArray[parameterIndex]);
        jassert(source != nullptr && target != nullptr);
        if (source != nullptr && target != nullptr)
            target->setValueNotifyingHost(source->getValue());
    }
}

void Multiband::initParameters(int bandindex)
{
    if (! juce::isPositiveAndBelow(bandindex, static_cast<int>(paramsArrays.size())))
        return;

    const auto& paramArray = paramsArrays[static_cast<size_t>(bandindex)];
    for (const auto& parameterID : paramArray)
    {
        auto* parameter = processor.treeState.getParameter(parameterID);
        jassert(parameter != nullptr);
        if (parameter != nullptr)
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }
}

void Multiband::setStatesWhenAdd(int insertionIndex, bool newBandIsOnLeft)
{
    // lineNum already includes the divider that was just inserted.  Therefore
    // the highest previously active band index is lineNum - 1.
    const int oldLastBandIndex = lineNum - 1;
    if (! juce::isPositiveAndBelow(insertionIndex, lineNum)
        || ! juce::isPositiveAndBelow(oldLastBandIndex, 4))
    {
        jassertfalse;
        return;
    }

    // Make space by shifting active bands from the insertion point one step to
    // the right.  Iterating backwards avoids overwriting a source band.
    // We loop backwards from the end to avoid overwriting the data we need to copy.
    // After this loop, the settings of the original band at `insertionIndex` are now temporarily stored at `insertionIndex + 1`.
    for (int i = oldLastBandIndex; i >= insertionIndex; --i)
        copyBandSettings(i + 1, i);

    // Also shift LFO targets for the same range of bands
    processor.shiftLfoModulationTargets(insertionIndex,
                                        oldLastBandIndex,
                                        1,
                                        false);

    // Preserve the logical identity of the old band.  If the new region is on
    // the left the old settings stay in insertionIndex + 1; otherwise restore
    // them to insertionIndex and create the default band on the right.
    if (newBandIsOnLeft)
    {
        // USER ACTION: Clicked on the LEFT side of the band.
        // EXPECTED RESULT: The NEW band appears on the LEFT, OLD band is shifted to the RIGHT.

        // The "Make Space" step has already moved the OLD band's settings to insertionIndex + 1. This is perfect.
        // We just need to reset the band at the original insertionIndex to its default state, creating the NEW band on the LEFT.
        resetBandToDefault(insertionIndex);
        processor.clearLfoModulationForBand(insertionIndex, false); // Clear LFOs for the new default band
    }
    else // Clicked on the RIGHT side
    {
        // USER ACTION: Clicked on the RIGHT side of the band.
        // EXPECTED RESULT: The OLD band stays on the LEFT, NEW band appears on the RIGHT.

        // The "Make Space" step moved the OLD settings to insertionIndex + 1. We need them back.
        // So, we copy the temporarily stored settings from (insertionIndex + 1) back to the original position.
        copyBandSettings(insertionIndex, insertionIndex + 1);
        processor.shiftLfoModulationTargets(insertionIndex + 1,
                                            insertionIndex + 1,
                                            -1,
                                            false);

        // Now, we reset the band to the right to be a new, default band.
        resetBandToDefault(insertionIndex + 1);
        processor.clearLfoModulationForBand(insertionIndex + 1, false); // Clear LFOs for the new default band
    }
}

void Multiband::setStatesWhenDelete(int deletedIndex)
{
    if (lineNum <= 0 || ! juce::isPositiveAndBelow(deletedIndex, lineNum + 1))
        return;

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

    // Remove routings owned by the deleted logical band before moving later
    // routings into its index.  Otherwise both the deleted band and its
    // successor can target the same newly-visible parameter.
    processor.clearLfoModulationForBand(deletedIndex, false);

    // Shift all active bands that came after the deleted one forward.
    // e.g., if index 1 is deleted, copy settings from 2 to 1, and from 3 to 2.
    const int oldLastBandIndex = lineNum;
    for (int i = deletedIndex; i < oldLastBandIndex; ++i)
        copyBandSettings(i, i + 1);

    // Also shift LFO targets for the same range
    processor.shiftLfoModulationTargets(deletedIndex + 1,
                                        oldLastBandIndex,
                                        -1,
                                        false);

    // Clean up the slot that has just become inactive.  Resetting band 3
    // unconditionally left stale state behind when deleting from a two- or
    // three-band layout.
    resetBandToDefault(oldLastBandIndex);
    processor.clearLfoModulationForBand(oldLastBandIndex, false);

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
    return sortLinesInternal(true);
}

int Multiband::sortLinesInternal(bool notifyFocusChange)
{
    if (isCanonicalisingLines)
        return juce::jlimit(0, juce::jmax(0, lineNum - 1), 0);

    const juce::ScopedValueSetter<bool> canonicalising(isCanonicalisingLines, true);

    // clear disabled lines and sort lines by frequency
    int newFreq = -1;
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
    const auto insertedPosition = newFreq >= 0
                                    ? std::find(freqVector.begin(), freqVector.end(), newFreq)
                                    : freqVector.end();
    const int changeIndex = insertedPosition != freqVector.end()
                              ? static_cast<int>(std::distance(freqVector.begin(), insertedPosition))
                              : 0;
    lineNum = static_cast<int>(freqVector.size());

    for (int i = 0; i < static_cast<int>(freqVector.size()); ++i)
    {
        freqDividerGroup[i]->setFreq(freqVector[static_cast<size_t>(i)]);
        freqDividerGroup[i]->setToggleState(true, juce::sendNotificationSync);
        freqDividerGroup[i]->setVisible(true);
    }
    for (int i = static_cast<int>(freqVector.size()); i < 3; ++i)
    {
        freqDividerGroup[i]->setFreq(-1);
        freqDividerGroup[i]->setToggleState(false, juce::sendNotificationSync);
        freqDividerGroup[i]->setVisible(false);
    }
    setLineIndex();

    updateFocusIndex(focusIndex, notifyFocusChange);
    return changeIndex;
}

std::array<float, 3> Multiband::getCanonicalCrossoverFrequencies(int requestedBandCount) const
{
    constexpr std::array<float, 3> defaults { 200.0f, 1000.0f, 5000.0f };
    std::array<float, 3> frequencies = defaults;
    const int requestedCrossovers = juce::jlimit(0, 3, requestedBandCount - 1);

    bool activeFrequenciesAreValid = true;
    for (int divider = 0; divider < 3; ++divider)
    {
        const auto* value = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(FREQ_ID, divider));
        const float frequency = value != nullptr ? value->load(std::memory_order_relaxed)
                                                 : defaults[static_cast<size_t>(divider)];
        frequencies[static_cast<size_t>(divider)] = frequency;

        if (divider < requestedCrossovers)
        {
            activeFrequenciesAreValid = activeFrequenciesAreValid
                                         && std::isfinite(frequency)
                                         && frequency >= 40.0f
                                         && frequency <= 10024.0f
                                         && (divider == 0
                                             || frequency
                                                    > frequencies[static_cast<size_t>(divider - 1)]);
        }
    }

    if (! activeFrequenciesAreValid)
        for (int divider = 0; divider < requestedCrossovers; ++divider)
            frequencies[static_cast<size_t>(divider)] = defaults[static_cast<size_t>(divider)];

    return frequencies;
}

void Multiband::setDividerState(int dividerIndex,
                                bool enabled,
                                float frequency,
                                juce::NotificationType parameterNotification)
{
    if (! juce::isPositiveAndBelow(dividerIndex, 3))
        return;

    auto& divider = *freqDividerGroup[static_cast<size_t>(dividerIndex)];
    if (enabled)
        divider.setFreq(frequency, juce::dontSendNotification);
    else
        divider.setFreq(-1, juce::dontSendNotification);

    divider.setToggleState(enabled, parameterNotification);
    divider.setVisible(enabled);
}

void Multiband::applyAuthoritativeBandCount(int requestedBandCount,
                                            bool forceFocusNotification,
                                            bool publishCanonicalParameters)
{
    const int newBandCount = juce::jlimit(1, 4, requestedBandCount);
    const int newLineCount = newBandCount - 1;
    const auto frequencies = getCanonicalCrossoverFrequencies(newBandCount);

    const juce::ScopedValueSetter<bool> canonicalising(isCanonicalisingLines, true);
    for (int divider = 0; divider < 3; ++divider)
    {
        const bool enabled = divider < newLineCount;
        setDividerState(divider,
                        enabled,
                        frequencies[static_cast<size_t>(divider)],
                        juce::dontSendNotification);

        if (! publishCanonicalParameters)
            continue;

        // A host can expose NUM_BANDS independently from the legacy divider
        // parameters. Publish the complete canonical tuple before the view is
        // considered synchronised; otherwise one visible fallback divider can
        // move while DSP keeps rejecting another hidden 21 Hz sentinel.
        if (enabled)
        {
            if (auto* frequencyParameter = processor.treeState.getParameter(
                    ParameterIDAndName::getIDString(FREQ_ID, divider)))
            {
                const float normalised = frequencyParameter->getNormalisableRange().convertTo0to1(
                    frequencies[static_cast<size_t>(divider)]);
                if (! juce::approximatelyEqual(frequencyParameter->getValue(), normalised))
                    frequencyParameter->setValueNotifyingHost(normalised);
            }
        }

        if (auto* lineStateParameter = processor.treeState.getParameter(
                ParameterIDAndName::getIDString(LINE_STATE_ID, divider)))
        {
            const float normalised = enabled ? 1.0f : 0.0f;
            if (! juce::approximatelyEqual(lineStateParameter->getValue(), normalised))
                lineStateParameter->setValueNotifyingHost(normalised);
        }
    }

    lineNum = newLineCount;
    setLineIndex();
    updateFocusIndex(focusIndex, forceFocusNotification);
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    refreshHoveredBandFromMouse();
    repaint();
}

void Multiband::synchroniseBandCountFromParameter()
{
    const auto* bandCount = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    const int requestedBandCount = bandCount != nullptr
                                     ? juce::roundToInt(bandCount->load(std::memory_order_relaxed))
                                     : 1;
    const int requestedLineCount = juce::jlimit(0, 3, requestedBandCount - 1);
    const auto canonicalFrequencies = getCanonicalCrossoverFrequencies(requestedBandCount);

    bool presentationIsCanonical = lineNum == requestedLineCount;
    for (int divider = 0; divider < 3; ++divider)
    {
        const bool shouldBeEnabled = divider < requestedLineCount;
        presentationIsCanonical = presentationIsCanonical
                                   && freqDividerGroup[static_cast<size_t>(divider)]->getToggleState()
                                          == shouldBeEnabled;

        const auto* lineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, divider));
        presentationIsCanonical = presentationIsCanonical
                                   && lineState != nullptr
                                   && (lineState->load(std::memory_order_relaxed) > 0.5f)
                                          == shouldBeEnabled;

        if (shouldBeEnabled)
        {
            const auto* frequency = processor.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(FREQ_ID, divider));
            presentationIsCanonical = presentationIsCanonical
                                       && frequency != nullptr
                                       && juce::approximatelyEqual(
                                           frequency->load(std::memory_order_relaxed),
                                           canonicalFrequencies[static_cast<size_t>(divider)]);
        }
    }

    if (! presentationIsCanonical)
    {
        {
            processor.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [this]
            {
                processor.requestMultibandTopologyReset();
            } };
            applyAuthoritativeBandCount(requestedBandCount, true, true);
        }

        // Parameter callbacks fired while the generation was odd can only
        // observe the immutable pre-edit snapshot. Publish one post-commit
        // notification so hosts persist the canonical NUM_BANDS/FREQ/LINE
        // tuple instead of the stale presentation generation.
        processor.lfoDataHasChanged();
    }
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
    if (dynamic_cast<juce::Button*>(e.eventComponent) != nullptr)
        return;

    isDragging = false;
    const auto localEvent = e.getEventRelativeTo(this);
    updateHoveredBand(localEvent.getPosition(), getLocalBounds().contains(localEvent.getPosition()));
    repaint();
}

void Multiband::mouseDrag(const juce::MouseEvent& e)
{
    if (dynamic_cast<juce::Button*>(e.eventComponent) != nullptr)
        return;

    if (getWidth() <= 0)
        return;

    // moving lines by dragging mouse
    if (e.mods.isLeftButtonDown())
    {
        const int dividerIndex = getDividerIndexForEvent(e);
        if (! juce::isPositiveAndBelow(dividerIndex, lineNum))
            return;

        isDragging = true;
        hoveredBandIndex = -1;
        updateCloseButtonVisibility();

        const auto localEvent = e.getEventRelativeTo(this);
        const float targetXPercent = localEvent.position.x / static_cast<float>(getWidth());
        dragLines(targetXPercent, dividerIndex);

        sortLinesInternal(false);
        setLineRelatedBoundsByX();
        setSoloRelatedBounds();
        repaint();
    }
}

void Multiband::mouseDown(const juce::MouseEvent& e)
{
    if (dynamic_cast<juce::Button*>(e.eventComponent) != nullptr)
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const int dividerIndex = getDividerIndexForEvent(e);
    if (dividerIndex < 0 && isEventFromDividerGroup(e))
        return;

    isDragging = dividerIndex >= 0;

    const auto localEvent = e.getEventRelativeTo(this);

    if (! isDragging && e.mods.isLeftButtonDown() && localEvent.y <= getHeight() / 5.0f) // create new lines
    {
        const float xPercent = localEvent.position.x / static_cast<float>(getWidth());
        if (lineNum < 3)
        {
            bool canCreate = xPercent >= limitLeft && xPercent <= limitRight;

            int i = 0;
            for (; canCreate && i < lineNum; ++i)
            {
                // can't create near existed lines
                if (freqDividerGroup[i]->getToggleState()
                    && std::abs(freqDividerGroup[i]->getVerticalLine().getXPercent() - xPercent) <= limitLeft)
                    canCreate = false;
            }
            if (canCreate)
            {
                const int splitBandIndex = getBandIndexAtX(localEvent.x);
                if (! juce::isPositiveAndBelow(splitBandIndex, lineNum + 1))
                    return;

                const bool newBandIsOnLeft = localEvent.position.x
                                             < getBandBounds(splitBandIndex).getCentreX();
                bool bandWasAdded = false;
                for (; i < 3; i++)
                {
                    // create lines and close buttons and then set state
                    if (! freqDividerGroup[i]->getToggleState())
                    {
                        processor.beginMultibandTopologyEdit();
                        const juce::ScopeGuard finishTopologyEdit { [this]
                        {
                            processor.requestMultibandTopologyReset();
                        } };
                        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
                        int freq = static_cast<int>(transformFromLog(xPercent));
                        freqDividerGroup[i]->setFreq(freq);
                        freqDividerGroup[i]->setToggleState(true, juce::sendNotificationSync);
                        const int changeIndex = sortLinesInternal(false);
                        setStatesWhenAdd(changeIndex, newBandIsOnLeft);

                        int focusAfterInsert = focusIndex;
                        if (focusIndex > changeIndex
                            || (focusIndex == changeIndex && newBandIsOnLeft))
                            ++focusAfterInsert;
                        updateFocusIndex(focusAfterInsert, true);

                        // Publish the larger DSP band count only after every
                        // destination parameter and attachment is coherent.
                        if (auto* param = processor.treeState.getParameter(NUM_BANDS_ID))
                            param->setValueNotifyingHost(
                                param->getNormalisableRange().convertTo0to1(lineNum + 1));

                        // Publish only after every parameter, routing,
                        // frequency and the final band count are coherent.
                        // This also catches add/delete pairs that happen
                        // between audio blocks and finish on the same count.
                        bandWasAdded = true;
                        break;
                    }
                }
                if (bandWasAdded)
                    processor.lfoDataHasChanged();
                setLineRelatedBoundsByX(); // TODO: dont use this, only set freq
                setSoloRelatedBounds();
                refreshHoveredBandFromMouse();
                repaint();
            }
        }
    }
    else if (! isDragging && e.mods.isLeftButtonDown() && localEvent.y > getHeight() / 5.0f) // focus on one band
    {
        const int selectedBand = getBandIndexAtX(localEvent.x);
        if (selectedBand >= 0)
            setFocusIndex(selectedBand);
    }
}

// 3 ways to change frequency: 1. change freqLabel; 2. drag; 3.change Ableton slider
void Multiband::dragLines(float xPercent, int index)
{
    // moving lines by dragging mouse
    if (juce::isPositiveAndBelow(index, lineNum))
        freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup);
}

void Multiband::beginCrossoverGesture()
{
    if (crossoverGestureDepth++ == 0)
        crossoverParametersTouched.fill(false);
}

void Multiband::touchCrossoverParameter(int dividerIndex)
{
    if (crossoverGestureDepth <= 0 || ! juce::isPositiveAndBelow(dividerIndex, 3))
        return;

    const auto arrayIndex = static_cast<size_t>(dividerIndex);
    if (crossoverParametersTouched[arrayIndex])
        return;

    if (auto* parameter = crossoverParameters[arrayIndex])
    {
        parameter->beginChangeGesture();
        crossoverParametersTouched[arrayIndex] = true;
    }
}

void Multiband::endCrossoverGesture()
{
    if (crossoverGestureDepth <= 0 || --crossoverGestureDepth > 0)
        return;

    for (size_t dividerIndex = 0; dividerIndex < crossoverParameters.size(); ++dividerIndex)
    {
        if (crossoverParametersTouched[dividerIndex])
            if (auto* parameter = crossoverParameters[dividerIndex])
                parameter->endChangeGesture();
    }

    crossoverParametersTouched.fill(false);
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

    // Divider groups deliberately overlap neighbouring bands to provide a
    // generous drag target.  Keep the compact band controls above those
    // transparent children so the close button always receives the click.
    for (int i = 0; i <= lineNum; ++i)
    {
        bandUIs[static_cast<size_t>(i)].closeButton->toFront(false);
        bandUIs[static_cast<size_t>(i)].soloButton->toFront(false);
        bandUIs[static_cast<size_t>(i)].enableButton->toFront(false);
    }
}

void Multiband::setSoloRelatedBounds()
{
    const float closeHitSize = juce::jmax(size, 24.0f);
    const auto placeBandButtons = [this, closeHitSize](int bandIndex, float centreX)
    {
        bandUIs[static_cast<size_t>(bandIndex)].enableButton->setBounds(
            juce::roundToInt(centreX - size * 1.5f),
            juce::roundToInt(margin),
            juce::roundToInt(size),
            juce::roundToInt(size));
        bandUIs[static_cast<size_t>(bandIndex)].soloButton->setBounds(
            juce::roundToInt(centreX + size * 0.5f),
            juce::roundToInt(margin),
            juce::roundToInt(size),
            juce::roundToInt(size));
        bandUIs[static_cast<size_t>(bandIndex)].closeButton->setBounds(
            juce::roundToInt(centreX - closeHitSize * 0.5f),
            juce::roundToInt(static_cast<float>(getHeight()) - closeHitSize - size * 0.5f),
            juce::roundToInt(closeHitSize),
            juce::roundToInt(closeHitSize));
    };

    for (int i = 0; i < 4; i++)
    {
        if (i <= lineNum)
        {
            bandUIs[i].soloButton->setVisible(true); // <--- MODIFIED
            bandUIs[i].enableButton->setVisible(true); // <--- MODIFIED
            bandUIs[i].closeButton->setVisible(false);
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
        placeBandButtons(0, getBandBounds(0).getCentreX());
        for (int i = 1; i < lineNum; i++)
            placeBandButtons(i, getBandBounds(i).getCentreX());
        placeBandButtons(lineNum, getBandBounds(lineNum).getCentreX());
    }
    else if (lineNum == 0)
    {
        placeBandButtons(0, static_cast<float>(getWidth()) * 0.5f);
        bandUIs[0].closeButton->setVisible(false); // <--- MODIFIED
    }

    for (int i = 0; i <= lineNum; ++i)
    {
        bandUIs[static_cast<size_t>(i)].closeButton->toFront(false);
        bandUIs[static_cast<size_t>(i)].soloButton->toFront(false);
        bandUIs[static_cast<size_t>(i)].enableButton->toFront(false);
    }

    updateCloseButtonVisibility();
}

void Multiband::setFocusChangedCallback(FocusChangedCallback callback)
{
    focusChangedCallback = std::move(callback);
}

int Multiband::getFocusIndex() const noexcept
{
    return focusIndex;
}

void Multiband::setFocusIndex(int index)
{
    updateFocusIndex(index, false);
}

bool Multiband::updateFocusIndex(int requestedIndex, bool forceNotification)
{
    const int newFocus = juce::jlimit(0, lineNum, requestedIndex);
    const bool didChange = newFocus != focusIndex;
    focusIndex = newFocus;

    if (didChange || forceNotification)
        notifyFocusChanged();

    if (didChange)
        repaint();

    return didChange;
}

void Multiband::notifyFocusChanged()
{
    if (isShowing())
        processor.setHistoryArray(focusIndex);

    if (focusChangedCallback)
        focusChangedCallback(focusIndex);
}

void Multiband::sliderValueChanged(juce::Slider* slider)
{
    if (isCanonicalisingLines)
        return;

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
    // Parameters and presets can reduce the band count asynchronously.  Route
    // the clamp through the same focus notification path as direct clicks.
    updateFocusIndex(focusIndex, false);
    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    refreshHoveredBandFromMouse();
}

void Multiband::buttonClicked(juce::Button* button)
{
    // click closebutton and delete line.
    bool bandWasDeleted = false;
    for (int i = 0; i <= lineNum; ++i)
    {
        if (button == bandUIs[i].closeButton.get()) // <--- MODIFIED
        {
            const int deletedIndex = i;
            const int oldFocus = focusIndex;
            processor.beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [this]
            {
                processor.requestMultibandTopologyReset();
            } };
            setStatesWhenDelete(i);
            sortLinesInternal(false);

            int focusAfterDelete = oldFocus;
            if (deletedIndex < oldFocus
                || (deletedIndex == oldFocus && oldFocus > lineNum))
                --focusAfterDelete;
            updateFocusIndex(focusAfterDelete, true);

            setLineRelatedBoundsByX();
            setSoloRelatedBounds();
            if (auto* param = processor.treeState.getParameter(NUM_BANDS_ID))
            {
                param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(lineNum + 1));
            }
            refreshHoveredBandFromMouse();
            bandWasDeleted = true;
            break;
        }
    }

    if (bandWasDeleted)
        processor.lfoDataHasChanged();

    repaint();
}

EnableButton& Multiband::getEnableButton(const int index)
{
    return *bandUIs[index].enableButton; // <--- MODIFIED
}

void Multiband::setBandBypassStates(int index, bool state)
{
    if (juce::isPositiveAndBelow(index, static_cast<int>(bandUIs.size())))
    {
        bandUIs[index].enableButton->setToggleState(state, juce::NotificationType::dontSendNotification); // <--- MODIFIED
        repaint();
    }
}

state::StateComponent& Multiband::getStateComponent()
{
    return stateComponent;
}

void Multiband::paintBandOverlay(juce::Graphics& g,
                                 int index,
                                 juce::Rectangle<float> area,
                                 juce::Point<float> mousePosition)
{
    if (area.isEmpty() || ! juce::isPositiveAndBelow(index, lineNum + 1))
        return;

    const bool selected = focusIndex == index;
    const bool hovered = ! isDragging && area.contains(mousePosition);

    if (selected)
    {
        juce::ColourGradient grad(fire::ui::colours::ember.withAlpha(0.10f),
                                  area.getX(), area.getY(),
                                  fire::ui::colours::ember.withAlpha(0.0f),
                                  area.getX(), area.getY() + area.getHeight() * 0.20f,
                                  false);
        g.setGradientFill(grad);
        g.fillRect(area);
    }

    if (hovered && ! selected)
    {
        g.setColour(fire::ui::colours::textPrimary.withAlpha(0.032f));
        g.fillRect(area);
    }

    if (shouldSetBlackMask(index))
    {
        g.setColour(fire::ui::colours::canvas.withAlpha(0.64f));
        g.fillRect(area);
    }

    if (! bandUIs[static_cast<size_t>(index)].enableButton->getToggleState())
    {
        g.setColour(fire::ui::colours::canvas.withAlpha(0.36f));
        g.fillRect(area);
    }

    if (selected)
    {
        // Keep the indicator at two physical pixels on standard and HiDPI
        // displays. Outward edge rounding closes any subpixel seam against a
        // divider or the analyser edge.
        const auto physicalScale = juce::jmax(1.0f,
            g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto left = std::floor(area.getX() * physicalScale) / physicalScale;
        const auto right = std::ceil(area.getRight() * physicalScale) / physicalScale;
        const auto railHeight = 2.0f / physicalScale;
        const juce::Rectangle<float> rail(left,
                                           area.getY(),
                                           juce::jmax(0.0f, right - left),
                                           railHeight);

        juce::ColourGradient railGradient(fire::ui::colours::ember.withAlpha(0.92f),
                                           rail.getX(), rail.getY(),
                                           fire::ui::colours::ember.withAlpha(0.92f),
                                           rail.getRight(), rail.getY(), false);
        railGradient.addColour(0.5, fire::ui::colours::flame.withAlpha(0.98f));
        g.setGradientFill(railGradient);
        g.fillRect(rail);
    }
}

float Multiband::getDividerX(int index) const
{
    if (! juce::isPositiveAndBelow(index, lineNum))
        return index < 0 ? 0.0f : static_cast<float>(getWidth());

    const auto& divider = freqDividerGroup[index]->getVerticalLine();
    const auto centreInGroup = divider.getBounds().toFloat().getCentreX();
    return juce::jlimit(0.0f,
                        static_cast<float>(getWidth()),
                        static_cast<float>(freqDividerGroup[index]->getX()) + centreInGroup);
}

juce::Rectangle<float> Multiband::getBandBounds(int index) const
{
    if (! juce::isPositiveAndBelow(index, lineNum + 1))
        return {};

    const auto left = index == 0 ? 0.0f : getDividerX(index - 1);
    const auto right = index == lineNum ? static_cast<float>(getWidth())
                                        : getDividerX(index);
    return juce::Rectangle<float>::leftTopRightBottom(left,
                                                       0.0f,
                                                       juce::jmax(left, right),
                                                       static_cast<float>(getHeight()));
}

// Gets the enable and solo state for a specific band.
Multiband::BandState Multiband::getBandState(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, static_cast<int>(bandUIs.size())))
    {
        jassertfalse;
        return { true, false };
    }

    return { bandUIs[bandIndex].enableButton->getToggleState(), bandUIs[bandIndex].soloButton->getToggleState() };
}

// Sets the enable and solo state for a specific band.
void Multiband::setBandState(int bandIndex, BandState state, juce::NotificationType notification)
{
    if (! juce::isPositiveAndBelow(bandIndex, static_cast<int>(bandUIs.size())))
    {
        jassertfalse;
        return;
    }

    bandUIs[bandIndex].enableButton->setToggleState(state.isEnabled, notification);
    bandUIs[bandIndex].soloButton->setToggleState(state.isSoloed, notification);
}

// Copies the complete settings (state and parameters) from one band to another.
void Multiband::copyBandSettings(int targetIndex, int sourceIndex)
{
    if (! juce::isPositiveAndBelow(targetIndex, 4)
        || ! juce::isPositiveAndBelow(sourceIndex, 4))
    {
        jassertfalse;
        return;
    }

    // 1. Copy the button states.
    setBandState(targetIndex, getBandState(sourceIndex));

    // 2. Copy all related audio parameters.
    setParametersToAFromB(targetIndex, sourceIndex);
}

// Resets a specific band to its default settings.
void Multiband::resetBandToDefault(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, 4))
    {
        jassertfalse;
        return;
    }

    // 1. Set button states to default (enabled, not soloed).
    setBandState(bandIndex, { true, false });

    // 2. Reset all related audio parameters to their default values.
    initParameters(bandIndex);
}

void Multiband::mouseMove(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    if (! event.mods.isLeftButtonDown())
        isDragging = false;

    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
    if (lineNum < 3
        && relativeEvent.y >= 0
        && relativeEvent.y <= getHeight() / 5)
        repaint();
}

void Multiband::mouseEnter(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
}

void Multiband::mouseExit(const juce::MouseEvent& event)
{
    const auto relativeEvent = event.getEventRelativeTo(this);
    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
}

void Multiband::visibilityChanged()
{
    if (isShowing())
        processor.setHistoryArray(focusIndex);
}

int Multiband::getBandIndexAtX(int x) const
{
    if (x < 0 || x > getWidth())
        return -1;

    for (int i = 0; i < lineNum; ++i)
        if (static_cast<float>(x) < getDividerX(i))
            return i;

    return lineNum;
}

int Multiband::getDividerIndexForEvent(const juce::MouseEvent& event) const
{
    for (int i = 0; i < lineNum; ++i)
    {
        auto* component = event.eventComponent;
        while (component != nullptr && component != this)
        {
            if (component == &freqDividerGroup[i]->getVerticalLine())
                return i;
            component = component->getParentComponent();
        }
    }

    return -1;
}

bool Multiband::isEventFromDividerGroup(const juce::MouseEvent& event) const
{
    auto* component = event.eventComponent;
    while (component != nullptr && component != this)
    {
        for (const auto& group : freqDividerGroup)
            if (component == group.get())
                return true;

        component = component->getParentComponent();
    }

    return false;
}

void Multiband::updateHoveredBand(juce::Point<int> localPosition, bool pointerIsInside)
{
    const int newHoveredBand = pointerIsInside && ! isDragging
                                 ? getBandIndexAtX(localPosition.x)
                                 : -1;
    if (newHoveredBand == hoveredBandIndex)
        return;

    hoveredBandIndex = newHoveredBand;
    updateCloseButtonVisibility();
    repaint();
}

void Multiband::refreshHoveredBandFromMouse()
{
    const auto mousePosition = getMouseXYRelative();
    updateHoveredBand(mousePosition, getLocalBounds().contains(mousePosition));
}

void Multiband::updateCloseButtonVisibility()
{
    for (int i = 0; i < static_cast<int>(bandUIs.size()); ++i)
    {
        const bool shouldShow = lineNum > 0
                             && i <= lineNum
                             && i == hoveredBandIndex
                             && ! isDragging;
        auto& closeButton = *bandUIs[static_cast<size_t>(i)].closeButton;
        closeButton.setVisible(shouldShow);
        if (shouldShow)
            closeButton.toFront(false);
    }
}

void Multiband::resortAndRedrawLines()
{
    const auto* bandCount = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    applyAuthoritativeBandCount(
        bandCount != nullptr ? juce::roundToInt(bandCount->load(std::memory_order_relaxed)) : 1,
        true,
        false);
}
