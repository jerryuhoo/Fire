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

namespace
{
bool isPrimaryPointerDown(const juce::MouseEvent& event) noexcept
{
    return event.mods.isLeftButtonDown()
        && ! event.mods.isPopupMenu()
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown();
}
} // namespace

struct Multiband::CrossoverGestureSession
{
    explicit CrossoverGestureSession(
        std::array<juce::RangedAudioParameter*, 3> parametersToUse)
        : parameters(parametersToUse)
    {
    }

    ~CrossoverGestureSession()
    {
        finish();
    }

    bool touch(size_t dividerIndex)
    {
        if (dividerIndex >= parameters.size())
            return false;

        if (touched[dividerIndex])
            return true;

        auto* parameter = parameters[dividerIndex];
        if (parameter == nullptr)
            return false;

        // Mark first: beginChangeGesture may synchronously destroy the editor
        // and release its owning reference to this session.
        touched[dividerIndex] = true;
        parameter->beginChangeGesture();
        return true;
    }

private:
    void finish()
    {
        const auto parametersToEnd = parameters;
        const auto touchedToEnd = touched;
        parameters.fill(nullptr);
        touched.fill(false);

        // Use only processor-owned parameter pointers from here on. Any one
        // end notification may synchronously destroy the editor, but every
        // gesture that began still receives its matching end notification.
        for (size_t dividerIndex = 0; dividerIndex < parametersToEnd.size(); ++dividerIndex)
            if (touchedToEnd[dividerIndex])
                if (auto* parameter = parametersToEnd[dividerIndex])
                    parameter->endChangeGesture();
    }

    std::array<juce::RangedAudioParameter*, 3> parameters {};
    std::array<bool, 3> touched {};
};

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
        bandUIs[i].closeButton->setPresented(false, false);
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
            [this, i] { return touchCrossoverParameter(i); },
            [this] { endCrossoverGesture(); });
        freqDividerGroup[i]->getVerticalLine().setPointerGestureAdmissionCallback(
            [this, i](juce::MouseInputSource::InputSourceType sourceType,
                      int sourceIndex)
            {
                return admitDividerPointerGesture(i, sourceType, sourceIndex);
            });
        freqDividerGroup[i]->setFrequencyEditCallback([this, i](float xPercent)
        {
            dragLines(xPercent, i);
        });
        freqDividerGroup[i]->setHiddenCallback([this, i]
        {
            handleDividerHidden(i);
        });
        addAndMakeVisible(*freqDividerGroup[i]);
        (freqDividerGroup[i]->getVerticalLine()).addListener(this);
        // Listen recursively so moving between the divider, its value label and
        // the Multiband background cannot leave the band hover state stale.
        freqDividerGroup[i]->addMouseListener(this, true);
        float freqValue = freqDividerGroup[i]->getVerticalLine().getValue();
        float xPercent = static_cast<float>(transformToLog(freqValue));
        freqDividerGroup[i]->getVerticalLine().setXPercent(xPercent);
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

void Multiband::animationTick(float deltaSeconds)
{
    if (! isShowing())
        return;

    for (const auto& dividerGroup : freqDividerGroup)
    {
        if (dividerGroup == nullptr)
            continue;

        auto& divider = dividerGroup->getVerticalLine();
        if (divider.advanceAnimation(deltaSeconds))
            divider.repaint();

        dividerGroup->advanceAnimation(deltaSeconds);
    }

    for (auto& bandUI : bandUIs)
        if (bandUI.closeButton != nullptr
            && bandUI.closeButton->advanceAnimation(deltaSeconds))
            bandUI.closeButton->repaint();
}

