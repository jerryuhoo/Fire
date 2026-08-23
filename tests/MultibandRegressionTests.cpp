#include <PluginEditor.h>
#include <PluginProcessor.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/SpectrogramPanel/CloseButton.h>
#include <Panels/SpectrogramPanel/Multiband.h>
#include <Utility/AudioHelpers.h>
#include <Utility/Parameters.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <set>
#include <vector>

struct ModulatableSliderTestAccess
{
    static std::function<void(int)> createMenuResultHandler(ModulatableSlider& slider)
    {
        return slider.createModulationMenuResultHandler();
    }
};

namespace
{
void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void setNormalisedParameter(FireAudioProcessor& processor,
                            const juce::String& parameterID,
                            float normalisedValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalisedValue));
}

void configureDriveProbe(FireAudioProcessor& processor, float drive)
{
    const auto bandParameter = [](const juce::String& base)
    {
        return ParameterIDAndName::getIDString(base, 0);
    };

    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    setPlainParameter(processor, bandParameter(BAND_ENABLE_ID), 1.0f);
    setPlainParameter(processor, bandParameter(DRIVE_BYPASS_ID), 1.0f);
    setPlainParameter(processor, bandParameter(DRIVE_ID), drive);
    setPlainParameter(processor, bandParameter(MODE_ID), 4.0f); // hard clip
    setPlainParameter(processor, bandParameter(SAFE_ID), 0.0f);
    setPlainParameter(processor, bandParameter(EXTREME_ID), 0.0f);
    setPlainParameter(processor, bandParameter(LINKED_ID), 0.0f);
    setPlainParameter(processor, bandParameter(OUTPUT_ID), 0.0f);
    setPlainParameter(processor, bandParameter(MIX_ID), 1.0f);
    setPlainParameter(processor, bandParameter(SHAPE_MIX_ID), 1.0f);
    setPlainParameter(processor, MIX_ID, 1.0f);
    setPlainParameter(processor, OUTPUT_ID, 0.0f);
}

juce::AudioBuffer<float> makeDriveProbeInput(int numSamples, double sampleRate)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi
                           * 997.0 * static_cast<double>(sample) / sampleRate;
        const auto value = 0.25f * static_cast<float>(std::sin(phase));
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value);
    }
    return buffer;
}

const ModulationRouting* findRouting(const juce::Array<ModulationRouting>& routings,
                                     const juce::String& target)
{
    for (const auto& routing : routings)
        if (routing.targetParameterID == target)
            return &routing;

    return nullptr;
}

void checkRoutingIsReset(const ModulationRouting& routing)
{
    CHECK(routing.sourceLfoIndex == 0);
    CHECK(routing.targetParameterID.isEmpty());
    CHECK(routing.depth == Catch::Approx(0.5f));
    CHECK(routing.isBipolar);
    CHECK_FALSE(routing.isBypassed);
}

class NonParameterStateCapture final : public juce::AudioProcessorListener
{
public:
    explicit NonParameterStateCapture(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
        processor.addListener(this);
    }

    ~NonParameterStateCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int,
                                        float) override
    {
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (! details.nonParameterStateChanged || captureInProgress)
            return;

        const juce::ScopedValueSetter<bool> captureGuard(captureInProgress, true);
        states.emplace_back();
        processor.getStateInformation(states.back());
    }

    FireAudioProcessor& processor;
    std::vector<juce::MemoryBlock> states;
    bool captureInProgress = false;
};

class ParameterGestureCapture final : public juce::AudioProcessorListener
{
public:
    ParameterGestureCapture(FireAudioProcessor& processorToObserve,
                            int parameterIndexToObserve)
        : processor(processorToObserve), parameterIndex(parameterIndexToObserve)
    {
        processor.addListener(this);
    }

    ~ParameterGestureCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (changedParameterIndex != parameterIndex)
            return;

        ++valueChangeCount;
        valueChangedOutsideGesture = valueChangedOutsideGesture || gestureDepth != 1;
        events.push_back('V');
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,
                                                   int changedParameterIndex) override
    {
        if (changedParameterIndex != parameterIndex)
            return;

        ++beginCount;
        ++gestureDepth;
        maximumGestureDepth = juce::jmax(maximumGestureDepth, gestureDepth);
        events.push_back('B');
    }

    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,
                                                 int changedParameterIndex) override
    {
        if (changedParameterIndex != parameterIndex)
            return;

        ++endCount;
        --gestureDepth;
        minimumGestureDepth = juce::jmin(minimumGestureDepth, gestureDepth);
        events.push_back('E');
    }

    FireAudioProcessor& processor;
    int parameterIndex = -1;
    int beginCount = 0;
    int endCount = 0;
    int valueChangeCount = 0;
    int gestureDepth = 0;
    int maximumGestureDepth = 0;
    int minimumGestureDepth = 0;
    bool valueChangedOutsideGesture = false;
    std::vector<char> events;
};

class CrossoverTupleCapture final : public juce::AudioProcessorListener
{
public:
    explicit CrossoverTupleCapture(FireAudioProcessor& processorToObserve)
        : processor(processorToObserve)
    {
        for (int divider = 0; divider < 3; ++divider)
        {
            const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, divider);
            auto* parameter = processor.treeState.getParameter(frequencyID);
            REQUIRE(parameter != nullptr);
            parameterIndices[static_cast<size_t>(divider)] = parameter->getParameterIndex();
            values[static_cast<size_t>(divider)] =
                processor.treeState.getRawParameterValue(frequencyID);
            REQUIRE(values[static_cast<size_t>(divider)] != nullptr);
        }

        processor.addListener(this);
    }

    ~CrossoverTupleCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (captureInProgress)
            return;

        int changedDivider = -1;
        for (int divider = 0; divider < 3; ++divider)
            if (changedParameterIndex == parameterIndices[static_cast<size_t>(divider)])
                changedDivider = divider;

        if (changedDivider < 0)
            return;

        changedDividers.push_back(changedDivider);
        ++valueNotifications[static_cast<size_t>(changedDivider)];
        snapshots.push_back({
            values[0]->load(std::memory_order_relaxed),
            values[1]->load(std::memory_order_relaxed),
            values[2]->load(std::memory_order_relaxed)
        });

        const auto& tuple = snapshots.back();
        allSnapshotsStrict = allSnapshotsStrict
                             && tuple[0] < tuple[1]
                             && tuple[1] < tuple[2];

        const juce::ScopedValueSetter<bool> captureGuard(captureInProgress, true);
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        ++savedStateCaptureCount;

        auto stateXml = juce::AudioProcessor::getXmlFromBinary(
            state.getData(), static_cast<int>(state.getSize()));
        if (stateXml == nullptr)
        {
            savedStateParseFailed = true;
            return;
        }

        auto* parameterState = stateXml->getChildByName(
            processor.treeState.state.getType().toString());
        if (parameterState == nullptr)
        {
            savedStateParseFailed = true;
            return;
        }

        int savedBandCount = -1;
        std::array<double, 3> savedFrequencies {};
        std::array<bool, 3> foundFrequencies {};
        for (auto* child : parameterState->getChildIterator())
        {
            const auto parameterID = child->getStringAttribute("id");
            if (parameterID == NUM_BANDS_ID)
                savedBandCount = juce::roundToInt(child->getDoubleAttribute("value"));

            for (int divider = 0; divider < 3; ++divider)
                if (parameterID == ParameterIDAndName::getIDString(FREQ_ID, divider))
                {
                    savedFrequencies[static_cast<size_t>(divider)] =
                        child->getDoubleAttribute("value");
                    foundFrequencies[static_cast<size_t>(divider)] = true;
                }
        }

        const int activeDividerCount = juce::jlimit(0, 3, savedBandCount - 1);
        if (activeDividerCount != 3)
        {
            savedStateParseFailed = true;
            return;
        }

        for (int divider = 0; divider < activeDividerCount; ++divider)
            if (! foundFrequencies[static_cast<size_t>(divider)])
            {
                savedStateParseFailed = true;
                return;
            }

        for (int divider = 1; divider < activeDividerCount; ++divider)
            allSavedStateTuplesStrict = allSavedStateTuplesStrict
                                        && savedFrequencies[static_cast<size_t>(divider - 1)]
                                               < savedFrequencies[static_cast<size_t>(divider)];
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    FireAudioProcessor& processor;
    std::array<int, 3> parameterIndices { -1, -1, -1 };
    std::array<std::atomic<float>*, 3> values {};
    std::array<int, 3> valueNotifications {};
    std::vector<int> changedDividers;
    std::vector<std::array<float, 3>> snapshots;
    bool allSnapshotsStrict = true;
    int savedStateCaptureCount = 0;
    bool allSavedStateTuplesStrict = true;
    bool savedStateParseFailed = false;
    bool captureInProgress = false;
};

void checkBalancedGesture(const ParameterGestureCapture& capture)
{
    CHECK(capture.beginCount == 1);
    CHECK(capture.endCount == 1);
    CHECK(capture.valueChangeCount >= 1);
    CHECK(capture.gestureDepth == 0);
    CHECK(capture.maximumGestureDepth == 1);
    CHECK(capture.minimumGestureDepth == 0);
    CHECK_FALSE(capture.valueChangedOutsideGesture);
    REQUIRE_FALSE(capture.events.empty());
    CHECK(capture.events.front() == 'B');
    CHECK(capture.events.back() == 'E');
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers = {})
{
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             modifiers,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &component,
             &component,
             time,
             position,
             time,
             1,
             false };
}