void Multiband::dismissTransientUi()
{
    clearPrimaryPointerState();
    hoveredBandIndex = -1;
    juce::Component::SafePointer<Multiband> safeThis(this);

    for (const auto& dividerGroup : freqDividerGroup)
    {
        if (dividerGroup != nullptr)
            dividerGroup->dismissImmediately();

        if (safeThis == nullptr)
            return;
    }

    for (auto& bandUI : bandUIs)
    {
        if (bandUI.soloButton != nullptr)
            bandUI.soloButton->dismissPointerGesture();

        if (safeThis == nullptr)
            return;

        if (bandUI.enableButton != nullptr)
            bandUI.enableButton->dismissPointerGesture();

        if (safeThis == nullptr)
            return;

        if (bandUI.closeButton != nullptr)
            bandUI.closeButton->setPresented(false, false);

        if (safeThis == nullptr)
            return;
    }
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

    juce::Component::SafePointer<Multiband> safeThis(this);
    const bool previousCanonicalisingState = isCanonicalisingLines;
    isCanonicalisingLines = true;
    const juce::ScopeGuard restoreCanonicalisingState {
        [safeThis, previousCanonicalisingState]
        {
            if (safeThis != nullptr)
                safeThis->isCanonicalisingLines = previousCanonicalisingState;
        }
    };

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

        if (safeThis == nullptr)
            return changeIndex;

        freqDividerGroup[i]->setToggleState(true, juce::sendNotificationSync);

        if (safeThis == nullptr)
            return changeIndex;

        freqDividerGroup[i]->setVisible(true);

        if (safeThis == nullptr)
            return changeIndex;
    }
    for (int i = static_cast<int>(freqVector.size()); i < 3; ++i)
    {
        freqDividerGroup[i]->setFreq(-1);

        if (safeThis == nullptr)
            return changeIndex;

        freqDividerGroup[i]->setToggleState(false, juce::sendNotificationSync);

        if (safeThis == nullptr)
            return changeIndex;

        freqDividerGroup[i]->setVisible(false);

        if (safeThis == nullptr)
            return changeIndex;
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

    // End interactions while every group and callback is still alive. This
    // happens before ScopedValueSetter below because a host gesture-end
    // notification is allowed to synchronously destroy the editor.
    if (primaryDragActive && activePointerDividerIndex >= newLineCount)
        clearPrimaryPointerState();

    juce::Component::SafePointer<Multiband> safeThis(this);
    for (int divider = newLineCount; divider < 3; ++divider)
    {
        freqDividerGroup[static_cast<size_t>(divider)]->dismissImmediately();
        if (safeThis == nullptr)
            return;
    }

    const bool previousCanonicalisingState = isCanonicalisingLines;
    isCanonicalisingLines = true;
    const juce::ScopeGuard restoreCanonicalisingState {
        [safeThis, previousCanonicalisingState]
        {
            if (safeThis != nullptr)
                safeThis->isCanonicalisingLines = previousCanonicalisingState;
        }
    };
    for (int divider = 0; divider < 3; ++divider)
    {
        const bool enabled = divider < newLineCount;
        setDividerState(divider,
                        enabled,
                        frequencies[static_cast<size_t>(divider)],
                        juce::dontSendNotification);

        if (safeThis == nullptr)
            return;

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
                {
                    frequencyParameter->setValueNotifyingHost(normalised);

                    if (safeThis == nullptr)
                        return;
                }
            }
        }

        if (auto* lineStateParameter = processor.treeState.getParameter(
                ParameterIDAndName::getIDString(LINE_STATE_ID, divider)))
        {
            const float normalised = enabled ? 1.0f : 0.0f;
            if (! juce::approximatelyEqual(lineStateParameter->getValue(), normalised))
            {
                lineStateParameter->setValueNotifyingHost(normalised);

                if (safeThis == nullptr)
                    return;
            }
        }
    }

    lineNum = newLineCount;
    setLineIndex();
    updateFocusIndex(focusIndex, forceFocusNotification);

    if (safeThis == nullptr)
        return;

    setLineRelatedBoundsByX();

    if (safeThis == nullptr)
        return;

    setSoloRelatedBounds();

    if (safeThis == nullptr)
        return;

    refreshHoveredBandFromMouse();

    if (safeThis == nullptr)
        return;

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
        auto* processorToUse = &processor;
        juce::Component::SafePointer<Multiband> safeThis(this);
        {
            processorToUse->beginMultibandTopologyEdit();
            const juce::ScopeGuard finishTopologyEdit { [processorToUse]
            {
                processorToUse->requestMultibandTopologyReset();
            } };
            applyAuthoritativeBandCount(requestedBandCount, true, true);

            if (safeThis == nullptr)
                return;
        }

        // Parameter callbacks fired while the generation was odd can only
        // observe the immutable pre-edit snapshot. Publish one post-commit
        // notification so hosts persist the canonical NUM_BANDS/FREQ/LINE
        // tuple instead of the stale presentation generation.
        processorToUse->lfoDataHasChanged();
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
    if (! primaryDragActive || ! isPointerSource(e))
        return;

    const int completedDividerIndex = activePointerDividerIndex;
    clearPrimaryPointerState();
    const auto localEvent = e.getEventRelativeTo(this);
    updateHoveredBand(localEvent.getPosition(), getLocalBounds().contains(localEvent.getPosition()));
    repaint();

    // VerticalLine normally receives mouseUp before this recursive listener.
    // Keep this idempotent fallback for hosts that only deliver the parent
    // notification; ending the gesture may synchronously close the editor.
    dismissTrackedDividerGesture(completedDividerIndex);
}