std::vector<CloseButton*> getPositionedCloseButtons(Multiband& multiband)
{
    std::vector<CloseButton*> buttons;
    for (int childIndex = 0; childIndex < multiband.getNumChildComponents(); ++childIndex)
        if (auto* button = dynamic_cast<CloseButton*>(multiband.getChildComponent(childIndex));
            button != nullptr && ! button->getBounds().isEmpty())
            buttons.push_back(button);

    std::sort(buttons.begin(), buttons.end(), [](const auto* lhs, const auto* rhs)
    {
        return lhs->getBounds().getCentreX() < rhs->getBounds().getCentreX();
    });
    return buttons;
}

void initialiseBandLayout(FireAudioProcessor& processor,
                          int numBands,
                          const std::array<float, 3>& frequencies = { 1000.0f, 3000.0f, 7000.0f })
{
    REQUIRE(numBands >= 1);
    REQUIRE(numBands <= 4);
    setPlainParameter(processor, NUM_BANDS_ID, static_cast<float>(numBands));
    for (int divider = 0; divider < 3; ++divider)
    {
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(FREQ_ID, divider),
                          frequencies[static_cast<size_t>(divider)]);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(LINE_STATE_ID, divider),
                          divider < numBands - 1 ? 1.0f : 0.0f);
    }
}

void copyAllParameterValues(const FireAudioProcessor& source,
                            FireAudioProcessor& destination)
{
    for (auto* sourceParameter : source.getParameters())
    {
        const auto* sourceWithID = dynamic_cast<const juce::AudioProcessorParameterWithID*>(
            sourceParameter);
        REQUIRE(sourceWithID != nullptr);
        auto* destinationParameter = destination.treeState.getParameter(
            sourceWithID->getParameterID());
        REQUIRE(destinationParameter != nullptr);
        destinationParameter->setValueNotifyingHost(sourceParameter->getValue());
    }
}

juce::AudioBuffer<float> makeCrossoverProbeInput(int numSamples, double sampleRate)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto time = static_cast<double>(sample) / sampleRate;
        const auto low = std::sin(juce::MathConstants<double>::twoPi * 500.0 * time);
        const auto high = std::sin(juce::MathConstants<double>::twoPi * 8000.0 * time);
        const auto value = static_cast<float>(0.14 * low + 0.11 * high);
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value);
    }
    return buffer;
}

double meanSquaredDifference(const juce::AudioBuffer<float>& lhs,
                             const juce::AudioBuffer<float>& rhs,
                             int startSample)
{
    REQUIRE(lhs.getNumChannels() == rhs.getNumChannels());
    REQUIRE(lhs.getNumSamples() == rhs.getNumSamples());
    REQUIRE(startSample >= 0);
    REQUIRE(startSample < lhs.getNumSamples());

    double squaredError = 0.0;
    int valueCount = 0;
    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = startSample; sample < lhs.getNumSamples(); ++sample)
        {
            const auto difference = static_cast<double>(lhs.getSample(channel, sample))
                                    - rhs.getSample(channel, sample);
            squaredError += difference * difference;
            ++valueCount;
        }

    return squaredError / static_cast<double>(juce::jmax(1, valueCount));
}

CloseButton* getVisibleCloseButton(const std::vector<CloseButton*>& closeButtons)
{
    CloseButton* result = nullptr;
    for (auto* closeButton : closeButtons)
        if (closeButton != nullptr && closeButton->isVisible())
        {
            CHECK(result == nullptr);
            result = closeButton;
        }

    return result;
}

int getVisibleBandEnableButtonCount(const Multiband& multiband)
{
    int visibleCount = 0;
    for (int childIndex = 0; childIndex < multiband.getNumChildComponents(); ++childIndex)
        if (const auto* enableButton = dynamic_cast<const EnableButton*>(
                multiband.getChildComponent(childIndex));
            enableButton != nullptr && enableButton->isVisible())
            ++visibleCount;

    return visibleCount;
}

int getVisibleDividerCount(const Multiband& multiband)
{
    int visibleCount = 0;
    for (int childIndex = 0; childIndex < multiband.getNumChildComponents(); ++childIndex)
        if (const auto* divider = dynamic_cast<const FreqDividerGroup*>(
                multiband.getChildComponent(childIndex));
            divider != nullptr && divider->isVisible())
            ++visibleCount;

    return visibleCount;
}

std::array<FreqDividerGroup*, 3> getDividerGroupsByIndex(Multiband& multiband)
{
    std::array<FreqDividerGroup*, 3> result {};
    for (int childIndex = 0; childIndex < multiband.getNumChildComponents(); ++childIndex)
        if (auto* group = dynamic_cast<FreqDividerGroup*>(
                multiband.getChildComponent(childIndex)))
        {
            const int dividerIndex = group->getVerticalLine().getIndex();
            if (juce::isPositiveAndBelow(dividerIndex, 3))
                result[static_cast<size_t>(dividerIndex)] = group;
        }

    return result;
}

template<typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    for (int childIndex = 0; childIndex < root.getNumChildComponents(); ++childIndex)
    {
        auto* child = root.getChildComponent(childIndex);
        if (auto* match = dynamic_cast<ComponentType*>(child))
            return match;

        if (child != nullptr)
            if (auto* nestedMatch = findDescendant<ComponentType>(*child))
                return nestedMatch;
    }

    return nullptr;
}
} // namespace

TEST_CASE("Crossover mouse and text edits bracket host automation gestures",
          "[multiband][ui][automation][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* dividerGroup = findDescendant<FreqDividerGroup>(*multiband);
    REQUIRE(dividerGroup != nullptr);

    auto& divider = dividerGroup->getVerticalLine();
    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);
    auto* frequencyParameter = processor.treeState.getParameter(frequencyID);
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor, frequencyParameter->getParameterIndex());

    SECTION("divider drag")
    {
        const float initialX = divider.getXPercent();
        const double initialFrequency = divider.getValue();
        const auto eventPosition = divider.getLocalBounds().toFloat().getCentre();
        auto& dividerComponent = static_cast<juce::Component&>(divider);

        dividerComponent.mouseDown(
            makeMouseEvent(divider,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));

        CHECK(host.beginCount == 0);
        CHECK(host.endCount == 0);
        CHECK(divider.getXPercent() == Catch::Approx(initialX));
        CHECK(divider.getValue() == Catch::Approx(initialFrequency));

        const float firstTargetX = juce::jlimit(0.11f, 0.89f, initialX + 0.02f);
        const float finalTargetX = juce::jlimit(0.11f, 0.89f, initialX + 0.04f);
        multiband->dragLines(firstTargetX, 0);
        multiband->dragLines(finalTargetX, 0);
        const float xBeforeMouseUp = divider.getXPercent();

        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 0);
        CHECK(host.valueChangeCount >= 2);

        dividerComponent.mouseUp(makeMouseEvent(divider, eventPosition));

        CHECK(divider.getXPercent() == Catch::Approx(xBeforeMouseUp));
        // The existing geometry path round-trips through an integer-Hz slider,
        // so the logarithmic position is expected to be quantised slightly.
        CHECK(divider.getXPercent() == Catch::Approx(finalTargetX).margin(0.001f));
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.valueChangeCount >= 2);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
        REQUIRE_FALSE(host.events.empty());
        CHECK(host.events.front() == 'B');
        CHECK(host.events.back() == 'E');
    }

    SECTION("divider drag that pushes an adjacent crossover")
    {
        FreqDividerGroup* adjacentGroup = nullptr;
        for (int childIndex = 0; childIndex < multiband->getNumChildComponents(); ++childIndex)
            if (auto* group = dynamic_cast<FreqDividerGroup*>(
                    multiband->getChildComponent(childIndex));
                group != nullptr && group->getVerticalLine().getIndex() == 1)
                adjacentGroup = group;

        REQUIRE(adjacentGroup != nullptr);
        auto& adjacentDivider = adjacentGroup->getVerticalLine();
        const float adjacentInitialX = adjacentDivider.getXPercent();

        const auto adjacentFrequencyID = ParameterIDAndName::getIDString(FREQ_ID, 1);
        auto* adjacentParameter = processor.treeState.getParameter(adjacentFrequencyID);
        REQUIRE(adjacentParameter != nullptr);
        ParameterGestureCapture adjacentHost(processor,
                                             adjacentParameter->getParameterIndex());

        const auto untouchedFrequencyID = ParameterIDAndName::getIDString(FREQ_ID, 2);
        auto* untouchedParameter = processor.treeState.getParameter(untouchedFrequencyID);
        REQUIRE(untouchedParameter != nullptr);
        ParameterGestureCapture untouchedHost(processor,
                                              untouchedParameter->getParameterIndex());

        const auto eventPosition = divider.getLocalBounds().toFloat().getCentre();
        auto& dividerComponent = static_cast<juce::Component&>(divider);
        dividerComponent.mouseDown(
            makeMouseEvent(divider,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));

        const float targetX = juce::jmin(0.79f, adjacentInitialX + 0.02f);
        multiband->dragLines(targetX, 0);

        CHECK(adjacentDivider.getXPercent() > adjacentInitialX);
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 0);
        CHECK(host.valueChangeCount >= 1);
        CHECK(adjacentHost.beginCount == 1);
        CHECK(adjacentHost.endCount == 0);
        CHECK(adjacentHost.valueChangeCount >= 1);
        CHECK(untouchedHost.beginCount == 0);
        CHECK(untouchedHost.endCount == 0);
        CHECK(untouchedHost.valueChangeCount == 0);

        dividerComponent.mouseUp(makeMouseEvent(divider, eventPosition));

        checkBalancedGesture(host);
        checkBalancedGesture(adjacentHost);
        CHECK(untouchedHost.beginCount == 0);
        CHECK(untouchedHost.endCount == 0);
        CHECK(untouchedHost.valueChangeCount == 0);
    }

    SECTION("frequency text entry")
    {
        auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroup);
        REQUIRE(frequencyText != nullptr);
        auto* label = findDescendant<juce::Label>(*frequencyText);
        REQUIRE(label != nullptr);

        frequencyText->setVisible(true);
        REQUIRE(static_cast<bool>(label->onEditorShow));
        REQUIRE(static_cast<bool>(label->onEditorHide));
        label->onEditorShow();
        CHECK(host.beginCount == 0);
        CHECK(host.endCount == 0);

        // Label::textEditorReturnKeyPressed copies the committed editor text
        // before invoking onEditorHide. Reproduce that callback precondition
        // without creating a native peer solely to satisfy keyboard focus.
        label->setText("1.60 kHz", juce::dontSendNotification);
        label->onEditorHide();

        auto* publishedFrequency = processor.treeState.getRawParameterValue(frequencyID);
        REQUIRE(publishedFrequency != nullptr);
        CHECK(publishedFrequency->load(std::memory_order_relaxed)
              == Catch::Approx(1600.0f));
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.valueChangeCount >= 1);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
        REQUIRE_FALSE(host.events.empty());
        CHECK(host.events.front() == 'B');
        CHECK(host.events.back() == 'E');
    }
}

TEST_CASE("Interactive crossover cascades publish only strictly ordered tuples",
          "[multiband][ui][automation][crossover][tuple]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 4, { 320.0f, 640.0f, 1280.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    REQUIRE(dividerGroups[1] != nullptr);
    REQUIRE(dividerGroups[2] != nullptr);

    std::array<juce::RangedAudioParameter*, 3> frequencyParameters {};
    for (int divider = 0; divider < 3; ++divider)
    {
        frequencyParameters[static_cast<size_t>(divider)] = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(FREQ_ID, divider));
        REQUIRE(frequencyParameters[static_cast<size_t>(divider)] != nullptr);
    }

    ParameterGestureCapture firstGesture(
        processor, frequencyParameters[0]->getParameterIndex());
    ParameterGestureCapture secondGesture(
        processor, frequencyParameters[1]->getParameterIndex());
    ParameterGestureCapture thirdGesture(
        processor, frequencyParameters[2]->getParameterIndex());
    CrossoverTupleCapture tupleCapture(processor);
    const auto topologyGeneration =
        processor.getMultibandTopologyGenerationForTesting();
    REQUIRE((topologyGeneration & 1u) == 0u);

    const auto checkThreeParameterCascade = [&]
    {
        CAPTURE(tupleCapture.changedDividers);
        for (const auto& tuple : tupleCapture.snapshots)
            UNSCOPED_INFO("tuple: " << tuple[0] << ", " << tuple[1] << ", " << tuple[2]);
        CHECK(tupleCapture.snapshots.size() == 3);
        CHECK(tupleCapture.allSnapshotsStrict);
        CHECK(tupleCapture.valueNotifications[0] == 1);
        CHECK(tupleCapture.valueNotifications[1] == 1);
        CHECK(tupleCapture.valueNotifications[2] == 1);
        CHECK(tupleCapture.savedStateCaptureCount == 3);
        CHECK(tupleCapture.allSavedStateTuplesStrict);
        CHECK_FALSE(tupleCapture.savedStateParseFailed);
        checkBalancedGesture(firstGesture);
        checkBalancedGesture(secondGesture);
        checkBalancedGesture(thirdGesture);
    };

    SECTION("rightward drag cascades across all three dividers")
    {
        auto& source = dividerGroups[0]->getVerticalLine();
        auto& sourceComponent = static_cast<juce::Component&>(source);
        const auto eventPosition = source.getLocalBounds().toFloat().getCentre();
        sourceComponent.mouseDown(
            makeMouseEvent(source,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));
        multiband->dragLines(0.70f, 0);
        sourceComponent.mouseUp(makeMouseEvent(source, eventPosition));

        checkThreeParameterCascade();
    }

    SECTION("leftward drag cascades across all three dividers")
    {
        auto& source = dividerGroups[2]->getVerticalLine();
        auto& sourceComponent = static_cast<juce::Component&>(source);
        const auto eventPosition = source.getLocalBounds().toFloat().getCentre();
        sourceComponent.mouseDown(
            makeMouseEvent(source,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));
        multiband->dragLines(0.30f, 2);
        sourceComponent.mouseUp(makeMouseEvent(source, eventPosition));

        checkThreeParameterCascade();
    }

    SECTION("frequency text entry uses the same cross-divider cascade")
    {
        auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroups[0]);
        REQUIRE(frequencyText != nullptr);
        auto* label = findDescendant<juce::Label>(*frequencyText);
        REQUIRE(label != nullptr);
        REQUIRE(static_cast<bool>(label->onEditorShow));
        REQUIRE(static_cast<bool>(label->onEditorHide));

        label->onEditorShow();
        label->setText("2.50 kHz", juce::dontSendNotification);
        label->onEditorHide();

        checkThreeParameterCascade();
        CHECK(label->getText() == "2.50 kHz");
    }

    SECTION("invalid text restores the old display without publication")
    {
        auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroups[0]);
        REQUIRE(frequencyText != nullptr);
        auto* label = findDescendant<juce::Label>(*frequencyText);
        REQUIRE(label != nullptr);
        REQUIRE(static_cast<bool>(label->onEditorShow));
        REQUIRE(static_cast<bool>(label->onEditorHide));
        const auto originalText = label->getText();
        const std::array<float, 3> originalFrequencies {
            processor.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(FREQ_ID, 0))->load(),
            processor.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(FREQ_ID, 1))->load(),
            processor.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(FREQ_ID, 2))->load()
        };

        for (const auto& invalidText : std::array<juce::String, 3> {
                 "-100 Hz", "", "nan kHz" })
        {
            label->onEditorShow();
            label->setText(invalidText, juce::dontSendNotification);
            label->onEditorHide();
            CHECK(label->getText() == originalText);
        }

        CHECK(tupleCapture.snapshots.empty());
        CHECK(tupleCapture.savedStateCaptureCount == 0);
        CHECK(firstGesture.beginCount == 0);
        CHECK(firstGesture.endCount == 0);
        CHECK(firstGesture.valueChangeCount == 0);
        CHECK(secondGesture.beginCount == 0);
        CHECK(secondGesture.endCount == 0);
        CHECK(secondGesture.valueChangeCount == 0);
        CHECK(thirdGesture.beginCount == 0);
        CHECK(thirdGesture.endCount == 0);
        CHECK(thirdGesture.valueChangeCount == 0);
        for (int divider = 0; divider < 3; ++divider)
            CHECK(processor.treeState.getRawParameterValue(
                      ParameterIDAndName::getIDString(FREQ_ID, divider))->load()
                  == Catch::Approx(originalFrequencies[static_cast<size_t>(divider)]));
    }

    CHECK(processor.getMultibandTopologyGenerationForTesting()
          == topologyGeneration);
}

TEST_CASE("Band move/reset parameter contract covers every per-band processor parameter",
          "[multiband][parameters]")
{
    // Keep this explicit: a newly-added processor parameter must make this test
    // fail until the add/delete copy contract deliberately accounts for it.
    const std::set<juce::String> expectedBaseIDs {
        MODE_ID,
        LINKED_ID,
        SAFE_ID,
        EXTREME_ID,
        DRIVE_ID,
        COMP_RATIO_ID,
        COMP_THRESH_ID,
        COMP_ATTACK_ID,
        COMP_RELEASE_ID,
        COMP_MIX_ID,
        WIDTH_ID,
        PAN_ID,
        WIDTH_MIX_ID,
        OUTPUT_ID,
        MIX_ID,
        BIAS_ID,
        REC_ID,
        SHAPE_MIX_ID,
        BAND_ENABLE_ID,
        BAND_SOLO_ID,
        DRIVE_BYPASS_ID,
        COMP_BYPASS_ID,
        WIDTH_BYPASS_ID,
        SHAPE_BYPASS_ID,
        DC_FILTER_ID,
    };

    std::set<juce::String> actualBaseIDs;
    for (const auto& parameter : ParameterIDAndName::getBandParameterInfo())
        actualBaseIDs.insert(parameter.idBase);

    CHECK(actualBaseIDs == expectedBaseIDs);

    FireAudioProcessor processor;
    for (int band = 0; band < 4; ++band)
        for (const auto& baseID : expectedBaseIDs)
        {
            INFO("Missing APVTS parameter "
                 << ParameterIDAndName::getIDString(baseID, band));
            CHECK(processor.treeState.getParameter(
                      ParameterIDAndName::getIDString(baseID, band)) != nullptr);
        }
}