void Multiband::mouseDrag(const juce::MouseEvent& e)
{
    if (dynamic_cast<juce::Button*>(e.eventComponent) != nullptr)
        return;

    if (! primaryDragActive || ! isPointerSource(e) || getWidth() <= 0)
        return;

    // moving lines by dragging mouse
    const int dividerIndex = getDividerIndexForEvent(e);
    if (! juce::isPositiveAndBelow(dividerIndex, lineNum)
        || dividerIndex != activePointerDividerIndex)
        return;

    isDragging = true;
    hoveredBandIndex = -1;
    updateCloseButtonVisibility();

    const auto localEvent = e.getEventRelativeTo(this);
    const float targetXPercent = localEvent.position.x / static_cast<float>(getWidth());
    juce::Component::SafePointer<Multiband> safeThis(this);
    dragLines(targetXPercent, dividerIndex);

    if (safeThis == nullptr)
        return;

    sortLinesInternal(false);

    if (safeThis == nullptr)
        return;

    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    repaint();
}

void Multiband::mouseDown(const juce::MouseEvent& e)
{
    const int incomingDividerIndex = getDividerIndexForEvent(e);
    if (primaryDragActive)
    {
        if (! isPointerSource(e))
            return;

        const int previousDividerIndex = activePointerDividerIndex;
        clearPrimaryPointerState();

        // For an event on the same VerticalLine, the target receives
        // mouseDown before this recursive listener and has already replaced
        // or rejected its stale gesture. Events anywhere else need the
        // parent to close the abandoned divider explicitly.
        if (previousDividerIndex != incomingDividerIndex)
        {
            juce::Component::SafePointer<Multiband> safeThis(this);
            dismissTrackedDividerGesture(previousDividerIndex);
            if (safeThis == nullptr)
                return;
        }
    }

    if (dynamic_cast<juce::Button*>(e.eventComponent) != nullptr)
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    if (! isPrimaryPointerDown(e))
        return;

    const int dividerIndex = incomingDividerIndex;
    if (dividerIndex < 0 && isEventFromDividerGroup(e))
        return;

    primaryDragActive = true;
    pointerSourceType = e.source.getType();
    pointerSourceIndex = e.source.getIndex();
    activePointerDividerIndex = dividerIndex;
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
                const int oldBandCount = lineNum + 1;
                const int newBandCount = oldBandCount + 1;
                int focusAfterInsert = focusIndex;
                if (focusIndex > splitBandIndex
                    || (focusIndex == splitBandIndex && newBandIsOnLeft))
                    ++focusAfterInsert;

                auto& processorToUse = processor;
                juce::Component::SafePointer<Multiband> safeThis(this);
                if (! processorToUse.addMultibandBand(
                        splitBandIndex,
                        oldBandCount,
                        newBandIsOnLeft,
                        transformFromLog(xPercent))
                    || safeThis == nullptr)
                    return;

                safeThis->focusIndex = juce::jlimit(0,
                                                    newBandCount - 1,
                                                    focusAfterInsert);
                safeThis->applyAuthoritativeBandCount(newBandCount,
                                                      false,
                                                      false);

                if (safeThis == nullptr)
                    return;

                // Focus propagation is the only remaining editor callback and
                // is deliberately the final operation in this path.
                safeThis->notifyFocusChanged();
                return;
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
    if (! juce::isPositiveAndBelow(index, lineNum))
        return;

    juce::Component::SafePointer<Multiband> safeThis(this);
    {
        // SliderAttachment notifications are synchronous. Suppress the normal
        // slider canonicalisation callback until this ordered recursive
        // publication is complete, otherwise integer-Hz quantisation can
        // re-enter moveToX and publish an outer divider prematurely.
        isPublishingCrossoverCascade = true;
        const juce::ScopeGuard cascadeGuard { [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->isPublishingCrossoverCascade = false;
        } };
        freqDividerGroup[index]->moveToX(lineNum, xPercent, limitLeft, freqDividerGroup);

        if (safeThis == nullptr)
            return;
    }

    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    refreshHoveredBandFromMouse();
    repaint();
}

void Multiband::beginCrossoverGesture()
{
    if (crossoverGestureDepth++ == 0)
        crossoverGestureSession = std::make_shared<CrossoverGestureSession>(
            crossoverParameters);
}