TEST_CASE("Drive changes the processed signal when its band is active",
          "[multiband][drive][processor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 2048;

    FireAudioProcessor zeroDriveProcessor;
    FireAudioProcessor fullDriveProcessor;
    configureDriveProbe(zeroDriveProcessor, 0.0f);
    configureDriveProbe(fullDriveProcessor, 100.0f);
    zeroDriveProcessor.prepareToPlay(sampleRate, blockSize);
    fullDriveProcessor.prepareToPlay(sampleRate, blockSize);

    auto zeroDriveOutput = makeDriveProbeInput(blockSize, sampleRate);
    auto fullDriveOutput = zeroDriveOutput;
    juce::MidiBuffer zeroDriveMidi;
    juce::MidiBuffer fullDriveMidi;
    zeroDriveProcessor.processBlock(zeroDriveOutput, zeroDriveMidi);
    fullDriveProcessor.processBlock(fullDriveOutput, fullDriveMidi);

    float maximumDifference = 0.0f;
    double zeroDriveEnergy = 0.0;
    double fullDriveEnergy = 0.0;
    for (int sample = blockSize / 2; sample < blockSize; ++sample)
    {
        const auto zeroDriveSample = zeroDriveOutput.getSample(0, sample);
        const auto fullDriveSample = fullDriveOutput.getSample(0, sample);
        REQUIRE(std::isfinite(zeroDriveSample));
        REQUIRE(std::isfinite(fullDriveSample));
        maximumDifference = juce::jmax(maximumDifference,
                                       std::abs(fullDriveSample - zeroDriveSample));
        zeroDriveEnergy += static_cast<double>(zeroDriveSample) * zeroDriveSample;
        fullDriveEnergy += static_cast<double>(fullDriveSample) * fullDriveSample;
    }

    CAPTURE(maximumDifference, zeroDriveEnergy, fullDriveEnergy);
    CHECK(maximumDifference > 0.25f);
    CHECK(fullDriveEnergy > zeroDriveEnergy * 2.0);
}

TEST_CASE("Shape bypass keeps Drive audible independently of the Shape mix",
          "[multiband][drive][shape][processor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 2048;

    const auto render = [=](float drive, float shapeMix)
    {
        FireAudioProcessor processor;
        configureDriveProbe(processor, drive);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(SHAPE_BYPASS_ID, 0),
                          0.0f);
        setPlainParameter(processor,
                          ParameterIDAndName::getIDString(SHAPE_MIX_ID, 0),
                          shapeMix);
        processor.prepareToPlay(sampleRate, blockSize);

        auto output = makeDriveProbeInput(blockSize, sampleRate);
        juce::MidiBuffer midi;
        processor.processBlock(output, midi);
        return output;
    };

    const auto zeroDriveShapeDry = render(0.0f, 0.0f);
    const auto fullDriveShapeDry = render(100.0f, 0.0f);
    const auto fullDriveShapeWet = render(100.0f, 1.0f);
    const auto driveDifference = meanSquaredDifference(zeroDriveShapeDry,
                                                       fullDriveShapeDry,
                                                       blockSize / 2);
    const auto bypassedShapeMixDifference = meanSquaredDifference(fullDriveShapeDry,
                                                                  fullDriveShapeWet,
                                                                  blockSize / 2);

    CAPTURE(driveDifference, bypassedShapeMixDifference);
    CHECK(driveDifference > 1.0e-3);
    CHECK(bypassedShapeMixDifference < 1.0e-12);
}

TEST_CASE("Deleting a band replaces its modulation with the surviving right band",
          "[multiband][lfo][delete]")
{
    FireAudioProcessor processor;
    const auto deletedDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto survivingDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);

    processor.assignLfoToTarget(3, deletedDrive);
    processor.setModulationDepth(deletedDrive, -0.7f);
    processor.toggleBipolarMode(deletedDrive);
    processor.getLfoManager().toggleBypassForRouting(deletedDrive);
    processor.assignLfoToTarget(1, survivingDrive);

    const auto before = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(before.size() >= 2);
    REQUIRE(findRouting(before, deletedDrive) != nullptr);
    REQUIRE(findRouting(before, survivingDrive) != nullptr);

    // Deleting zero-based band 0 shifts source bands 1...3 one slot left.
    processor.shiftLfoModulationTargets(1, 3, -1);

    const auto after = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(after.size() == before.size());
    const auto* movedSurvivor = findRouting(after, deletedDrive);
    REQUIRE(movedSurvivor != nullptr);
    CHECK(movedSurvivor->sourceLfoIndex == 1);
    CHECK(findRouting(after, survivingDrive) == nullptr);

    int activeRoutingCount = 0;
    for (const auto& routing : after)
        activeRoutingCount += routing.targetParameterID.isNotEmpty() ? 1 : 0;
    CHECK(activeRoutingCount == 1);

    // The route belonging to the deleted band must be reusable without stale
    // depth, polarity or bypass state leaking into a later assignment.
    checkRoutingIsReset(after.getReference(0));
}

TEST_CASE("Adding a band clears overwritten modulation destinations",
          "[multiband][lfo][add]")
{
    FireAudioProcessor processor;
    const auto originalFirstDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto shiftedFirstDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    const auto oldFourthDrive = ParameterIDAndName::getIDString(DRIVE_ID, 3);

    processor.assignLfoToTarget(2, originalFirstDrive);
    processor.assignLfoToTarget(3, oldFourthDrive);
    processor.setModulationDepth(oldFourthDrive, -0.4f);
    processor.toggleBipolarMode(oldFourthDrive);
    processor.getLfoManager().toggleBypassForRouting(oldFourthDrive);

    const auto before = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(before.size() >= 2);

    // Inserting at zero-based band 0 shifts source bands 0...2 right. Band 3
    // is a destination of that move, so its old route must be overwritten.
    processor.shiftLfoModulationTargets(0, 2, 1);

    const auto after = processor.getLfoManager().getModulationRoutingsCopy();
    REQUIRE(after.size() == before.size());
    const auto* movedFirst = findRouting(after, shiftedFirstDrive);
    REQUIRE(movedFirst != nullptr);
    CHECK(movedFirst->sourceLfoIndex == 2);
    CHECK(findRouting(after, originalFirstDrive) == nullptr);
    CHECK(findRouting(after, oldFourthDrive) == nullptr);

    int activeRoutingCount = 0;
    for (const auto& routing : after)
        activeRoutingCount += routing.targetParameterID.isNotEmpty() ? 1 : 0;
    CHECK(activeRoutingCount == 1);
    checkRoutingIsReset(after.getReference(1));
}

TEST_CASE("Band add and delete notify the host after topology commit",
          "[multiband][ui][state][host][topology][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("add")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 1);
        const auto oldDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        const auto shiftedDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);
        processor.assignLfoToTarget(2, oldDrive);

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);

        NonParameterStateCapture host(processor);
        const auto insertionPoint = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * 0.35f,
            static_cast<float>(multiband->getHeight()) * 0.10f
        };
        static_cast<juce::Component&>(*multiband).mouseDown(
            makeMouseEvent(*multiband,
                           insertionPoint,
                           juce::ModifierKeys::leftButtonModifier));

        REQUIRE(host.states.size() == 1);
        FireAudioProcessor restored;
        restored.setStateInformation(host.states.front().getData(),
                                     static_cast<int>(host.states.front().getSize()));
        const auto* restoredBandCount = restored.treeState.getRawParameterValue(
            NUM_BANDS_ID);
        REQUIRE(restoredBandCount != nullptr);
        CHECK(restoredBandCount->load(std::memory_order_relaxed)
              == Catch::Approx(2.0f));
        const auto restoredRoutings = restored.getLfoManager()
                                          .getModulationRoutingsCopy();
        const auto* movedRouting = findRouting(restoredRoutings, shiftedDrive);
        REQUIRE(movedRouting != nullptr);
        CHECK(movedRouting->sourceLfoIndex == 2);
        CHECK(findRouting(restoredRoutings, oldDrive) == nullptr);
    }

    SECTION("delete")
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 2);
        const auto deletedDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        const auto survivingDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);
        processor.assignLfoToTarget(1, survivingDrive);

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);
        auto closeButtons = getPositionedCloseButtons(*multiband);
        REQUIRE(closeButtons.size() == 2);
        auto* firstBandClose = closeButtons.front();
        REQUIRE(firstBandClose != nullptr);
        const auto closeCentre = firstBandClose->getBounds().toFloat().getCentre();
        multiband->mouseMove(makeMouseEvent(*multiband, closeCentre));
        REQUIRE(firstBandClose->isVisible());

        NonParameterStateCapture host(processor);
        firstBandClose->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        REQUIRE(host.states.size() == 1);
        FireAudioProcessor restored;
        restored.setStateInformation(host.states.front().getData(),
                                     static_cast<int>(host.states.front().getSize()));
        const auto* restoredBandCount = restored.treeState.getRawParameterValue(
            NUM_BANDS_ID);
        REQUIRE(restoredBandCount != nullptr);
        CHECK(restoredBandCount->load(std::memory_order_relaxed)
              == Catch::Approx(1.0f));
        const auto restoredRoutings = restored.getLfoManager()
                                          .getModulationRoutingsCopy();
        const auto* movedRouting = findRouting(restoredRoutings, deletedDrive);
        REQUIRE(movedRouting != nullptr);
        CHECK(movedRouting->sourceLfoIndex == 1);
        CHECK(findRouting(restoredRoutings, survivingDrive) == nullptr);
    }
}