VerticalLine::ParameterGestureToken Multiband::touchCrossoverParameter(
    int dividerIndex)
{
    if (! juce::isPositiveAndBelow(dividerIndex, 3))
        return {};

    // Some regression and state-restoration paths intentionally publish a
    // crossover without an interactive gesture. They still need the value
    // update, but do not require a host begin/end pair.
    if (crossoverGestureDepth <= 0)
    {
        static const auto publicationToken = std::make_shared<int>(0);
        return publicationToken;
    }

    const auto arrayIndex = static_cast<size_t>(dividerIndex);
    auto session = crossoverGestureSession;
    if (session == nullptr)
        return {};

    juce::Component::SafePointer<Multiband> safeThis(this);
    if (! session->touch(arrayIndex))
        return {};

    if (safeThis == nullptr)
        return {};

    if (crossoverGestureDepth <= 0 || crossoverGestureSession != session)
        return {};

    return session;
}

void Multiband::endCrossoverGesture()
{
    if (crossoverGestureDepth <= 0 || --crossoverGestureDepth > 0)
        return;

    auto endingSession = std::move(crossoverGestureSession);

    // Releasing the last reference emits every matching end notification and
    // may synchronously delete this Multiband. It must remain the final access.
    endingSession.reset();
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
    const float closeHitSize = juce::jmax(
        size, static_cast<float>(CloseButton::minimumHitTargetSize));
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
        }
        else
        {
            bandUIs[i].soloButton->setVisible(false); // <--- MODIFIED
            bandUIs[i].enableButton->setVisible(false); // <--- MODIFIED
            bandUIs[i].closeButton->setPresented(false, false);
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
    {
        juce::Component::SafePointer<Multiband> safeThis(this);
        notifyFocusChanged();

        if (safeThis == nullptr)
            return didChange;
    }

    if (didChange)
        repaint();

    return didChange;
}

void Multiband::notifyFocusChanged()
{
    const int publishedFocus = focusIndex;
    auto callback = focusChangedCallback;

    if (isShowing())
        processor.setHistoryArray(publishedFocus);

    // The callback can synchronously close the editor. Invoke a local copy so
    // deleting this component cannot destroy the callable while it is active.
    if (callback)
        callback(publishedFocus);
}

void Multiband::sliderValueChanged(juce::Slider* slider)
{
    if (isCanonicalisingLines || isPublishingCrossoverCascade
        || processor.isMultibandTopologyEditInProgress())
        return;

    juce::Component::SafePointer<Multiband> safeThis(this);
    lineNum = countLines();
    setLineIndex();
    for (int i = 0; i < lineNum; i++)
    {
        if (slider == &freqDividerGroup[i]->getVerticalLine())
        {
            int freq = slider->getValue();
            freqDividerGroup[i]->setFreq(freq);

            if (safeThis == nullptr)
                return;

            freqDividerGroup[i]->moveToX(lineNum, freqDividerGroup[i]->getVerticalLine().getXPercent(), limitLeft, freqDividerGroup);

            if (safeThis == nullptr)
                return;
        }
    }
    // Parameters and presets can reduce the band count asynchronously.  Route
    // the clamp through the same focus notification path as direct clicks.
    updateFocusIndex(focusIndex, false);

    if (safeThis == nullptr)
        return;

    setLineRelatedBoundsByX();
    setSoloRelatedBounds();
    refreshHoveredBandFromMouse();
}