TEST_CASE("Deleting the focused last band rebinds Drive to the remaining audible band",
          "[multiband][ui][delete][focus]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto deletedDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    setPlainParameter(processor, firstDriveID, 11.0f);
    setPlainParameter(processor, deletedDriveID, 67.0f);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);

    multiband->setFocusIndex(1);
    CHECK(multiband->getFocusIndex() == 1);
    CHECK(bandPanel->getFocusBandNum() == 1);
    REQUIRE(bandPanel->getDriveKnob() != nullptr);
    CHECK(bandPanel->getDriveKnob()->getParamID() == deletedDriveID);

    auto closeButtons = getPositionedCloseButtons(*multiband);
    REQUIRE(closeButtons.size() == 2);
    auto* lastBandClose = closeButtons.back();
    REQUIRE(lastBandClose != nullptr);
    CHECK_FALSE(lastBandClose->isVisible());

    // The button begins hidden. A normal move into its eventual hit region
    // must make it visible and place it above the spectrum children.
    const auto closeCentre = lastBandClose->getBounds().toFloat().getCentre();
    multiband->mouseMove(makeMouseEvent(*multiband, closeCentre));
    REQUIRE(lastBandClose->isVisible());
    CHECK(lastBandClose->getBounds().contains(closeCentre.toInt()));
    CHECK(multiband->getComponentAt(closeCentre.toInt()) == lastBandClose);

    lastBandClose->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    CHECK(multiband->getFocusIndex() == 0);
    CHECK(bandPanel->getFocusBandNum() == 0);
    CHECK(bandPanel->getDriveKnob()->getParamID() == firstDriveID);

    const auto* numBands = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(numBands != nullptr);
    CHECK(numBands->load() == Catch::Approx(1.0f));

    // This is the regression assertion for the original silent-control bug:
    // the visible knob must now write active Band 1, not the reset hidden slot.
    bandPanel->getDriveKnob()->setValue(83.0, juce::sendNotificationSync);
    const auto* firstDrive = processor.treeState.getRawParameterValue(firstDriveID);
    const auto* hiddenDrive = processor.treeState.getRawParameterValue(deletedDriveID);
    REQUIRE(firstDrive != nullptr);
    REQUIRE(hiddenDrive != nullptr);
    CHECK(firstDrive->load() == Catch::Approx(83.0f));
    CHECK(hiddenDrive->load() == Catch::Approx(0.0f));
}

TEST_CASE("Set Value popup keeps the modulation target that opened it",
          "[multiband][ui][modulation][value-entry]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto secondDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    constexpr float firstBaseValue = 10.0f;
    constexpr float secondBaseValue = 70.0f;
    constexpr float firstInitialDepth = 0.15f;
    constexpr float secondInitialDepth = -0.35f;
    constexpr float enteredValue = 40.0f;

    setPlainParameter(processor, firstDriveID, firstBaseValue);
    setPlainParameter(processor, secondDriveID, secondBaseValue);
    processor.assignLfoToTarget(0, firstDriveID);
    processor.assignLfoToTarget(1, secondDriveID);
    processor.setModulationDepth(firstDriveID, firstInitialDepth);
    processor.setModulationDepth(secondDriveID, secondInitialDepth);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    auto* valueEntryPopup = findDescendant<ValueEntryPopup>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);
    REQUIRE(valueEntryPopup != nullptr);

    multiband->setFocusIndex(0);
    auto* reusedDriveKnob = bandPanel->getDriveKnob();
    REQUIRE(reusedDriveKnob != nullptr);
    REQUIRE(reusedDriveKnob->getParamID() == firstDriveID);
    REQUIRE(static_cast<bool>(reusedDriveKnob->onSetValueRequested));
    auto deliverMenuResult = ModulatableSliderTestAccess::createMenuResultHandler(
        *reusedDriveKnob);
    deliverMenuResult(static_cast<int>(ModulatableSlider::ModulationMenuCommand::setValue));
    REQUIRE(valueEntryPopup->isVisible());

    // The non-modal popup remains open while the one shared BandPanel slider
    // is rebound to another band.  Accepting the value must still address the
    // parameter that opened the popup, not the slider's newer parameter ID.
    multiband->setFocusIndex(1);
    REQUIRE(bandPanel->getDriveKnob() == reusedDriveKnob);
    REQUIRE(reusedDriveKnob->getParamID() == secondDriveID);
    REQUIRE(valueEntryPopup->isVisible());
    REQUIRE(static_cast<bool>(valueEntryPopup->onOk));
    valueEntryPopup->onOk(enteredValue);
    CHECK_FALSE(valueEntryPopup->isVisible());

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* firstRouting = findRouting(routings, firstDriveID);
    const auto* secondRouting = findRouting(routings, secondDriveID);
    REQUIRE(firstRouting != nullptr);
    REQUIRE(secondRouting != nullptr);
    REQUIRE(firstRouting->isBipolar);

    auto* firstParameter = processor.treeState.getParameter(firstDriveID);
    REQUIRE(firstParameter != nullptr);
    const auto range = firstParameter->getNormalisableRange();
    const auto expectedFirstDepth = juce::jlimit(
        -1.0f,
        1.0f,
        2.0f * (range.convertTo0to1(enteredValue)
                - range.convertTo0to1(firstBaseValue)));

    CHECK(firstRouting->depth == Catch::Approx(expectedFirstDepth));
    CHECK(secondRouting->depth == Catch::Approx(secondInitialDepth));

    const auto* firstBase = processor.treeState.getRawParameterValue(firstDriveID);
    const auto* secondBase = processor.treeState.getRawParameterValue(secondDriveID);
    REQUIRE(firstBase != nullptr);
    REQUIRE(secondBase != nullptr);
    CHECK(firstBase->load() == Catch::Approx(firstBaseValue));
    CHECK(secondBase->load() == Catch::Approx(secondBaseValue));
}

TEST_CASE("Assign callbacks remain alive while assigning clears the slider callback",
          "[multiband][ui][modulation][assign]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.parameterID = "assign-target";
    slider.setBounds(0, 0, 100, 100);

    auto lifetimeOwner = std::make_shared<int>(1);
    std::weak_ptr<int> lifetimeObserver = lifetimeOwner;
    bool callbackStayedAlive = false;
    juce::String receivedTarget;

    slider.onClickInAssignMode = [&slider,
                                  lifetimeGuard = std::move(lifetimeOwner),
                                  &callbackStayedAlive,
                                  &receivedTarget](const juce::String& target)
    {
        const std::weak_ptr<int> observer { lifetimeGuard };
        receivedTarget = target;

        // FireAudioProcessorEditor::exitAssignMode performs this same clear.
        // The callback's captured state must remain alive for the rest of the
        // invocation even though the slider no longer owns it.
        slider.onClickInAssignMode = nullptr;
        callbackStayedAlive = ! observer.expired();
    };

    slider.mouseDown(makeMouseEvent(
        slider,
        { 50.0f, 50.0f },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));

    CHECK(receivedTarget == "assign-target");
    CHECK(callbackStayedAlive);
    CHECK(lifetimeObserver.expired());
    CHECK_FALSE(static_cast<bool>(slider.onClickInAssignMode));
}

TEST_CASE("Delayed modulation menu actions retain the target present when the menu opened",
          "[multiband][ui][modulation][popup-menu][target]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto secondDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    constexpr float firstBaseValue = 10.0f;
    constexpr float secondBaseValue = 70.0f;
    constexpr float firstInitialDepth = 0.20f;
    constexpr float secondInitialDepth = -0.35f;
    setPlainParameter(processor, firstDriveID, firstBaseValue);
    setPlainParameter(processor, secondDriveID, secondBaseValue);
    processor.assignLfoToTarget(0, firstDriveID);
    processor.assignLfoToTarget(1, secondDriveID);
    processor.setModulationDepth(firstDriveID, firstInitialDepth);
    processor.setModulationDepth(secondDriveID, secondInitialDepth);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    auto* valueEntryPopup = findDescendant<ValueEntryPopup>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);
    REQUIRE(valueEntryPopup != nullptr);

    multiband->setFocusIndex(0);
    auto* reusedDriveKnob = bandPanel->getDriveKnob();
    REQUIRE(reusedDriveKnob != nullptr);
    const auto targetWhenMenuOpened = reusedDriveKnob->getParamID();
    REQUIRE(targetWhenMenuOpened == firstDriveID);
    auto deliverMenuResult = ModulatableSliderTestAccess::createMenuResultHandler(
        *reusedDriveKnob);

    // PopupMenu completion is asynchronous. Model that delay without relying
    // on a platform popup window: the shared BandPanel knob is rebound before
    // the selected command is delivered to its existing callback.
    multiband->setFocusIndex(1);
    REQUIRE(bandPanel->getDriveKnob() == reusedDriveKnob);
    REQUIRE(reusedDriveKnob->getParamID() == secondDriveID);

    SECTION("Set Value")
    {
        constexpr float enteredValue = 40.0f;
        REQUIRE(static_cast<bool>(reusedDriveKnob->onSetValueRequested));
        deliverMenuResult(static_cast<int>(ModulatableSlider::ModulationMenuCommand::setValue));
        REQUIRE(valueEntryPopup->isVisible());
        REQUIRE(static_cast<bool>(valueEntryPopup->onOk));
        valueEntryPopup->onOk(enteredValue);

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* firstRouting = findRouting(routings, targetWhenMenuOpened);
        const auto* secondRouting = findRouting(routings, secondDriveID);
        REQUIRE(firstRouting != nullptr);
        REQUIRE(secondRouting != nullptr);

        auto* firstParameter = processor.treeState.getParameter(firstDriveID);
        REQUIRE(firstParameter != nullptr);
        const auto range = firstParameter->getNormalisableRange();
        const auto expectedFirstDepth = juce::jlimit(
            -1.0f,
            1.0f,
            2.0f * (range.convertTo0to1(enteredValue)
                    - range.convertTo0to1(firstBaseValue)));
        CHECK(firstRouting->depth == Catch::Approx(expectedFirstDepth));
        CHECK(secondRouting->depth == Catch::Approx(secondInitialDepth));
    }

    SECTION("Clear")
    {
        REQUIRE(static_cast<bool>(reusedDriveKnob->onModulationCleared));
        deliverMenuResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        CHECK(findRouting(routings, targetWhenMenuOpened) == nullptr);
        const auto* secondRouting = findRouting(routings, secondDriveID);
        REQUIRE(secondRouting != nullptr);
        CHECK(secondRouting->depth == Catch::Approx(secondInitialDepth));
    }

    SECTION("Invert Depth")
    {
        REQUIRE(static_cast<bool>(reusedDriveKnob->onModulationInverted));
        deliverMenuResult(static_cast<int>(ModulatableSlider::ModulationMenuCommand::invertDepth));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* firstRouting = findRouting(routings, targetWhenMenuOpened);
        const auto* secondRouting = findRouting(routings, secondDriveID);
        REQUIRE(firstRouting != nullptr);
        REQUIRE(secondRouting != nullptr);
        CHECK(firstRouting->depth == Catch::Approx(-firstInitialDepth));
        CHECK(secondRouting->depth == Catch::Approx(secondInitialDepth));
    }

    SECTION("Bipolar mode")
    {
        REQUIRE(static_cast<bool>(reusedDriveKnob->onBipolarModeToggled));
        deliverMenuResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::togglePolarity));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* firstRouting = findRouting(routings, targetWhenMenuOpened);
        const auto* secondRouting = findRouting(routings, secondDriveID);
        REQUIRE(firstRouting != nullptr);
        REQUIRE(secondRouting != nullptr);
        CHECK_FALSE(firstRouting->isBipolar);
        CHECK(secondRouting->isBipolar);
    }

    SECTION("Bypass")
    {
        REQUIRE(static_cast<bool>(reusedDriveKnob->onBypassToggled));
        deliverMenuResult(static_cast<int>(ModulatableSlider::ModulationMenuCommand::toggleBypass));

        const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
        const auto* firstRouting = findRouting(routings, targetWhenMenuOpened);
        const auto* secondRouting = findRouting(routings, secondDriveID);
        REQUIRE(firstRouting != nullptr);
        REQUIRE(secondRouting != nullptr);
        CHECK(firstRouting->isBypassed);
        CHECK_FALSE(secondRouting->isBypassed);
    }
}

TEST_CASE("Deleting a middle band moves every survivor setting and resets the inactive slot",
          "[multiband][ui][delete][parameters]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3);

    const auto& bandParameters = ParameterIDAndName::getBandParameterInfo();
    std::vector<float> expectedMovedValues;
    std::vector<float> expectedResetValues;
    expectedMovedValues.reserve(bandParameters.size());
    expectedResetValues.reserve(bandParameters.size());

    for (const auto& parameterInfo : bandParameters)
    {
        const auto targetID = ParameterIDAndName::getIDString(parameterInfo.idBase, 1);
        const auto sourceID = ParameterIDAndName::getIDString(parameterInfo.idBase, 2);
        auto* target = processor.treeState.getParameter(targetID);
        auto* source = processor.treeState.getParameter(sourceID);
        REQUIRE(target != nullptr);
        REQUIRE(source != nullptr);

        setNormalisedParameter(processor, targetID, 0.17f);
        auto sourceValue = 0.73f;
        if (parameterInfo.idBase == LINKED_ID
            || parameterInfo.idBase == BAND_SOLO_ID
            || parameterInfo.idBase == DRIVE_BYPASS_ID)
            sourceValue = 0.0f;
        else if (parameterInfo.idBase == BAND_ENABLE_ID)
            sourceValue = 1.0f;
        setNormalisedParameter(processor, sourceID, sourceValue);

        expectedMovedValues.push_back(source->getValue());
        expectedResetValues.push_back(source->getDefaultValue());
    }

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);
    multiband->setFocusIndex(2);
    REQUIRE(bandPanel->getFocusBandNum() == 2);

    auto closeButtons = getPositionedCloseButtons(*multiband);
    REQUIRE(closeButtons.size() == 3);
    auto* middleBandClose = closeButtons[1];
    const auto closeCentre = middleBandClose->getBounds().toFloat().getCentre();
    multiband->mouseMove(makeMouseEvent(*multiband, closeCentre));
    REQUIRE(middleBandClose->isVisible());
    middleBandClose->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    CHECK(multiband->getFocusIndex() == 1);
    CHECK(bandPanel->getFocusBandNum() == 1);
    CHECK(bandPanel->getDriveKnob()->getParamID()
          == ParameterIDAndName::getIDString(DRIVE_ID, 1));

    for (size_t parameterIndex = 0; parameterIndex < bandParameters.size(); ++parameterIndex)
    {
        const auto& parameterInfo = bandParameters[parameterIndex];
        const auto movedID = ParameterIDAndName::getIDString(parameterInfo.idBase, 1);
        const auto resetID = ParameterIDAndName::getIDString(parameterInfo.idBase, 2);
        auto* moved = processor.treeState.getParameter(movedID);
        auto* reset = processor.treeState.getParameter(resetID);
        REQUIRE(moved != nullptr);
        REQUIRE(reset != nullptr);
        INFO("Band parameter " << parameterInfo.idBase);
        CHECK(moved->getValue() == Catch::Approx(expectedMovedValues[parameterIndex]));
        CHECK(reset->getValue() == Catch::Approx(expectedResetValues[parameterIndex]));
    }
}

TEST_CASE("Adding a divider preserves the focused band's identity on either side",
          "[multiband][ui][add][focus]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto runInsertScenario = [](bool insertOnLeft)
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 1);

        const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        const auto secondDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
        const auto firstDriveBypassID = ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 0);
        const auto secondDriveBypassID = ParameterIDAndName::getIDString(DRIVE_BYPASS_ID, 1);
        setPlainParameter(processor, firstDriveID, 61.0f);
        setPlainParameter(processor, firstDriveBypassID, 0.0f);

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        auto* multiband = findDescendant<Multiband>(*editor);
        auto* bandPanel = findDescendant<BandPanel>(*editor);
        REQUIRE(multiband != nullptr);
        REQUIRE(bandPanel != nullptr);
        REQUIRE(multiband->getFocusIndex() == 0);
        REQUIRE(bandPanel->getDriveKnob()->getParamID() == firstDriveID);

        const auto insertionPoint = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * (insertOnLeft ? 0.35f : 0.65f),
            static_cast<float>(multiband->getHeight()) * 0.10f
        };
        static_cast<juce::Component&>(*multiband).mouseDown(
            makeMouseEvent(*multiband,
                           insertionPoint,
                           juce::ModifierKeys::leftButtonModifier));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        const auto* firstDrive = processor.treeState.getRawParameterValue(firstDriveID);
        const auto* secondDrive = processor.treeState.getRawParameterValue(secondDriveID);
        const auto* firstDriveBypass = processor.treeState.getRawParameterValue(firstDriveBypassID);
        const auto* secondDriveBypass = processor.treeState.getRawParameterValue(secondDriveBypassID);
        const auto* numBands = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
        REQUIRE(firstDrive != nullptr);
        REQUIRE(secondDrive != nullptr);
        REQUIRE(firstDriveBypass != nullptr);
        REQUIRE(secondDriveBypass != nullptr);
        REQUIRE(numBands != nullptr);
        CHECK(numBands->load() == Catch::Approx(2.0f));

        if (insertOnLeft)
        {
            // A new default band occupies the left side; the old logical band,
            // including Drive's power state, moves right and remains selected.
            CHECK(multiband->getFocusIndex() == 1);
            CHECK(bandPanel->getFocusBandNum() == 1);
            CHECK(bandPanel->getDriveKnob()->getParamID() == secondDriveID);
            CHECK(firstDrive->load() == Catch::Approx(0.0f));
            CHECK(firstDriveBypass->load() == Catch::Approx(1.0f));
            CHECK(secondDrive->load() == Catch::Approx(61.0f));
            CHECK(secondDriveBypass->load() == Catch::Approx(0.0f));
        }
        else
        {
            // A new default band occupies the right side. The old band does
            // not move logically, so focus and its attachment stay on Band 1.
            CHECK(multiband->getFocusIndex() == 0);
            CHECK(bandPanel->getFocusBandNum() == 0);
            CHECK(bandPanel->getDriveKnob()->getParamID() == firstDriveID);
            CHECK(firstDrive->load() == Catch::Approx(61.0f));
            CHECK(firstDriveBypass->load() == Catch::Approx(0.0f));
            CHECK(secondDrive->load() == Catch::Approx(0.0f));
            CHECK(secondDriveBypass->load() == Catch::Approx(1.0f));
        }
    };

    SECTION("new band is inserted left of the focused band")
    {
        runInsertScenario(true);
    }

    SECTION("new band is inserted right of the focused band")
    {
        runInsertScenario(false);
    }
}