void Multiband::buttonClicked(juce::Button* button)
{
    for (int i = 0; i <= lineNum; ++i)
    {
        if (button == bandUIs[i].closeButton.get()) // <--- MODIFIED
        {
            const int deletedIndex = i;
            const int oldFocus = focusIndex;
            const int oldBandCount = lineNum + 1;
            const int newBandCount = oldBandCount - 1;
            int focusAfterDelete = oldFocus;
            if (deletedIndex < oldFocus
                || (deletedIndex == oldFocus && oldFocus >= newBandCount))
                --focusAfterDelete;

            // Any synchronous host callback below may delete this component
            // together with the whole editor. Keep the transaction processor-
            // owned, and inspect the weak pointer before touching presentation.
            auto& processorToUse = processor;
            juce::Component::SafePointer<Multiband> safeThis(this);
            if (! processorToUse.deleteMultibandBand(deletedIndex,
                                                      oldBandCount)
                || safeThis == nullptr)
                return;

            safeThis->focusIndex = juce::jlimit(0,
                                                newBandCount - 1,
                                                focusAfterDelete);
            safeThis->applyAuthoritativeBandCount(newBandCount,
                                                  false,
                                                  false);

            if (safeThis == nullptr)
                return;

            // Focus propagation can also invoke editor-owned callbacks, so it
            // is deliberately the final operation in this listener.
            safeThis->notifyFocusChanged();
            return;
        }
    }

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

void Multiband::mouseMove(const juce::MouseEvent& event)
{
    juce::Component::SafePointer<Multiband> safeThis(this);
    recoverMissingPointerUp(event);
    if (safeThis == nullptr)
        return;

    const auto relativeEvent = event.getEventRelativeTo(this);
    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
    if (lineNum < 3
        && relativeEvent.y >= 0
        && relativeEvent.y <= getHeight() / 5)
        repaint();
}

void Multiband::mouseEnter(const juce::MouseEvent& event)
{
    juce::Component::SafePointer<Multiband> safeThis(this);
    recoverMissingPointerUp(event);
    if (safeThis == nullptr)
        return;

    const auto relativeEvent = event.getEventRelativeTo(this);
    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
}

void Multiband::mouseExit(const juce::MouseEvent& event)
{
    juce::Component::SafePointer<Multiband> safeThis(this);
    recoverMissingPointerUp(event);
    if (safeThis == nullptr)
        return;

    const auto relativeEvent = event.getEventRelativeTo(this);
    updateHoveredBand(relativeEvent.getPosition(), getLocalBounds().contains(relativeEvent.getPosition()));
}

void Multiband::visibilityChanged()
{
    const juce::Component::SafePointer<Multiband> safeThis(this);
    juce::Component::visibilityChanged();

    if (safeThis == nullptr)
        return;

    if (! isVisible())
    {
        // A parent visibility change is not forwarded to children, so close
        // divider gestures and button presses explicitly before this surface
        // can be shown again. Gesture-end may synchronously delete the editor.
        dismissTransientUi();
        return;
    }

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

bool Multiband::isPointerSource(const juce::MouseEvent& event) const noexcept
{
    return event.source.getType() == pointerSourceType
        && event.source.getIndex() == pointerSourceIndex;
}

bool Multiband::admitDividerPointerGesture(
    int dividerIndex,
    juce::MouseInputSource::InputSourceType sourceType,
    int sourceIndex)
{
    if (! primaryDragActive)
        return true;

    if (sourceType != pointerSourceType || sourceIndex != pointerSourceIndex)
        return false;

    const int previousDividerIndex = activePointerDividerIndex;
    clearPrimaryPointerState();
    if (previousDividerIndex == dividerIndex)
        return true;

    juce::Component::SafePointer<Multiband> safeThis(this);
    dismissTrackedDividerGesture(previousDividerIndex);
    return safeThis != nullptr;
}

void Multiband::clearPrimaryPointerState() noexcept
{
    isDragging = false;
    primaryDragActive = false;
    pointerSourceIndex = -1;
    activePointerDividerIndex = -1;
}

void Multiband::dismissTrackedDividerGesture(int dividerIndex)
{
    if (! juce::isPositiveAndBelow(dividerIndex, 3)
        || freqDividerGroup[static_cast<size_t>(dividerIndex)] == nullptr)
        return;

    // The gesture-end callback may synchronously close the editor. This must
    // remain the final access through Multiband in the calling branch.
    freqDividerGroup[static_cast<size_t>(dividerIndex)]
        ->getVerticalLine().dismissPrimaryPointerGesture();
}

void Multiband::recoverMissingPointerUp(const juce::MouseEvent& event)
{
    if (! primaryDragActive
        || ! isPointerSource(event)
        || event.mods.isLeftButtonDown())
        return;

    const int completedDividerIndex = activePointerDividerIndex;
    clearPrimaryPointerState();

    // A move/enter/exit without the accepted primary button is the first
    // observable boundary after a missing mouseUp.
    dismissTrackedDividerGesture(completedDividerIndex);
}

void Multiband::handleDividerHidden(int dividerIndex)
{
    if (primaryDragActive && activePointerDividerIndex == dividerIndex)
        clearPrimaryPointerState();

    if (! juce::isPositiveAndBelow(dividerIndex, 3))
        return;

    auto safeGroup = juce::Component::SafePointer<FreqDividerGroup>(
        freqDividerGroup[static_cast<size_t>(dividerIndex)].get());
    juce::MessageManager::callAsync([safeGroup]
    {
        if (safeGroup != nullptr)
            safeGroup->dismissImmediately();
    });
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
        closeButton.setPresented(shouldShow);
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