TEST_CASE("A newly added divider reaches the first audio block without a message-loop handoff",
          "[multiband][ui][add][processor][timing]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 8192;
    constexpr float hiddenFrequency = 200.0f;

    FireAudioProcessor editedProcessor;
    editedProcessor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(editedProcessor,
                         1,
                         { hiddenFrequency, 3000.0f, 7000.0f });

    const auto firstBandID = [](const juce::String& baseID)
    {
        return ParameterIDAndName::getIDString(baseID, 0);
    };
    setPlainParameter(editedProcessor, firstBandID(DRIVE_BYPASS_ID), 1.0f);
    setPlainParameter(editedProcessor, firstBandID(DRIVE_ID), 100.0f);
    setPlainParameter(editedProcessor, firstBandID(MODE_ID), 4.0f);
    setPlainParameter(editedProcessor, firstBandID(SAFE_ID), 0.0f);
    setPlainParameter(editedProcessor, firstBandID(LINKED_ID), 0.0f);
    setPlainParameter(editedProcessor, firstBandID(MIX_ID), 1.0f);
    setPlainParameter(editedProcessor, firstBandID(SHAPE_MIX_ID), 1.0f);
    editedProcessor.prepareToPlay(sampleRate, blockSize);

    auto editor = std::make_unique<FireAudioProcessorEditor>(editedProcessor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);

    // Insert on the right: the deliberately coloured old band stays left and
    // the new default band occupies the right. Do not pump the message loop
    // after this call; processBlock must observe the complete new topology.
    const auto insertionPoint = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * 0.80f,
        static_cast<float>(multiband->getHeight()) * 0.10f
    };
    static_cast<juce::Component&>(*multiband).mouseDown(
        makeMouseEvent(*multiband,
                       insertionPoint,
                       juce::ModifierKeys::leftButtonModifier));

    const auto* publishedFrequency = editedProcessor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    const auto* publishedBandCount = editedProcessor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(publishedFrequency != nullptr);
    REQUIRE(publishedBandCount != nullptr);
    REQUIRE(publishedBandCount->load() == Catch::Approx(2.0f));
    REQUIRE(publishedFrequency->load() > hiddenFrequency * 10.0f);

    FireAudioProcessor freshProcessor;
    FireAudioProcessor staleFrequencyControl;
    copyAllParameterValues(editedProcessor, freshProcessor);
    copyAllParameterValues(editedProcessor, staleFrequencyControl);
    setPlainParameter(staleFrequencyControl,
                      ParameterIDAndName::getIDString(FREQ_ID, 0),
                      hiddenFrequency);
    freshProcessor.prepareToPlay(sampleRate, blockSize);
    staleFrequencyControl.prepareToPlay(sampleRate, blockSize);

    auto editedOutput = makeCrossoverProbeInput(blockSize, sampleRate);
    auto freshOutput = editedOutput;
    auto staleOutput = editedOutput;
    juce::MidiBuffer editedMidi;
    juce::MidiBuffer freshMidi;
    juce::MidiBuffer staleMidi;
    editedProcessor.processBlock(editedOutput, editedMidi);
    freshProcessor.processBlock(freshOutput, freshMidi);
    staleFrequencyControl.processBlock(staleOutput, staleMidi);

    // Ignore the short crossover smoothing transient. The rest of this very
    // first block must follow the newly-published divider, not hidden freq1.
    const auto freshError = meanSquaredDifference(editedOutput,
                                                  freshOutput,
                                                  blockSize / 2);
    const auto staleError = meanSquaredDifference(editedOutput,
                                                  staleOutput,
                                                  blockSize / 2);
    CAPTURE(publishedFrequency->load(), freshError, staleError);
    REQUIRE(staleError > 1.0e-7);
    CHECK(freshError < staleError * 0.10);
}

TEST_CASE("A rapid add-delete cycle resets reused DSP slots even when the band count returns",
          "[multiband][ui][add][delete][processor][generation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 2048;

    FireAudioProcessor editedProcessor;
    editedProcessor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(editedProcessor, 1);
    configureDriveProbe(editedProcessor, 100.0f);
    setPlainParameter(editedProcessor, HQ_ID, 1.0f);
    editedProcessor.prepareToPlay(sampleRate, blockSize);

    // Leave substantial distortion/oversampling history in the original slot.
    // The subsequent UI operations intentionally happen with no audio block in
    // between, so NUM_BANDS is 1 both times the audio thread observes it.
    juce::MidiBuffer primeMidi;
    for (int block = 0; block < 4; ++block)
    {
        auto prime = makeDriveProbeInput(blockSize, sampleRate);
        editedProcessor.processBlock(prime, primeMidi);
    }

    auto editor = std::make_unique<FireAudioProcessorEditor>(editedProcessor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);

    // Insert a default band on the right, then immediately remove the old band
    // on the left. The new logical band reuses DSP slot zero while the final
    // band-count value is indistinguishable from its pre-edit value.
    const auto insertionPoint = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * 0.80f,
        static_cast<float>(multiband->getHeight()) * 0.10f
    };
    static_cast<juce::Component&>(*multiband).mouseDown(
        makeMouseEvent(*multiband,
                       insertionPoint,
                       juce::ModifierKeys::leftButtonModifier));
    auto closeButtons = getPositionedCloseButtons(*multiband);
    REQUIRE(closeButtons.size() == 2);
    closeButtons.front()->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    const auto* finalBandCount = editedProcessor.treeState.getRawParameterValue(NUM_BANDS_ID);
    const auto* replacementDrive = editedProcessor.treeState.getRawParameterValue(
        ParameterIDAndName::getIDString(DRIVE_ID, 0));
    REQUIRE(finalBandCount != nullptr);
    REQUIRE(replacementDrive != nullptr);
    REQUIRE(finalBandCount->load() == Catch::Approx(1.0f));
    REQUIRE(replacementDrive->load() == Catch::Approx(0.0f));

    FireAudioProcessor freshProcessor;
    copyAllParameterValues(editedProcessor, freshProcessor);
    freshProcessor.prepareToPlay(sampleRate, blockSize);

    // The published slot replacement now fades the old graph out, resets it
    // only at zero, keeps it muted for 1 ms, then fades the replacement in.
    // Feed silence through that complete 5+1+5 ms transaction before judging
    // the replacement slot; the audible fade-out may legitimately contain the
    // deliberately primed old tail.
    constexpr int topologyRampSamples = 240;
    constexpr int topologyWarmupSamples = 48;
    constexpr int topologyTransitionSamples = 2 * topologyRampSamples
                                            + topologyWarmupSamples;
    const int settledStart = topologyTransitionSamples
                           + static_cast<int>(std::ceil(
                                 editedProcessor.getTotalLatency()))
                           + 2;
    REQUIRE(settledStart < blockSize);

    juce::AudioBuffer<float> transitionEdited(2, blockSize);
    juce::AudioBuffer<float> transitionFresh(2, blockSize);
    transitionEdited.clear();
    transitionFresh.clear();
    juce::MidiBuffer editedMidi;
    juce::MidiBuffer freshMidi;
    editedProcessor.processBlock(transitionEdited, editedMidi);
    freshProcessor.processBlock(transitionFresh, freshMidi);

    const auto postWarmError = meanSquaredDifference(transitionEdited,
                                                     transitionFresh,
                                                     settledStart);
    const auto postWarmResidual = juce::jmax(
        transitionEdited.getMagnitude(0,
                                      settledStart,
                                      blockSize - settledStart),
        transitionEdited.getMagnitude(1,
                                      settledStart,
                                      blockSize - settledStart));

    juce::AudioBuffer<float> settledEdited(2, blockSize);
    juce::AudioBuffer<float> settledFresh(2, blockSize);
    settledEdited.clear();
    settledFresh.clear();
    editedProcessor.processBlock(settledEdited, editedMidi);
    freshProcessor.processBlock(settledFresh, freshMidi);
    const auto settledError = meanSquaredDifference(settledEdited,
                                                    settledFresh,
                                                    0);
    const auto settledResidual = juce::jmax(
        settledEdited.getMagnitude(0, 0, blockSize),
        settledEdited.getMagnitude(1, 0, blockSize));
    CAPTURE(settledStart,
            postWarmError,
            postWarmResidual,
            settledError,
            settledResidual);
    CHECK(postWarmError < 1.0e-12);
    CHECK(postWarmResidual < 1.0e-6f);
    CHECK(settledError < 1.0e-12);
    CHECK(settledResidual < 1.0e-6f);
}

TEST_CASE("Close controls remain hit-testable while crossing a divider child",
          "[multiband][ui][hover][delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* dividerGroup = findDescendant<FreqDividerGroup>(*multiband);
    REQUIRE(dividerGroup != nullptr);

    const auto closeButtons = getPositionedCloseButtons(*multiband);
    REQUIRE(closeButtons.size() == 2);
    auto& divider = dividerGroup->getVerticalLine();
    REQUIRE(divider.getWidth() >= 2);

    const auto checkMoveThroughDivider = [&](float localDividerX)
    {
        const auto localPosition = juce::Point<float> {
            localDividerX,
            static_cast<float>(divider.getHeight()) * 0.5f
        };
        multiband->mouseMove(makeMouseEvent(divider, localPosition));

        auto* visibleClose = getVisibleCloseButton(closeButtons);
        REQUIRE(visibleClose != nullptr);
        const auto closeCentre = visibleClose->getBounds().getCentre();
        CHECK(multiband->getComponentAt(closeCentre) == visibleClose);
    };

    // Both events originate from the overlapping divider child. Crossing from
    // one side to the other must transfer hover between bands without a frame
    // where the close control disappears or falls behind the divider group.
    checkMoveThroughDivider(1.0f);
    checkMoveThroughDivider(static_cast<float>(divider.getWidth() - 1));
}

TEST_CASE("Host band-count automation is authoritative over divider presentation state",
          "[multiband][ui][automation][focus]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 1);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);
    REQUIRE(getVisibleBandEnableButtonCount(*multiband) == 1);
    REQUIRE(getVisibleDividerCount(*multiband) == 0);

    // Simulate a host changing only the automatable DSP topology parameter.
    // The editor timer must reconcile its presentation without requiring the
    // legacy lineState parameters to arrive in a particular order.
    {
        NonParameterStateCapture host(processor);
        setPlainParameter(processor, NUM_BANDS_ID, 3.0f);
        editor->timerCallback();
        REQUIRE(host.states.size() == 1);

        FireAudioProcessor restored;
        restored.setStateInformation(host.states.front().getData(),
                                     static_cast<int>(host.states.front().getSize()));
        const auto* restoredBandCount = restored.treeState.getRawParameterValue(
            NUM_BANDS_ID);
        REQUIRE(restoredBandCount != nullptr);
        CHECK(restoredBandCount->load(std::memory_order_relaxed)
              == Catch::Approx(3.0f));
        for (int divider = 0; divider < 2; ++divider)
        {
            const auto* restoredLineState = restored.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(LINE_STATE_ID, divider));
            const auto* restoredFrequency = restored.treeState.getRawParameterValue(
                ParameterIDAndName::getIDString(FREQ_ID, divider));
            REQUIRE(restoredLineState != nullptr);
            REQUIRE(restoredFrequency != nullptr);
            CHECK(restoredLineState->load(std::memory_order_relaxed)
                  == Catch::Approx(1.0f));
            CHECK(restoredFrequency->load(std::memory_order_relaxed) > 21.0f);
        }
    }
    CHECK(getVisibleBandEnableButtonCount(*multiband) == 3);
    CHECK(getVisibleDividerCount(*multiband) == 2);
    CHECK(multiband->getFocusIndex() == 0);
    CHECK(bandPanel->getFocusBandNum() == 0);

    multiband->setFocusIndex(2);
    REQUIRE(multiband->getFocusIndex() == 2);
    REQUIRE(bandPanel->getFocusBandNum() == 2);

    // lineState is retained for preset/UI compatibility, but it must never
    // drive NUM_BANDS backwards. The next timer tick restores the view from
    // the authoritative parameter and keeps the selected active band valid.
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LINE_STATE_ID, 0),
                      0.0f);
    setPlainParameter(processor,
                      ParameterIDAndName::getIDString(LINE_STATE_ID, 1),
                      0.0f);
    // Deliver the attachment callbacks exactly as a host/message-thread update
    // would. Even after legacy divider state reaches the widgets, it must not
    // be allowed to rewrite the DSP's authoritative band count.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    const auto* numBands = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(numBands != nullptr);
    REQUIRE(numBands->load() == Catch::Approx(3.0f));
    editor->timerCallback();
    CHECK(numBands->load() == Catch::Approx(3.0f));
    CHECK(getVisibleBandEnableButtonCount(*multiband) == 3);
    CHECK(getVisibleDividerCount(*multiband) == 2);
    CHECK(multiband->getFocusIndex() == 2);
    CHECK(bandPanel->getFocusBandNum() == 2);

    // Reducing the DSP count clamps selection and rebinds every control in the
    // same timer callback; no stale hidden Band 3 attachment may remain.
    setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
    editor->timerCallback();
    CHECK(getVisibleBandEnableButtonCount(*multiband) == 1);
    CHECK(getVisibleDividerCount(*multiband) == 0);
    CHECK(multiband->getFocusIndex() == 0);
    CHECK(bandPanel->getFocusBandNum() == 0);
    REQUIRE(bandPanel->getDriveKnob() != nullptr);
    CHECK(bandPanel->getDriveKnob()->getParamID()
          == ParameterIDAndName::getIDString(DRIVE_ID, 0));
}

TEST_CASE("Host band-count expansion publishes usable fallback dividers",
          "[multiband][ui][automation][fallback][processor]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 8192;

    FireAudioProcessor editedProcessor;
    editedProcessor.hasUpdateCheckBeenPerformed = true;
    configureDriveProbe(editedProcessor, 100.0f);
    for (int band = 1; band < 3; ++band)
        setPlainParameter(editedProcessor,
                          ParameterIDAndName::getIDString(BAND_ENABLE_ID, band),
                          0.0f);

    const auto firstFrequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);
    const auto secondFrequencyID = ParameterIDAndName::getIDString(FREQ_ID, 1);
    const auto firstLineStateID = ParameterIDAndName::getIDString(LINE_STATE_ID, 0);
    const auto secondLineStateID = ParameterIDAndName::getIDString(LINE_STATE_ID, 1);
    const auto thirdLineStateID = ParameterIDAndName::getIDString(LINE_STATE_ID, 2);
    const auto* initialFirstFrequency = editedProcessor.treeState.getRawParameterValue(
        firstFrequencyID);
    const auto* initialSecondFrequency = editedProcessor.treeState.getRawParameterValue(
        secondFrequencyID);
    const auto* initialFirstLineState = editedProcessor.treeState.getRawParameterValue(
        firstLineStateID);
    const auto* initialSecondLineState = editedProcessor.treeState.getRawParameterValue(
        secondLineStateID);
    REQUIRE(initialFirstFrequency != nullptr);
    REQUIRE(initialSecondFrequency != nullptr);
    REQUIRE(initialFirstLineState != nullptr);
    REQUIRE(initialSecondLineState != nullptr);
    const float hiddenFirst = initialFirstFrequency->load();
    const float hiddenSecond = initialSecondFrequency->load();
    REQUIRE((! std::isfinite(hiddenFirst)
             || ! std::isfinite(hiddenSecond)
             || hiddenFirst < 40.0f
             || hiddenSecond > 10024.0f
             || hiddenFirst >= hiddenSecond));
    REQUIRE(initialFirstLineState->load() < 0.5f);
    REQUIRE(initialSecondLineState->load() < 0.5f);

    editedProcessor.prepareToPlay(sampleRate, blockSize);
    auto editor = std::make_unique<FireAudioProcessorEditor>(editedProcessor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);

    // A generic host exposes only NUM_BANDS. The editor must turn legacy
    // inactive sentinels into a complete, host-visible three-band topology.
    setPlainParameter(editedProcessor, NUM_BANDS_ID, 3.0f);
    editor->timerCallback();

    const auto* firstFrequency = editedProcessor.treeState.getRawParameterValue(firstFrequencyID);
    const auto* secondFrequency = editedProcessor.treeState.getRawParameterValue(secondFrequencyID);
    const auto* firstLineState = editedProcessor.treeState.getRawParameterValue(firstLineStateID);
    const auto* secondLineState = editedProcessor.treeState.getRawParameterValue(secondLineStateID);
    const auto* thirdLineState = editedProcessor.treeState.getRawParameterValue(thirdLineStateID);
    REQUIRE(firstFrequency != nullptr);
    REQUIRE(secondFrequency != nullptr);
    REQUIRE(firstLineState != nullptr);
    REQUIRE(secondLineState != nullptr);
    REQUIRE(thirdLineState != nullptr);

    const float fallbackFirst = firstFrequency->load();
    const float fallbackSecond = secondFrequency->load();
    REQUIRE(std::isfinite(fallbackFirst));
    REQUIRE(std::isfinite(fallbackSecond));
    REQUIRE(fallbackFirst >= 40.0f);
    REQUIRE(fallbackSecond <= 10024.0f);
    REQUIRE(fallbackFirst < fallbackSecond);
    CHECK(firstLineState->load() > 0.5f);
    CHECK(secondLineState->load() > 0.5f);
    CHECK(thirdLineState->load() < 0.5f);
    CHECK(getVisibleDividerCount(*multiband) == 2);

    // Move only the lower divider. Keep more than Multiband's 0.1 logarithmic
    // separation so this is an ordinary legal drag and cannot push line two.
    const float fallbackFirstX = transformToLog(fallbackFirst);
    const float fallbackSecondX = transformToLog(fallbackSecond);
    REQUIRE(fallbackSecondX - fallbackFirstX > 0.16f);
    const float draggedX = fallbackFirstX
                           + (fallbackSecondX - fallbackFirstX) * 0.45f;
    REQUIRE(fallbackSecondX - draggedX > 0.10f);
    multiband->dragLines(draggedX, 0);

    const float draggedFirst = firstFrequency->load();
    const float untouchedSecond = secondFrequency->load();
    REQUIRE(draggedFirst > fallbackFirst);
    REQUIRE(draggedFirst < untouchedSecond);
    CHECK(untouchedSecond == Catch::Approx(fallbackSecond).margin(1.0f));

    FireAudioProcessor freshProcessor;
    FireAudioProcessor staleFallbackControl;
    copyAllParameterValues(editedProcessor, freshProcessor);
    copyAllParameterValues(editedProcessor, staleFallbackControl);
    setPlainParameter(staleFallbackControl, firstFrequencyID, fallbackFirst);
    freshProcessor.prepareToPlay(sampleRate, blockSize);
    staleFallbackControl.prepareToPlay(sampleRate, blockSize);

    const double crossoverProbeFrequency = std::sqrt(
        static_cast<double>(fallbackFirst) * static_cast<double>(draggedFirst));
    const double highProbeFrequency = juce::jmin(
        sampleRate * 0.35,
        static_cast<double>(fallbackSecond) * 4.0);
    juce::AudioBuffer<float> input(2, blockSize);
    for (int sample = 0; sample < blockSize; ++sample)
    {
        const auto time = static_cast<double>(sample) / sampleRate;
        const auto value = static_cast<float>(
            0.18 * std::sin(juce::MathConstants<double>::twoPi
                            * crossoverProbeFrequency * time)
            + 0.07 * std::sin(juce::MathConstants<double>::twoPi
                              * highProbeFrequency * time));
        input.setSample(0, sample, value);
        input.setSample(1, sample, value);
    }

    auto editedOutput = input;
    auto freshOutput = input;
    auto staleOutput = input;
    juce::MidiBuffer editedMidi;
    juce::MidiBuffer freshMidi;
    juce::MidiBuffer staleMidi;
    editedProcessor.processBlock(editedOutput, editedMidi);
    freshProcessor.processBlock(freshOutput, freshMidi);
    staleFallbackControl.processBlock(staleOutput, staleMidi);

    const auto freshError = meanSquaredDifference(editedOutput,
                                                  freshOutput,
                                                  blockSize / 2);
    const auto staleError = meanSquaredDifference(editedOutput,
                                                  staleOutput,
                                                  blockSize / 2);
    CAPTURE(fallbackFirst,
            fallbackSecond,
            draggedFirst,
            untouchedSecond,
            crossoverProbeFrequency,
            freshError,
            staleError);
    REQUIRE(staleError > 1.0e-7);
    CHECK(freshError < staleError * 0.10);
}
