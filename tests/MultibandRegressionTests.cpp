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

    static std::function<void(int)> createAssignmentResultHandler(
        ModulatableSlider& slider)
    {
        return slider.createLfoAssignmentMenuResultHandler();
    }
};

struct MultibandPointerTestAccess
{
    static bool isDragging(const Multiband& multiband)
    {
        return multiband.isDragging;
    }

    static bool hasPrimaryDrag(const Multiband& multiband)
    {
        return multiband.primaryDragActive;
    }

    static void setTrackedPointerSource(
        Multiband& multiband,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        multiband.pointerSourceType = sourceType;
        multiband.pointerSourceIndex = sourceIndex;
    }

    static void setHoveredBand(Multiband& multiband, int bandIndex)
    {
        if (juce::isPositiveAndBelow(bandIndex, multiband.lineNum + 1))
        {
            const auto bounds = multiband.getBandBounds(bandIndex);
            multiband.updateHoveredBand(bounds.getCentre().roundToInt(), true);
        }
        else
        {
            multiband.updateHoveredBand({}, false);
        }
    }

    static float getBandHover(const Multiband& multiband, int bandIndex)
    {
        return multiband.bandHoverAnimations[static_cast<size_t>(bandIndex)]
            .current;
    }
};

struct VerticalLinePointerTestAccess
{
    static bool hasPrimaryDrag(const VerticalLine& divider)
    {
        return divider.primaryDragActive;
    }

    static void setTrackedPointerSource(
        VerticalLine& divider,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        divider.pointerSourceType = sourceType;
        divider.pointerSourceIndex = sourceIndex;
    }

    static void gainKeyboardFocus(VerticalLine& divider)
    {
        divider.focusGained(juce::Component::focusChangedByTabKey);
    }

    static void loseKeyboardFocus(VerticalLine& divider)
    {
        divider.focusLost(juce::Component::focusChangedDirectly);
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

class EditorResetOnProcessorCallback final : public juce::AudioProcessorListener
{
public:
    EditorResetOnProcessorCallback(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<FireAudioProcessorEditor>& editorToReset,
        int parameterIndexToObserve,
        bool resetForNonParameterState)
        : processor(processorToObserve),
          editor(editorToReset),
          parameterIndex(parameterIndexToObserve),
          resetOnNonParameterState(resetForNonParameterState)
    {
        processor.addListener(this);
    }

    ~EditorResetOnProcessorCallback() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (! resetOnNonParameterState
            && changedParameterIndex == parameterIndex)
            resetEditor();
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (resetOnNonParameterState && details.nonParameterStateChanged)
            resetEditor();
    }

    FireAudioProcessor& processor;
    std::unique_ptr<FireAudioProcessorEditor>& editor;
    int parameterIndex = -1;
    bool resetOnNonParameterState = false;
    bool didResetEditor = false;

private:
    void resetEditor()
    {
        if (editor == nullptr)
            return;

        didResetEditor = true;
        editor.reset();
    }
};

enum class ParameterCallbackStage
{
    begin,
    value,
    end
};

class EditorResetOnParameterStage final : public juce::AudioProcessorListener
{
public:
    EditorResetOnParameterStage(
        FireAudioProcessor& processorToObserve,
        std::unique_ptr<FireAudioProcessorEditor>& editorToReset,
        int parameterIndexToObserve,
        ParameterCallbackStage stageToObserve)
        : processor(processorToObserve),
          editor(editorToReset),
          parameterIndex(parameterIndexToObserve),
          stage(stageToObserve)
    {
        processor.addListener(this);
    }

    ~EditorResetOnParameterStage() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int changedParameterIndex,
                                        float) override
    {
        if (stage == ParameterCallbackStage::value)
            resetEditor(changedParameterIndex);
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,
                                                   int changedParameterIndex) override
    {
        if (stage == ParameterCallbackStage::begin)
            resetEditor(changedParameterIndex);
    }

    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,
                                                 int changedParameterIndex) override
    {
        if (stage == ParameterCallbackStage::end)
            resetEditor(changedParameterIndex);
    }

    bool didResetEditor = false;

private:
    void resetEditor(int changedParameterIndex)
    {
        if (changedParameterIndex != parameterIndex || editor == nullptr)
            return;

        didResetEditor = true;
        editor.reset();
    }

    FireAudioProcessor& processor;
    std::unique_ptr<FireAudioProcessorEditor>& editor;
    int parameterIndex = -1;
    ParameterCallbackStage stage;
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

void checkNoGestureActivity(const ParameterGestureCapture& capture)
{
    CHECK(capture.beginCount == 0);
    CHECK(capture.endCount == 0);
    CHECK(capture.valueChangeCount == 0);
    CHECK(capture.gestureDepth == 0);
    CHECK(capture.maximumGestureDepth == 0);
    CHECK(capture.minimumGestureDepth == 0);
    CHECK_FALSE(capture.valueChangedOutsideGesture);
    CHECK(capture.events.empty());
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

juce::MouseEvent makeDragMouseEvent(juce::Component& component,
                                    juce::Point<float> position,
                                    juce::Point<float> mouseDownPosition,
                                    juce::ModifierKeys modifiers)
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
             mouseDownPosition,
             time,
             1,
             true };
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
        if (closeButton != nullptr && closeButton->isPresented())
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

TEST_CASE("Crossover hit targets remain centred at default and doubled UI sizes",
          "[multiband][divider][ui][layout][scale][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1000.0f, 0.0f, 0.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);

    const auto checkLayoutAfterDrag = [&](int editorWidth,
                                          int editorHeight,
                                          float targetXPercent)
    {
        editor->setBounds(0, 0, editorWidth, editorHeight);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);
        REQUIRE(multiband->getWidth() > 0);
        const auto dividerGroups = getDividerGroupsByIndex(*multiband);
        REQUIRE(dividerGroups[0] != nullptr);

        multiband->dragLines(targetXPercent, 0);

        auto& divider = dividerGroups[0]->getVerticalLine();
        const float actualCentre =
            static_cast<float>(dividerGroups[0]->getX())
            + divider.getBounds().toFloat().getCentreX();
        const float authoritativeCentre =
            divider.getXPercent() * static_cast<float>(multiband->getWidth());
        const float requestedCentre =
            targetXPercent * static_cast<float>(multiband->getWidth());

        CAPTURE(editorWidth,
                editorHeight,
                multiband->getWidth(),
                dividerGroups[0]->getWidth(),
                divider.getWidth(),
                targetXPercent,
                divider.getXPercent(),
                requestedCentre,
                authoritativeCentre,
                actualCentre);

        // Integer-Hz parameter snapping may move the requested position by a
        // fraction of a pixel, while integer component bounds can contribute at
        // most another half pixel. The enlarged hit target must nevertheless
        // remain centred on the authoritative rail instead of shifting it by
        // half the difference between the old and new hit widths.
        CHECK(actualCentre
              == Catch::Approx(authoritativeCentre).margin(0.51f));
        CHECK(actualCentre == Catch::Approx(requestedCentre).margin(1.0f));

        const float centreBeforeRelayout = actualCentre;
        multiband->setLineRelatedBoundsByX();
        const float centreAfterRelayout =
            static_cast<float>(dividerGroups[0]->getX())
            + divider.getBounds().toFloat().getCentreX();
        CHECK(centreAfterRelayout
              == Catch::Approx(centreBeforeRelayout).margin(0.001f));
    };

    checkLayoutAfterDrag(1000, 500, 0.42f);
    checkLayoutAfterDrag(2000, 1000, 0.64f);
}

TEST_CASE("Crossover text gestures survive synchronous editor teardown",
          "[multiband][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1000.0f, 0.0f, 0.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);

    auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroups[0]);
    REQUIRE(frequencyText != nullptr);
    auto* label = findDescendant<juce::Label>(*frequencyText);
    REQUIRE(label != nullptr);
    REQUIRE(static_cast<bool>(label->onEditorShow));
    REQUIRE(static_cast<bool>(label->onEditorHide));

    const auto frequencyID = ParameterIDAndName::getIDString(FREQ_ID, 0);
    auto* frequencyParameter = processor.treeState.getParameter(frequencyID);
    REQUIRE(frequencyParameter != nullptr);
    const auto initialFrequency = frequencyParameter->convertFrom0to1(
        frequencyParameter->getValue());
    ParameterGestureCapture host(processor, frequencyParameter->getParameterIndex());

    const auto commitText = [label]
    {
        label->onEditorShow();
        label->setText("1.60 kHz", juce::dontSendNotification);
        auto editorHide = label->onEditorHide;
        editorHide();
    };

    SECTION("teardown from begin notification closes before any value")
    {
        EditorResetOnParameterStage resetter(
            processor,
            editor,
            frequencyParameter->getParameterIndex(),
            ParameterCallbackStage::begin);

        commitText();

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
        CHECK(frequencyParameter->convertFrom0to1(frequencyParameter->getValue())
              == Catch::Approx(initialFrequency));
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.valueChangeCount == 0);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
        CHECK(host.events == std::vector<char> { 'B', 'E' });
    }

    SECTION("teardown from value notification preserves begin value end ordering")
    {
        EditorResetOnParameterStage resetter(
            processor,
            editor,
            frequencyParameter->getParameterIndex(),
            ParameterCallbackStage::value);

        commitText();

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
        CHECK(frequencyParameter->convertFrom0to1(frequencyParameter->getValue())
              == Catch::Approx(1600.0f));
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.valueChangeCount >= 1);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
        REQUIRE(host.events.size() >= 3);
        CHECK(host.events.front() == 'B');
        CHECK(host.events.back() == 'E');
    }
}

TEST_CASE("Crossover sorting survives synchronous editor teardown",
          "[multiband][ui][automation][focus][sort][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 0.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);

    SECTION("focus notification")
    {
        bool callbackRan = false;
        multiband->setFocusChangedCallback(
            [&callbackRan, &editor](int)
            {
                callbackRan = true;
                editor.reset();
            });

        multiband->sortLines();

        CHECK(callbackRan);
        CHECK(editor == nullptr);
    }

    SECTION("frequency parameter notification")
    {
        const auto dividerGroups = getDividerGroupsByIndex(*multiband);
        REQUIRE(dividerGroups[0] != nullptr);
        REQUIRE(dividerGroups[1] != nullptr);

        // Make the presentation unsorted without publishing it. Sorting then
        // changes FREQ0 synchronously through its SliderAttachment.
        dividerGroups[0]->setFreq(5000.0f, juce::dontSendNotification);
        dividerGroups[1]->setFreq(2000.0f, juce::dontSendNotification);

        auto* frequencyParameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(FREQ_ID, 0));
        REQUIRE(frequencyParameter != nullptr);
        EditorResetOnProcessorCallback resetter(
            processor,
            editor,
            frequencyParameter->getParameterIndex(),
            false);

        multiband->sortLines();

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
    }
}

TEST_CASE("Crossover cascade finishes every gesture after editor teardown",
          "[multiband][ui][automation][gesture][lifecycle][cascade]")
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
    for (int dividerIndex = 0; dividerIndex < 3; ++dividerIndex)
    {
        frequencyParameters[static_cast<size_t>(dividerIndex)] =
            processor.treeState.getParameter(
                ParameterIDAndName::getIDString(FREQ_ID, dividerIndex));
        REQUIRE(frequencyParameters[static_cast<size_t>(dividerIndex)] != nullptr);
    }

    ParameterGestureCapture firstHost(
        processor, frequencyParameters[0]->getParameterIndex());
    ParameterGestureCapture secondHost(
        processor, frequencyParameters[1]->getParameterIndex());
    ParameterGestureCapture thirdHost(
        processor, frequencyParameters[2]->getParameterIndex());

    const std::array<double, 3> initialFrequencies {
        frequencyParameters[0]->convertFrom0to1(frequencyParameters[0]->getValue()),
        frequencyParameters[1]->convertFrom0to1(frequencyParameters[1]->getValue()),
        frequencyParameters[2]->convertFrom0to1(frequencyParameters[2]->getValue())
    };

    SECTION("deepest value teardown stops the recursive publication")
    {
        EditorResetOnParameterStage resetter(
            processor,
            editor,
            frequencyParameters[2]->getParameterIndex(),
            ParameterCallbackStage::value);

        auto& source = dividerGroups[0]->getVerticalLine();
        auto& sourceComponent = static_cast<juce::Component&>(source);
        const auto eventPosition = source.getLocalBounds().toFloat().getCentre();
        sourceComponent.mouseDown(
            makeMouseEvent(source,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));
        multiband->dragLines(0.70f, 0);

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
        CHECK(frequencyParameters[0]->convertFrom0to1(frequencyParameters[0]->getValue())
              == Catch::Approx(initialFrequencies[0]));
        CHECK(frequencyParameters[1]->convertFrom0to1(frequencyParameters[1]->getValue())
              == Catch::Approx(initialFrequencies[1]));
        CHECK(frequencyParameters[2]->convertFrom0to1(frequencyParameters[2]->getValue())
              != Catch::Approx(initialFrequencies[2]));
        checkNoGestureActivity(firstHost);
        checkNoGestureActivity(secondHost);
        checkBalancedGesture(thirdHost);
    }

    SECTION("first end teardown still finishes every touched parameter")
    {
        auto& source = dividerGroups[0]->getVerticalLine();
        auto& sourceComponent = static_cast<juce::Component&>(source);
        const auto eventPosition = source.getLocalBounds().toFloat().getCentre();
        sourceComponent.mouseDown(
            makeMouseEvent(source,
                           eventPosition,
                           juce::ModifierKeys::leftButtonModifier));
        multiband->dragLines(0.70f, 0);

        CHECK(firstHost.beginCount == 1);
        CHECK(secondHost.beginCount == 1);
        CHECK(thirdHost.beginCount == 1);
        CHECK(firstHost.endCount == 0);
        CHECK(secondHost.endCount == 0);
        CHECK(thirdHost.endCount == 0);

        EditorResetOnParameterStage resetter(
            processor,
            editor,
            frequencyParameters[0]->getParameterIndex(),
            ParameterCallbackStage::end);
        sourceComponent.mouseUp(makeMouseEvent(source, eventPosition));

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
        checkBalancedGesture(firstHost);
        checkBalancedGesture(secondHost);
        checkBalancedGesture(thirdHost);
    }
}

TEST_CASE("VerticalLine stops value publication when its owner is removed",
          "[multiband][ui][automation][gesture][lifecycle][vertical-line]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto divider = std::make_unique<VerticalLine>();
    divider->setRange(40.0, 10024.0, 1.0);
    divider->setValue(1000.0, juce::dontSendNotification);

    bool changeCallbackRan = false;
    const auto lifetimeToken = std::make_shared<int>(0);
    divider->setParameterGestureCallbacks(
        [] {},
        [&divider, &changeCallbackRan, lifetimeToken]
        {
            changeCallbackRan = true;
            divider.reset();
            return lifetimeToken;
        },
        [] {});

    auto* dividerToEdit = divider.get();
    REQUIRE(dividerToEdit != nullptr);
    dividerToEdit->setValueAsPartOfGesture(1600.0,
                                           juce::sendNotificationSync);

    CHECK(changeCallbackRan);
    CHECK(divider == nullptr);
}

TEST_CASE("Crossover controls reject popup and auxiliary pointer gestures",
          "[multiband][ui][automation][gesture][input]")
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
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);

    auto& divider = dividerGroups[0]->getVerticalLine();
    auto& dividerComponent = static_cast<juce::Component&>(divider);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    const auto* bandCount = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(bandCount != nullptr);

    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor, frequencyParameter->getParameterIndex());

    const auto exerciseRejectedGesture = [&](const char* description,
                                             juce::ModifierKeys downModifiers,
                                             juce::ModifierKeys upModifiers)
    {
        INFO(description);

        const auto addPoint = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * 0.82f,
            static_cast<float>(multiband->getHeight()) * 0.10f
        };
        multibandComponent.mouseDown(
            makeMouseEvent(*multiband, addPoint, downModifiers));
        CHECK_FALSE(MultibandPointerTestAccess::isDragging(*multiband));
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        multibandComponent.mouseUp(
            makeMouseEvent(*multiband, addPoint, upModifiers));
        CHECK(bandCount->load(std::memory_order_relaxed) == Catch::Approx(2.0f));

        const auto focusPoint = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * 0.82f,
            static_cast<float>(multiband->getHeight()) * 0.70f
        };
        multibandComponent.mouseDown(
            makeMouseEvent(*multiband, focusPoint, downModifiers));
        multibandComponent.mouseUp(
            makeMouseEvent(*multiband, focusPoint, upModifiers));
        CHECK(multiband->getFocusIndex() == 0);

        const auto initialX = divider.getXPercent();
        const auto initialFrequency = divider.getValue();
        const auto downPosition = divider.getLocalBounds().toFloat().getCentre();
        const auto downEvent = makeMouseEvent(divider,
                                               downPosition,
                                               downModifiers);
        dividerComponent.mouseDown(downEvent);
        multibandComponent.mouseDown(downEvent);
        CHECK_FALSE(MultibandPointerTestAccess::isDragging(*multiband));
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));

        const auto targetInMultiband = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * 0.75f,
            static_cast<float>(multiband->getHeight()) * 0.50f
        };
        const auto targetInDivider = divider.getLocalPoint(multiband,
                                                            targetInMultiband);
        const auto dragEvent = makeDragMouseEvent(divider,
                                                   targetInDivider,
                                                   downPosition,
                                                   downModifiers);
        dividerComponent.mouseDrag(dragEvent);
        multibandComponent.mouseDrag(dragEvent);

        const auto upEvent = makeMouseEvent(divider,
                                            targetInDivider,
                                            upModifiers);
        dividerComponent.mouseUp(upEvent);
        multibandComponent.mouseUp(upEvent);

        CHECK(divider.getXPercent() == Catch::Approx(initialX));
        CHECK(divider.getValue() == Catch::Approx(initialFrequency));
        CHECK_FALSE(MultibandPointerTestAccess::isDragging(*multiband));
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        checkNoGestureActivity(host);
    };

    exerciseRejectedGesture(
        "physical right click",
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        {});
    exerciseRejectedGesture(
        "middle click",
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        {});

#if JUCE_MAC
    exerciseRejectedGesture(
        "macOS Control-click",
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::ctrlModifier },
        juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
#endif
}

TEST_CASE("Crossover controls replace stale ownership from the same pointer source",
          "[multiband][ui][automation][gesture][input][stale][source]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);

    auto& divider = dividerGroups[0]->getVerticalLine();
    auto& dividerComponent = static_cast<juce::Component&>(divider);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor,
                                 frequencyParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto secondary = juce::ModifierKeys {
        juce::ModifierKeys::rightButtonModifier
    };
    const auto dividerCentre = divider.getLocalBounds().toFloat().getCentre();
    const auto dispatchDown = [&](juce::ModifierKeys modifiers)
    {
        const auto event = makeMouseEvent(divider, dividerCentre, modifiers);
        dividerComponent.mouseDown(event);
        multibandComponent.mouseDown(event);
    };
    const auto dispatchDrag = [&](float xPercent,
                                  juce::ModifierKeys modifiers)
    {
        const auto targetInMultiband = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * xPercent,
            static_cast<float>(multiband->getHeight()) * 0.50f
        };
        const auto targetInDivider = divider.getLocalPoint(multiband,
                                                            targetInMultiband);
        const auto event = makeDragMouseEvent(divider,
                                               targetInDivider,
                                               dividerCentre,
                                               modifiers);
        dividerComponent.mouseDrag(event);
        multibandComponent.mouseDrag(event);
    };
    const auto dispatchUp = [&]
    {
        const auto event = makeMouseEvent(divider, dividerCentre);
        dividerComponent.mouseUp(event);
        multibandComponent.mouseUp(event);
    };

    dispatchDown(primary);
    dispatchDrag(0.62f, primary);
    const auto acceptedFrequency = frequencyParameter->convertFrom0to1(
        frequencyParameter->getValue());
    REQUIRE(host.beginCount == 1);
    REQUIRE(host.endCount == 0);
    REQUIRE(host.valueChangeCount >= 1);

    SECTION("a rejected down closes the old gesture and cannot keep dragging")
    {
        dispatchDown(secondary);
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));

        dispatchDrag(0.78f, secondary);
        dispatchUp();
        CHECK(frequencyParameter->convertFrom0to1(frequencyParameter->getValue())
              == Catch::Approx(acceptedFrequency));
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
    }

    SECTION("a new primary down closes the old gesture before starting another")
    {
        dispatchDown(primary);
        // Crossover host gestures are lazy: a fresh pointer owns the UI now,
        // but its host begin is emitted only when the first value is written.
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));

        dispatchDrag(0.72f, primary);
        CHECK(host.beginCount == 2);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 1);
        dispatchUp();
        CHECK(host.beginCount == 2);
        CHECK(host.endCount == 2);
        CHECK(host.gestureDepth == 0);
        CHECK(host.maximumGestureDepth == 1);
        CHECK(host.minimumGestureDepth == 0);
        CHECK_FALSE(host.valueChangedOutsideGesture);
    }

    SECTION("a parent-only mouseUp still closes the divider gesture")
    {
        const auto event = makeMouseEvent(divider, dividerCentre);
        multibandComponent.mouseUp(event);

        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        CHECK_FALSE(VerticalLinePointerTestAccess::hasPrimaryDrag(divider));
        CHECK_FALSE(host.valueChangedOutsideGesture);
    }

    SECTION("a buttonless move recovers an omitted mouseUp")
    {
        multibandComponent.mouseMove(makeMouseEvent(divider, dividerCentre));

        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        CHECK_FALSE(VerticalLinePointerTestAccess::hasPrimaryDrag(divider));
        CHECK_FALSE(host.valueChangedOutsideGesture);
    }
}

TEST_CASE("Hiding Multiband balances an active crossover gesture",
          "[multiband][ui][automation][gesture][visibility][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    auto& divider = dividerGroups[0]->getVerticalLine();
    auto& dividerComponent = static_cast<juce::Component&>(divider);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor,
                                 frequencyParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto dividerCentre = divider.getLocalBounds().toFloat().getCentre();
    const auto down = makeMouseEvent(divider, dividerCentre, primary);
    dividerComponent.mouseDown(down);
    multibandComponent.mouseDown(down);

    const auto targetInMultiband = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * 0.62f,
        static_cast<float>(multiband->getHeight()) * 0.50f
    };
    const auto targetInDivider = divider.getLocalPoint(multiband,
                                                        targetInMultiband);
    const auto drag = makeDragMouseEvent(divider,
                                          targetInDivider,
                                          dividerCentre,
                                          primary);
    dividerComponent.mouseDrag(drag);
    multibandComponent.mouseDrag(drag);
    REQUIRE(host.beginCount == 1);
    REQUIRE(host.endCount == 0);
    REQUIRE(host.valueChangeCount >= 1);
    REQUIRE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
    REQUIRE(VerticalLinePointerTestAccess::hasPrimaryDrag(divider));

    SECTION("visibility boundary")
    {
        multiband->setVisible(false);

        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        CHECK_FALSE(VerticalLinePointerTestAccess::hasPrimaryDrag(divider));
        CHECK_FALSE(host.valueChangedOutsideGesture);

        multiband->setVisible(true);
        const auto delayedUp = makeMouseEvent(divider, targetInDivider);
        dividerComponent.mouseUp(delayedUp);
        multibandComponent.mouseUp(delayedUp);
        CHECK(host.beginCount == 1);
        CHECK(host.endCount == 1);
        CHECK(host.gestureDepth == 0);
    }

    SECTION("gesture-end synchronously closes the editor")
    {
        EditorResetOnParameterStage resetter(
            processor,
            editor,
            frequencyParameter->getParameterIndex(),
            ParameterCallbackStage::end);

        multiband->setVisible(false);

        CHECK(resetter.didResetEditor);
        CHECK(editor == nullptr);
        checkBalancedGesture(host);
    }
}

TEST_CASE("Hiding Multiband clears every band-button press",
          "[multiband][ui][button][visibility][lifecycle][regression]")
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
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("enable")
    {
        auto& button = multiband->getEnableButton(0);
        auto& component = static_cast<juce::Component&>(button);
        const auto originalState = button.getToggleState();
        component.mouseDown(makeMouseEvent(
            component, component.getLocalBounds().toFloat().getCentre(), primary));
        REQUIRE(button.isDown());

        multiband->setVisible(false);

        CHECK_FALSE(button.isDown());
        multiband->setVisible(true);
        component.mouseUp(makeMouseEvent(
            component, component.getLocalBounds().toFloat().getCentre()));
        CHECK(button.getToggleState() == originalState);
    }

    SECTION("solo")
    {
        SoloButton* button = nullptr;
        for (auto* child : multiband->getChildren())
            if (auto* candidate = dynamic_cast<SoloButton*>(child);
                candidate != nullptr
                && candidate->isVisible()
                && ! candidate->getBounds().isEmpty())
            {
                button = candidate;
                break;
            }

        REQUIRE(button != nullptr);
        auto& component = static_cast<juce::Component&>(*button);
        const auto originalState = button->getToggleState();
        component.mouseDown(makeMouseEvent(
            component, component.getLocalBounds().toFloat().getCentre(), primary));
        REQUIRE(button->isDown());

        multiband->setVisible(false);

        CHECK_FALSE(button->isDown());
        multiband->setVisible(true);
        component.mouseUp(makeMouseEvent(
            component, component.getLocalBounds().toFloat().getCentre()));
        CHECK(button->getToggleState() == originalState);
    }

    SECTION("close")
    {
        const auto closeButtons = getPositionedCloseButtons(*multiband);
        REQUIRE_FALSE(closeButtons.empty());
        auto* button = closeButtons.front();
        REQUIRE(button != nullptr);
        button->setPresented(true, false);
        auto& component = static_cast<juce::Component&>(*button);
        component.mouseDown(makeMouseEvent(
            component, component.getLocalBounds().toFloat().getCentre(), primary));
        REQUIRE(button->isDown());

        multiband->setVisible(false);

        CHECK_FALSE(button->isDown());
        CHECK_FALSE(button->isPresented());
    }
}

TEST_CASE("A foreign pointer cannot steal a crossover gesture through another divider",
          "[multiband][ui][automation][gesture][input][source][multitouch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    REQUIRE(dividerGroups[1] != nullptr);

    auto& first = dividerGroups[0]->getVerticalLine();
    auto& second = dividerGroups[1]->getVerticalLine();
    auto& firstComponent = static_cast<juce::Component&>(first);
    auto& secondComponent = static_cast<juce::Component&>(second);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* firstParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    auto* secondParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 1));
    REQUIRE(firstParameter != nullptr);
    REQUIRE(secondParameter != nullptr);
    ParameterGestureCapture firstHost(processor,
                                      firstParameter->getParameterIndex());
    ParameterGestureCapture secondHost(processor,
                                       secondParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto firstCentre = first.getLocalBounds().toFloat().getCentre();
    const auto secondCentre = second.getLocalBounds().toFloat().getCentre();
    const auto firstDown = makeMouseEvent(first, firstCentre, primary);
    firstComponent.mouseDown(firstDown);
    multibandComponent.mouseDown(firstDown);

    const auto targetInMultiband = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * 0.45f,
        static_cast<float>(multiband->getHeight()) * 0.50f
    };
    const auto targetInFirst = first.getLocalPoint(multiband,
                                                    targetInMultiband);
    const auto firstDrag = makeDragMouseEvent(first,
                                               targetInFirst,
                                               firstCentre,
                                               primary);
    firstComponent.mouseDrag(firstDrag);
    multibandComponent.mouseDrag(firstDrag);
    REQUIRE(firstHost.beginCount == 1);
    REQUIRE(firstHost.endCount == 0);
    REQUIRE(firstHost.valueChangeCount >= 1);
    const auto acceptedFirstValue = firstParameter->getValue();
    const auto acceptedSecondValue = secondParameter->getValue();

    // Even the owning source cannot redirect an established drag stream to a
    // sibling divider without a new mouseDown lifecycle boundary.
    const auto redirectedTargetInSecond = second.getLocalPoint(
        multiband,
        juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * 0.72f,
            static_cast<float>(multiband->getHeight()) * 0.50f
        });
    const auto redirectedDrag = makeDragMouseEvent(second,
                                                    redirectedTargetInSecond,
                                                    secondCentre,
                                                    primary);
    secondComponent.mouseDrag(redirectedDrag);
    multibandComponent.mouseDrag(redirectedDrag);
    CHECK(firstParameter->getValue() == Catch::Approx(acceptedFirstValue));
    CHECK(secondParameter->getValue() == Catch::Approx(acceptedSecondValue));
    checkNoGestureActivity(secondHost);

    // The public test event factory uses the main mouse source. Rebind the
    // accepted gesture to a synthetic touch identity so the next main-mouse
    // event reproduces an interleaved foreign source deterministically.
    const auto mainSource = juce::Desktop::getInstance().getMainMouseSource();
    constexpr auto foreignType = juce::MouseInputSource::touch;
    const int foreignIndex = mainSource.getIndex() + 17;
    VerticalLinePointerTestAccess::setTrackedPointerSource(first,
                                                            foreignType,
                                                            foreignIndex);
    MultibandPointerTestAccess::setTrackedPointerSource(*multiband,
                                                        foreignType,
                                                        foreignIndex);

    const auto secondDown = makeMouseEvent(second, secondCentre, primary);
    secondComponent.mouseDown(secondDown);
    multibandComponent.mouseDown(secondDown);

    CHECK(VerticalLinePointerTestAccess::hasPrimaryDrag(first));
    CHECK_FALSE(VerticalLinePointerTestAccess::hasPrimaryDrag(second));
    CHECK(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
    CHECK(firstHost.beginCount == 1);
    CHECK(firstHost.endCount == 0);
    checkNoGestureActivity(secondHost);

    const auto secondDrag = makeDragMouseEvent(second,
                                                redirectedTargetInSecond,
                                                secondCentre,
                                                primary);
    secondComponent.mouseDrag(secondDrag);
    multibandComponent.mouseDrag(secondDrag);
    CHECK(firstParameter->getValue() == Catch::Approx(acceptedFirstValue));
    CHECK(secondParameter->getValue() == Catch::Approx(acceptedSecondValue));

    VerticalLinePointerTestAccess::setTrackedPointerSource(
        first, mainSource.getType(), mainSource.getIndex());
    MultibandPointerTestAccess::setTrackedPointerSource(
        *multiband, mainSource.getType(), mainSource.getIndex());
    const auto firstUp = makeMouseEvent(first, firstCentre);
    firstComponent.mouseUp(firstUp);
    multibandComponent.mouseUp(firstUp);

    CHECK(firstHost.endCount == 1);
    CHECK(firstHost.gestureDepth == 0);

    // Always dispatch a release to the rejected target too. With correct
    // ownership this is a no-op; it also keeps a failing implementation from
    // leaking test-only gesture state into destruction.
    const auto secondUp = makeMouseEvent(second, secondCentre);
    secondComponent.mouseUp(secondUp);
    multibandComponent.mouseUp(secondUp);

    checkBalancedGesture(firstHost);
    checkNoGestureActivity(secondHost);
}

TEST_CASE("Crossover stale-pointer recovery survives synchronous editor teardown",
          "[multiband][ui][automation][gesture][input][stale][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    REQUIRE(dividerGroups[1] != nullptr);

    auto& first = dividerGroups[0]->getVerticalLine();
    auto& second = dividerGroups[1]->getVerticalLine();
    auto& firstComponent = static_cast<juce::Component&>(first);
    auto& secondComponent = static_cast<juce::Component&>(second);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* firstParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    auto* secondParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 1));
    REQUIRE(firstParameter != nullptr);
    REQUIRE(secondParameter != nullptr);
    ParameterGestureCapture firstHost(processor,
                                      firstParameter->getParameterIndex());
    ParameterGestureCapture secondHost(processor,
                                       secondParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto firstCentre = first.getLocalBounds().toFloat().getCentre();
    const auto firstDown = makeMouseEvent(first, firstCentre, primary);
    firstComponent.mouseDown(firstDown);
    multibandComponent.mouseDown(firstDown);

    const auto targetInMultiband = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * 0.45f,
        static_cast<float>(multiband->getHeight()) * 0.50f
    };
    const auto targetInFirst = first.getLocalPoint(multiband,
                                                    targetInMultiband);
    const auto firstDrag = makeDragMouseEvent(first,
                                               targetInFirst,
                                               firstCentre,
                                               primary);
    firstComponent.mouseDrag(firstDrag);
    multibandComponent.mouseDrag(firstDrag);
    REQUIRE(firstHost.beginCount == 1);
    REQUIRE(firstHost.endCount == 0);
    REQUIRE(firstHost.valueChangeCount >= 1);

    EditorResetOnParameterStage resetter(
        processor,
        editor,
        firstParameter->getParameterIndex(),
        ParameterCallbackStage::end);

    SECTION("parent-only release")
    {
        // Skip VerticalLine::mouseUp to exercise the parent's fallback. Its
        // final gesture-end callback deletes multiband and the editor.
        multibandComponent.mouseUp(makeMouseEvent(first, firstCentre));
    }

    SECTION("same-source replacement on another divider")
    {
        // JUCE invokes the target before recursive mouse listeners. The
        // admission callback must stop immediately when ending the old line
        // synchronously destroys this new target too.
        const auto secondCentre = second.getLocalBounds().toFloat().getCentre();
        secondComponent.mouseDown(makeMouseEvent(second,
                                                  secondCentre,
                                                  primary));
    }

    CHECK(resetter.didResetEditor);
    CHECK(editor == nullptr);
    checkBalancedGesture(firstHost);
    checkNoGestureActivity(secondHost);
}

TEST_CASE("Topology automation closes a crossover gesture before hiding its divider",
          "[multiband][ui][automation][gesture][topology][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    REQUIRE(dividerGroups[1] != nullptr);

    enum class TopologyChange
    {
        bandCount,
        legacyLineState
    };
    int activeDividerIndex = -1;
    TopologyChange topologyChange = TopologyChange::bandCount;

    SECTION("NUM_BANDS collapses the active divider")
    {
        activeDividerIndex = 1;
        topologyChange = TopologyChange::bandCount;
    }

    SECTION("legacy LINE_STATE hides the active divider")
    {
        activeDividerIndex = 0;
        topologyChange = TopologyChange::legacyLineState;
    }

    REQUIRE(juce::isPositiveAndBelow(activeDividerIndex, 2));
    auto& dividerGroup = *dividerGroups[static_cast<size_t>(activeDividerIndex)];
    auto& divider = dividerGroup.getVerticalLine();
    auto& dividerComponent = static_cast<juce::Component&>(divider);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, activeDividerIndex));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor,
                                 frequencyParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto dividerCentre = divider.getLocalBounds().toFloat().getCentre();
    const auto down = makeMouseEvent(divider, dividerCentre, primary);
    dividerComponent.mouseDown(down);
    multibandComponent.mouseDown(down);

    const float acceptedX = activeDividerIndex == 0 ? 0.45f : 0.72f;
    const auto targetInMultiband = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * acceptedX,
        static_cast<float>(multiband->getHeight()) * 0.50f
    };
    const auto targetInDivider = divider.getLocalPoint(multiband,
                                                        targetInMultiband);
    const auto drag = makeDragMouseEvent(divider,
                                          targetInDivider,
                                          dividerCentre,
                                          primary);
    dividerComponent.mouseDrag(drag);
    multibandComponent.mouseDrag(drag);
    REQUIRE(host.beginCount == 1);
    REQUIRE(host.endCount == 0);
    REQUIRE(host.valueChangeCount >= 1);

    if (topologyChange == TopologyChange::bandCount)
    {
        setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
        multiband->synchroniseBandCountFromParameter();
    }
    else
    {
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LINE_STATE_ID,
                                             activeDividerIndex),
            0.0f);
        CHECK_FALSE(dividerGroup.isVisible());

        // LINE_STATE arrives through ButtonAttachment::setValue. Ownership is
        // revoked in that callback, while the host end waits one message turn
        // so it cannot delete the attachment under its ScopedValueSetter.
        CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    if (topologyChange == TopologyChange::bandCount)
        CHECK_FALSE(dividerGroup.isVisible());
    CHECK(host.beginCount == 1);
    CHECK(host.endCount == 1);
    CHECK(host.gestureDepth == 0);
    CHECK_FALSE(MultibandPointerTestAccess::hasPrimaryDrag(*multiband));
    CHECK_FALSE(VerticalLinePointerTestAccess::hasPrimaryDrag(divider));
    CHECK_FALSE(host.valueChangedOutsideGesture);

    // Keep failing implementations balanced during teardown; with the fixed
    // lifecycle this is idempotent and emits no additional host notification.
    multiband->dismissTransientUi();
    CHECK(host.beginCount == 1);
    CHECK(host.endCount == 1);
    CHECK(host.gestureDepth == 0);
    CHECK(host.maximumGestureDepth == 1);
    CHECK(host.minimumGestureDepth == 0);
}

TEST_CASE("Deferred divider cleanup cannot end a reactivated slot gesture",
          "[multiband][ui][automation][gesture][topology][lifecycle][session][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1800.0f, 0.0f, 0.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->setVisible(true);
    editor->stopTimer();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);

    auto& dividerGroup = *dividerGroups[0];
    auto& divider = dividerGroup.getVerticalLine();
    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, 0));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor,
                                 frequencyParameter->getParameterIndex());

    const auto publishInsideGesture = [&](double frequency)
    {
        divider.beginParameterGesture();
        divider.setValueAsPartOfGesture(frequency,
                                        juce::sendNotificationSync);
    };

    publishInsideGesture(2400.0);
    REQUIRE(host.beginCount == 1);
    REQUIRE(host.endCount == 0);

    const auto lineStateID = ParameterIDAndName::getIDString(LINE_STATE_ID, 0);
    setPlainParameter(processor, lineStateID, 0.0f);
    REQUIRE_FALSE(dividerGroup.isVisible());

    // Reuse the same fixed divider component before the old attachment-stack
    // cleanup gets its message turn, then replace the abandoned edit with a
    // valid gesture in the new visible session.
    setPlainParameter(processor, lineStateID, 1.0f);
    REQUIRE(dividerGroup.isVisible());
    divider.endParameterGesture();
    publishInsideGesture(3600.0);
    REQUIRE(host.beginCount == 2);
    REQUIRE(host.endCount == 1);

    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    CHECK(dividerGroup.isVisible());
    CHECK(host.beginCount == 2);
    CHECK(host.endCount == 1);

    divider.endParameterGesture();
    CHECK(host.beginCount == 2);
    CHECK(host.endCount == 2);
    CHECK(host.gestureDepth == 0);
}

TEST_CASE("Topology-driven crossover teardown survives synchronous editor closure",
          "[multiband][ui][automation][gesture][topology][lifecycle][teardown]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->stopTimer();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    REQUIRE(dividerGroups[1] != nullptr);

    enum class TopologyChange
    {
        bandCount,
        legacyLineState
    };
    int activeDividerIndex = -1;
    TopologyChange topologyChange = TopologyChange::bandCount;

    SECTION("NUM_BANDS synchronous reconciliation")
    {
        activeDividerIndex = 1;
        topologyChange = TopologyChange::bandCount;
    }

    SECTION("LINE_STATE deferred attachment cleanup")
    {
        activeDividerIndex = 0;
        topologyChange = TopologyChange::legacyLineState;
    }

    REQUIRE(juce::isPositiveAndBelow(activeDividerIndex, 2));
    auto& divider = dividerGroups[static_cast<size_t>(activeDividerIndex)]
                        ->getVerticalLine();
    auto& dividerComponent = static_cast<juce::Component&>(divider);
    auto& multibandComponent = static_cast<juce::Component&>(*multiband);
    auto* frequencyParameter = processor.treeState.getParameter(
        ParameterIDAndName::getIDString(FREQ_ID, activeDividerIndex));
    REQUIRE(frequencyParameter != nullptr);
    ParameterGestureCapture host(processor,
                                 frequencyParameter->getParameterIndex());

    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto dividerCentre = divider.getLocalBounds().toFloat().getCentre();
    const auto down = makeMouseEvent(divider, dividerCentre, primary);
    dividerComponent.mouseDown(down);
    multibandComponent.mouseDown(down);

    const float acceptedX = activeDividerIndex == 0 ? 0.45f : 0.72f;
    const auto targetInMultiband = juce::Point<float> {
        static_cast<float>(multiband->getWidth()) * acceptedX,
        static_cast<float>(multiband->getHeight()) * 0.50f
    };
    const auto targetInDivider = divider.getLocalPoint(multiband,
                                                        targetInMultiband);
    const auto drag = makeDragMouseEvent(divider,
                                          targetInDivider,
                                          dividerCentre,
                                          primary);
    dividerComponent.mouseDrag(drag);
    multibandComponent.mouseDrag(drag);
    REQUIRE(host.beginCount == 1);
    REQUIRE(host.endCount == 0);
    REQUIRE(host.valueChangeCount >= 1);

    EditorResetOnParameterStage resetter(
        processor,
        editor,
        frequencyParameter->getParameterIndex(),
        ParameterCallbackStage::end);

    if (topologyChange == TopologyChange::bandCount)
    {
        setPlainParameter(processor, NUM_BANDS_ID, 1.0f);
        multiband->synchroniseBandCountFromParameter();
    }
    else
    {
        setPlainParameter(
            processor,
            ParameterIDAndName::getIDString(LINE_STATE_ID,
                                             activeDividerIndex),
            0.0f);

        // The attachment callback must return before gesture end is allowed
        // to delete the attachment and its owning editor. Deterministically
        // reproduce the competing 60 Hz reconciliation before delivering the
        // queued cleanup: NUM_BANDS re-shows the compatibility-hidden slot,
        // but the old frequency gesture must still be ended by its token.
        CHECK(editor != nullptr);
        multiband->synchroniseBandCountFromParameter();
        REQUIRE(editor != nullptr);
        CHECK(dividerGroups[static_cast<size_t>(activeDividerIndex)]->isVisible());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    CHECK(resetter.didResetEditor);
    CHECK(editor == nullptr);
    checkBalancedGesture(host);
}

TEST_CASE("Fallback crossover publication survives editor closure at every UI stage",
          "[multiband][ui][automation][fallback][topology][lifecycle][teardown]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    struct ClosureScenario
    {
        const char* description = nullptr;
        juce::String parameterID;
    };

    const std::array<ClosureScenario, 4> scenarios {{
        { "first fallback frequency", ParameterIDAndName::getIDString(FREQ_ID, 0) },
        { "second fallback frequency", ParameterIDAndName::getIDString(FREQ_ID, 1) },
        { "first compatibility line state", ParameterIDAndName::getIDString(LINE_STATE_ID, 0) },
        { "second compatibility line state", ParameterIDAndName::getIDString(LINE_STATE_ID, 1) },
    }};

    for (const auto& scenario : scenarios)
    DYNAMIC_SECTION(scenario.description)
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 1, { 21.0f, 21.0f, 21.0f });

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);

        auto* observedParameter = processor.treeState.getParameter(
            scenario.parameterID);
        REQUIRE(observedParameter != nullptr);
        const auto generationBefore =
            processor.getMultibandTopologyGenerationForTesting();
        REQUIRE((generationBefore & 1u) == 0u);

        // A generic host changes only the authoritative count. Reconciliation
        // must publish usable FREQ/LINE_STATE fallbacks, and any one of those
        // synchronous notifications is allowed to close this editor.
        setPlainParameter(processor, NUM_BANDS_ID, 3.0f);
        EditorResetOnProcessorCallback closureListener(
            processor,
            editor,
            observedParameter->getParameterIndex(),
            false);

        multiband->synchroniseBandCountFromParameter();

        CHECK(closureListener.didResetEditor);
        CHECK(editor == nullptr);
        const auto generationAfter =
            processor.getMultibandTopologyGenerationForTesting();
        CHECK((generationAfter & 1u) == 0u);
        CHECK(generationAfter == generationBefore + 2u);
        CHECK_FALSE(processor.isMultibandTopologyEditInProgress());
        CHECK(processor.tryAcquireMultibandTopologyWriterLockForTesting());
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
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
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
        CHECK(label->getText() == "2.5 kHz");
    }

    SECTION("keyboard nudges use the same ordered cross-divider cascade")
    {
        auto& source = dividerGroups[0]->getVerticalLine();
        source.grabKeyboardFocus();
        REQUIRE(source.hasKeyboardFocus(true));
        CHECK(source.keyPressed(
            juce::KeyPress { juce::KeyPress::rightKey }));

        checkThreeParameterCascade();
    }

    SECTION("accessible values use the same ordered cross-divider cascade")
    {
        auto& source = dividerGroups[0]->getVerticalLine();
        auto* accessibility = source.getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        auto* value = accessibility->getValueInterface();
        REQUIRE(value != nullptr);
        REQUIRE_FALSE(value->isReadOnly());

        value->setValue(2500.0);

        checkThreeParameterCascade();
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

TEST_CASE("Frequency labels animate on the shared UI clock and stay edge-safe",
          "[multiband][ui][animation][frequency-label]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1500.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* dividerGroup = findDescendant<FreqDividerGroup>(*multiband);
    REQUIRE(dividerGroup != nullptr);
    auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroup);
    REQUIRE(frequencyText != nullptr);
    auto* label = findDescendant<juce::Label>(*frequencyText);
    REQUIRE(label != nullptr);

    frequencyText->setFreq(1000);
    CHECK(label->getText() == "1 kHz");
    frequencyText->setFreq(1250);
    CHECK(label->getText() == "1.25 kHz");
    frequencyText->setFreq(1500);
    CHECK(label->getText() == "1.5 kHz");

    frequencyText->setFade(true, false);
    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    REQUIRE_FALSE(frequencyText->isVisible());

    frequencyText->setFade(true, true);
    REQUIRE(frequencyText->isVisible());
    const auto initialAlpha = label->getAlpha();
    CHECK(initialAlpha == Catch::Approx(0.0f));
    CHECK(dividerGroup->advanceAnimation(1.0f / 60.0f));
    CHECK(label->getAlpha() > initialAlpha);
    CHECK(label->getAlpha() < 1.0f);

    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    CHECK(label->getAlpha() == Catch::Approx(1.0f).margin(0.002f));

    // Divider groups overlap neighbouring bands but JUCE clips their children.
    // Exercise the supported scale range and keep the compact value bubble
    // wholly inside its parent at every size.
    for (const auto width : { 75, 100, 200 })
    {
        dividerGroup->setBounds(0, 0, width, width * 2);
        CAPTURE(width, frequencyText->getBounds().toString());
        CHECK(frequencyText->getWidth() > 0);
        CHECK(frequencyText->getHeight() > 0);
        CHECK(dividerGroup->getLocalBounds().contains(frequencyText->getBounds()));
    }

    // Hiding the top-level editor must synchronously clear presentation state;
    // otherwise a fully revealed value bubble can flash on the next reopen.
    multiband->dismissTransientUi();
    CHECK(label->getAlpha() == Catch::Approx(0.0f));
    CHECK_FALSE(frequencyText->isVisible());

    // Showing and dismissing an editor without a valid changed value must not
    // create an empty host gesture. JUCE's real TextEditor owns discarded-text
    // behaviour; the component opens a gesture only at a valid commit.
    VerticalLine lifecycleDivider;
    FreqTextLabel lifecycleLabel(lifecycleDivider);
    int gestureBegins = 0;
    int gestureEnds = 0;
    lifecycleDivider.setParameterGestureCallbacks(
        [&gestureBegins] { ++gestureBegins; },
        [] { return std::make_shared<int>(0); },
        [&gestureEnds] { ++gestureEnds; });
    auto* lifecycleChild = findDescendant<juce::Label>(lifecycleLabel);
    REQUIRE(lifecycleChild != nullptr);
    REQUIRE(static_cast<bool>(lifecycleChild->onEditorShow));

    lifecycleChild->onEditorShow();
    REQUIRE(gestureBegins == 0);
    lifecycleLabel.dismissImmediately();

    CHECK(gestureBegins == 0);
    CHECK(gestureEnds == 0);
    CHECK(lifecycleChild->getAlpha() == Catch::Approx(0.0f));
    CHECK_FALSE(lifecycleLabel.isVisible());
}

TEST_CASE("Crossover keyboard focus reveals its exact frequency label",
          "[multiband][divider][ui][keyboard][focus][frequency-label][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1500.0f, 3000.0f, 7000.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* dividerGroup = findDescendant<FreqDividerGroup>(*multiband);
    REQUIRE(dividerGroup != nullptr);
    auto* frequencyText = findDescendant<FreqTextLabel>(*dividerGroup);
    REQUIRE(frequencyText != nullptr);
    auto* label = findDescendant<juce::Label>(*frequencyText);
    REQUIRE(label != nullptr);
    auto& divider = dividerGroup->getVerticalLine();

    frequencyText->setFade(true, false);
    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    REQUIRE_FALSE(frequencyText->isVisible());

    divider.grabKeyboardFocus();
    REQUIRE(divider.hasKeyboardFocus(true));
    VerticalLinePointerTestAccess::gainKeyboardFocus(divider);
    REQUIRE(divider.hasVisibleKeyboardFocus());
    CHECK(dividerGroup->advanceAnimation(1.0f / 60.0f));
    CHECK(frequencyText->isVisible());
    CHECK(label->getAlpha() > 0.0f);

    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    CHECK(label->getAlpha() == Catch::Approx(1.0f).margin(0.002f));

    // An accepted pointer gesture retains actual focus for immediate arrow
    // use, but its pointer-origin focus must not pin the value bubble open.
    const auto position = divider.getLocalBounds().toFloat().getCentre();
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    static_cast<juce::Component&>(divider).mouseDown(
        makeMouseEvent(divider, position, primary));
    CHECK_FALSE(divider.hasVisibleKeyboardFocus());
    static_cast<juce::Component&>(divider).mouseUp(
        makeMouseEvent(divider, position));
    static_cast<juce::Component&>(divider).mouseExit(
        makeMouseEvent(divider, { -1.0f, -1.0f }));
    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    CHECK(divider.hasKeyboardFocus(true));
    CHECK_FALSE(divider.hasVisibleKeyboardFocus());
    CHECK_FALSE(frequencyText->isVisible());

    REQUIRE(divider.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    REQUIRE(divider.hasVisibleKeyboardFocus());
    dividerGroup->advanceAnimation(1.0f / 60.0f);
    CHECK(frequencyText->isVisible());
    CHECK(label->getAlpha() > 0.0f);

    VerticalLinePointerTestAccess::loseKeyboardFocus(divider);
    CHECK_FALSE(divider.hasVisibleKeyboardFocus());
    for (int frame = 0; frame < 120; ++frame)
        dividerGroup->advanceAnimation(1.0f / 60.0f);
    CHECK_FALSE(frequencyText->isVisible());
}

TEST_CASE("Band move/reset parameter contract covers every per-band processor parameter",
          "[multiband][parameters]")
{
    // Keep this explicit: a newly-added processor parameter must make this test
    // fail until the add/delete copy contract deliberately accounts for it.
    std::set<juce::String> expectedBaseIDs {
        OTT_DEPTH_ID,
        OTT_TIME_ID,
        OTT_UPWARD_ID,
        OTT_DOWNWARD_ID,
        OTT_OUTPUT_ID,
        OTT_MIX_ID,
        OTT_ENABLED_ID,
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

    // Keep the accepted insert/module schema independent of the production
    // helper and counts, so adding a slot, field or node requires review here.
    for (int slot = 1; slot <= 8; ++slot)
        for (const auto* field : { "Control1", "Control2", "Control3",
                                  "Control4", "Control5", "Control6",
                                  "Type", "Enabled", "Order" })
            expectedBaseIDs.insert("bandFx" + juce::String(slot) + field);
    for (int node = 0; node < 13; ++node)
        expectedBaseIDs.insert("bandModuleOrder" + juce::String(node) + "Band");
    for (int slot = 1; slot <= 8; ++slot)
        for (const auto* field : { "CloudsEngine", "CloudsFreeze", "CloudsSpread",
                                  "CloudsFeedback", "CloudsReverb" })
            expectedBaseIDs.insert("bandFx" + juce::String(slot) + field);
    for (int slot = 1; slot <= 8; ++slot)
        expectedBaseIDs.insert("bandFx" + juce::String(slot) + "ModulationType");

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
        editor->setVisible(true);
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

TEST_CASE("Band removal survives synchronous editor teardown at every publication phase",
          "[multiband][ui][delete][lifetime][topology][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    struct TeardownScenario
    {
        const char* description = nullptr;
        juce::String parameterID;
        bool nonParameterState = false;
    };

    const std::array<TeardownScenario, 6> scenarios {{
        { "disabled divider", ParameterIDAndName::getIDString(LINE_STATE_ID, 1), false },
        { "compacted crossover", ParameterIDAndName::getIDString(FREQ_ID, 0), false },
        { "copied band parameter", ParameterIDAndName::getIDString(DRIVE_ID, 0), false },
        { "reset inactive parameter", ParameterIDAndName::getIDString(DRIVE_ID, 2), false },
        { "final band count", NUM_BANDS_ID, false },
        { "post-commit non-parameter state", {}, true },
    }};

    for (const auto& scenario : scenarios)
    DYNAMIC_SECTION(scenario.description)
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 3, { 1000.0f, 3000.0f, 7000.0f });

        const auto firstDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        const auto secondDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);
        const auto thirdDrive = ParameterIDAndName::getIDString(DRIVE_ID, 2);
        setPlainParameter(processor, firstDrive, 11.0f);
        setPlainParameter(processor, secondDrive, 22.0f);
        setPlainParameter(processor, thirdDrive, 33.0f);
        processor.assignLfoToTarget(0, firstDrive);
        processor.assignLfoToTarget(1, secondDrive);
        processor.assignLfoToTarget(2, thirdDrive);

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        editor->setVisible(true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);
        auto closeButtons = getPositionedCloseButtons(*multiband);
        REQUIRE(closeButtons.size() == 3);
        auto* firstBandClose = closeButtons.front();
        REQUIRE(firstBandClose != nullptr);
        const auto closeCentre = firstBandClose->getBounds().toFloat().getCentre();
        multiband->mouseMove(makeMouseEvent(*multiband, closeCentre));
        REQUIRE(firstBandClose->isVisible());

        int parameterIndex = -1;
        if (! scenario.nonParameterState)
        {
            auto* observedParameter = processor.treeState.getParameter(
                scenario.parameterID);
            REQUIRE(observedParameter != nullptr);
            parameterIndex = observedParameter->getParameterIndex();
        }

        const auto generationBefore =
            processor.getMultibandTopologyGenerationForTesting();
        REQUIRE((generationBefore & 1u) == 0u);
        EditorResetOnProcessorCallback teardownListener(
            processor, editor, parameterIndex, scenario.nonParameterState);

        firstBandClose->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        CHECK(teardownListener.didResetEditor);
        CHECK(editor == nullptr);
        const auto generationAfter =
            processor.getMultibandTopologyGenerationForTesting();
        CHECK((generationAfter & 1u) == 0u);
        CHECK(generationAfter == generationBefore + 2u);
        CHECK(processor.tryAcquireMultibandTopologyWriterLockForTesting());

        const auto* bandCount = processor.treeState.getRawParameterValue(
            NUM_BANDS_ID);
        const auto* firstFrequency = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(FREQ_ID, 0));
        const auto* firstLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 0));
        const auto* secondLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 1));
        const auto* thirdLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 2));
        REQUIRE(bandCount != nullptr);
        REQUIRE(firstFrequency != nullptr);
        REQUIRE(firstLineState != nullptr);
        REQUIRE(secondLineState != nullptr);
        REQUIRE(thirdLineState != nullptr);
        CHECK(bandCount->load() == Catch::Approx(2.0f));
        CHECK(firstFrequency->load() == Catch::Approx(3000.0f));
        CHECK(firstLineState->load() == Catch::Approx(1.0f));
        CHECK(secondLineState->load() == Catch::Approx(0.0f));
        CHECK(thirdLineState->load() == Catch::Approx(0.0f));

        const auto* movedFirstDrive = processor.treeState.getRawParameterValue(
            firstDrive);
        const auto* movedSecondDrive = processor.treeState.getRawParameterValue(
            secondDrive);
        const auto* resetThirdDrive = processor.treeState.getRawParameterValue(
            thirdDrive);
        REQUIRE(movedFirstDrive != nullptr);
        REQUIRE(movedSecondDrive != nullptr);
        REQUIRE(resetThirdDrive != nullptr);
        CHECK(movedFirstDrive->load() == Catch::Approx(22.0f));
        CHECK(movedSecondDrive->load() == Catch::Approx(33.0f));
        CHECK(resetThirdDrive->load() == Catch::Approx(0.0f));

        const auto routings = processor.getLfoManager()
                                  .getModulationRoutingsCopy();
        const auto* movedFirstRouting = findRouting(routings, firstDrive);
        const auto* movedSecondRouting = findRouting(routings, secondDrive);
        REQUIRE(movedFirstRouting != nullptr);
        REQUIRE(movedSecondRouting != nullptr);
        CHECK(movedFirstRouting->sourceLfoIndex == 1);
        CHECK(movedSecondRouting->sourceLfoIndex == 2);
        CHECK(findRouting(routings, thirdDrive) == nullptr);
    }
}

TEST_CASE("Band addition survives synchronous editor closure at every publication phase",
          "[multiband][ui][add][lifetime][topology][transaction]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    struct ClosureScenario
    {
        const char* description = nullptr;
        juce::String parameterID;
        bool nonParameterState = false;
    };

    const std::array<ClosureScenario, 7> scenarios {{
        { "inserted crossover", ParameterIDAndName::getIDString(FREQ_ID, 0), false },
        { "shifted crossover", ParameterIDAndName::getIDString(FREQ_ID, 1), false },
        { "enabled divider", ParameterIDAndName::getIDString(LINE_STATE_ID, 1), false },
        { "copied band parameter", ParameterIDAndName::getIDString(DRIVE_ID, 2), false },
        { "reset new band parameter", ParameterIDAndName::getIDString(DRIVE_ID, 0), false },
        { "final band count", NUM_BANDS_ID, false },
        { "post-commit non-parameter state", {}, true },
    }};

    for (const auto& scenario : scenarios)
    DYNAMIC_SECTION(scenario.description)
    {
        FireAudioProcessor processor;
        processor.hasUpdateCheckBeenPerformed = true;
        initialiseBandLayout(processor, 2, { 1000.0f, 3000.0f, 7000.0f });

        const auto firstDrive = ParameterIDAndName::getIDString(DRIVE_ID, 0);
        const auto secondDrive = ParameterIDAndName::getIDString(DRIVE_ID, 1);
        const auto thirdDrive = ParameterIDAndName::getIDString(DRIVE_ID, 2);
        setPlainParameter(processor, firstDrive, 11.0f);
        setPlainParameter(processor, secondDrive, 22.0f);
        setPlainParameter(processor, thirdDrive, 73.0f);
        processor.assignLfoToTarget(0, firstDrive);
        processor.assignLfoToTarget(1, secondDrive);
        processor.assignLfoToTarget(2, thirdDrive);

        auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
        editor->setBounds(0, 0, 1000, 500);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

        auto* multiband = findDescendant<Multiband>(*editor);
        REQUIRE(multiband != nullptr);

        int parameterIndex = -1;
        if (! scenario.nonParameterState)
        {
            auto* observedParameter = processor.treeState.getParameter(
                scenario.parameterID);
            REQUIRE(observedParameter != nullptr);
            parameterIndex = observedParameter->getParameterIndex();
        }

        const auto generationBefore =
            processor.getMultibandTopologyGenerationForTesting();
        REQUIRE((generationBefore & 1u) == 0u);
        EditorResetOnProcessorCallback closureListener(
            processor, editor, parameterIndex, scenario.nonParameterState);

        constexpr float insertionX = 0.20f;
        const auto insertionPoint = juce::Point<float> {
            static_cast<float>(multiband->getWidth()) * insertionX,
            static_cast<float>(multiband->getHeight()) * 0.10f
        };
        static_cast<juce::Component&>(*multiband).mouseDown(
            makeMouseEvent(*multiband,
                           insertionPoint,
                           juce::ModifierKeys::leftButtonModifier));

        CHECK(closureListener.didResetEditor);
        CHECK(editor == nullptr);
        const auto generationAfter =
            processor.getMultibandTopologyGenerationForTesting();
        CHECK((generationAfter & 1u) == 0u);
        CHECK(generationAfter == generationBefore + 2u);
        CHECK(processor.tryAcquireMultibandTopologyWriterLockForTesting());

        const auto* bandCount = processor.treeState.getRawParameterValue(
            NUM_BANDS_ID);
        const auto* firstFrequency = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(FREQ_ID, 0));
        const auto* secondFrequency = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(FREQ_ID, 1));
        const auto* firstLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 0));
        const auto* secondLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 1));
        const auto* thirdLineState = processor.treeState.getRawParameterValue(
            ParameterIDAndName::getIDString(LINE_STATE_ID, 2));
        REQUIRE(bandCount != nullptr);
        REQUIRE(firstFrequency != nullptr);
        REQUIRE(secondFrequency != nullptr);
        REQUIRE(firstLineState != nullptr);
        REQUIRE(secondLineState != nullptr);
        REQUIRE(thirdLineState != nullptr);
        CHECK(bandCount->load() == Catch::Approx(3.0f));
        CHECK(firstFrequency->load()
              == Catch::Approx(static_cast<float>(
                  static_cast<int>(transformFromLog(insertionX)))).margin(1.0f));
        CHECK(secondFrequency->load() == Catch::Approx(1000.0f));
        CHECK(firstLineState->load() == Catch::Approx(1.0f));
        CHECK(secondLineState->load() == Catch::Approx(1.0f));
        CHECK(thirdLineState->load() == Catch::Approx(0.0f));

        const auto* resetFirstDrive = processor.treeState.getRawParameterValue(
            firstDrive);
        const auto* movedFirstDrive = processor.treeState.getRawParameterValue(
            secondDrive);
        const auto* movedSecondDrive = processor.treeState.getRawParameterValue(
            thirdDrive);
        REQUIRE(resetFirstDrive != nullptr);
        REQUIRE(movedFirstDrive != nullptr);
        REQUIRE(movedSecondDrive != nullptr);
        CHECK(resetFirstDrive->load() == Catch::Approx(0.0f));
        CHECK(movedFirstDrive->load() == Catch::Approx(11.0f));
        CHECK(movedSecondDrive->load() == Catch::Approx(22.0f));

        const auto routings = processor.getLfoManager()
                                  .getModulationRoutingsCopy();
        const auto* movedFirstRouting = findRouting(routings, secondDrive);
        const auto* movedSecondRouting = findRouting(routings, thirdDrive);
        REQUIRE(movedFirstRouting != nullptr);
        REQUIRE(movedSecondRouting != nullptr);
        CHECK(movedFirstRouting->sourceLfoIndex == 0);
        CHECK(movedSecondRouting->sourceLfoIndex == 1);
        CHECK(findRouting(routings, firstDrive) == nullptr);
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
    editor->setVisible(true);
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
    auto* entryEditor = findDescendant<juce::TextEditor>(*valueEntryPopup);
    REQUIRE(entryEditor != nullptr);
    entryEditor->setText(juce::String(enteredValue), juce::dontSendNotification);
    REQUIRE(static_cast<juce::Component&>(*entryEditor).keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
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
        auto* entryEditor = findDescendant<juce::TextEditor>(*valueEntryPopup);
        REQUIRE(entryEditor != nullptr);
        entryEditor->setText(juce::String(enteredValue), juce::dontSendNotification);
        REQUIRE(static_cast<juce::Component&>(*entryEditor).keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));

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

TEST_CASE("Slider context assignment retains its target while a shared panel knob rebinds",
          "[multiband][ui][modulation][assign][popup-menu][target]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    auto* bandPanel = findDescendant<BandPanel>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(bandPanel != nullptr);

    multiband->setFocusIndex(0);
    auto* reusedDriveKnob = bandPanel->getDriveKnob();
    REQUIRE(reusedDriveKnob != nullptr);
    REQUIRE(static_cast<bool>(reusedDriveKnob->onLfoAssignmentRequested));
    const auto targetWhenMenuOpened = reusedDriveKnob->getParamID();
    REQUIRE(targetWhenMenuOpened
            == ParameterIDAndName::getIDString(DRIVE_ID, 0));
    auto deliverAssignment = ModulatableSliderTestAccess::createAssignmentResultHandler(
        *reusedDriveKnob);

    // Popup completion is asynchronous; the shared BandPanel knob may point
    // at another band before the user chooses an LFO.
    multiband->setFocusIndex(1);
    REQUIRE(bandPanel->getDriveKnob() == reusedDriveKnob);
    const auto reboundTarget = reusedDriveKnob->getParamID();
    REQUIRE(reboundTarget == ParameterIDAndName::getIDString(DRIVE_ID, 1));

    deliverAssignment(4);

    const auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* assigned = findRouting(routings, targetWhenMenuOpened);
    REQUIRE(assigned != nullptr);
    CHECK(assigned->sourceLfoIndex == 3);
    CHECK(findRouting(routings, reboundTarget) == nullptr);
}

TEST_CASE("BandPanel defers shared knob rebinds until its host gesture ends",
          "[multiband][ui][focus][gesture][attachment]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    initialiseBandLayout(processor, 2);

    BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 300);
    auto* driveKnob = panel.getDriveKnob();
    REQUIRE(driveKnob != nullptr);

    const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto secondDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    auto* firstDrive = processor.treeState.getParameter(firstDriveID);
    auto* secondDrive = processor.treeState.getParameter(secondDriveID);
    REQUIRE(firstDrive != nullptr);
    REQUIRE(secondDrive != nullptr);

    ParameterGestureCapture firstHost(processor, firstDrive->getParameterIndex());
    ParameterGestureCapture secondHost(processor, secondDrive->getParameterIndex());
    const auto downPosition = driveKnob->getLocalBounds().toFloat().getCentre();
    const auto dragPosition = downPosition + juce::Point<float> { 0.0f, -24.0f };

    driveKnob->mouseDown(makeMouseEvent(
        *driveKnob,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(driveKnob->hasActiveInteraction());
    REQUIRE(firstHost.beginCount == 1);

    panel.setFocusBandNum(1);
    CHECK(panel.getFocusBandNum() == 0);
    CHECK(driveKnob->getParamID() == firstDriveID);

    driveKnob->mouseDrag(makeDragMouseEvent(
        *driveKnob,
        dragPosition,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    driveKnob->mouseUp(makeDragMouseEvent(
        *driveKnob,
        dragPosition,
        downPosition,
        juce::ModifierKeys {}));

    CHECK_FALSE(driveKnob->hasActiveInteraction());
    CHECK(panel.getFocusBandNum() == 1);
    CHECK(driveKnob->getParamID() == secondDriveID);
    checkBalancedGesture(firstHost);
    CHECK(secondHost.beginCount == 0);
    CHECK(secondHost.endCount == 0);
    CHECK(secondHost.valueChangeCount == 0);
}

TEST_CASE("BandPanel keeps a modulation handle drag on its original band",
          "[multiband][ui][focus][gesture][modulation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    initialiseBandLayout(processor, 2);

    const auto firstDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 0);
    const auto secondDriveID = ParameterIDAndName::getIDString(DRIVE_ID, 1);
    processor.assignLfoToTarget(0, firstDriveID);
    processor.setModulationDepth(firstDriveID, 0.20f);

    BandPanel panel(processor, {}, {}, {}, {}, {});
    panel.setBounds(0, 0, 1000, 300);
    auto* driveKnob = panel.getDriveKnob();
    REQUIRE(driveKnob != nullptr);
    driveKnob->isModulated = true;
    driveKnob->lfoAmount = 0.20;

    const auto downPosition = driveKnob->getModulationHandleBounds().getCentre();
    const auto dragPosition = downPosition + juce::Point<float> { 0.0f, -80.0f };
    driveKnob->mouseDown(makeMouseEvent(
        *driveKnob,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(driveKnob->isModHandleMouseDown);

    panel.setFocusBandNum(1);
    CHECK(panel.getFocusBandNum() == 0);
    CHECK(driveKnob->getParamID() == firstDriveID);

    driveKnob->mouseDrag(makeDragMouseEvent(
        *driveKnob,
        dragPosition,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));

    auto routings = processor.getLfoManager().getModulationRoutingsCopy();
    const auto* firstRouting = findRouting(routings, firstDriveID);
    REQUIRE(firstRouting != nullptr);
    CHECK(firstRouting->depth == Catch::Approx(0.60f));
    CHECK(findRouting(routings, secondDriveID) == nullptr);

    driveKnob->mouseUp(makeDragMouseEvent(
        *driveKnob,
        dragPosition,
        downPosition,
        juce::ModifierKeys {}));

    CHECK_FALSE(driveKnob->hasActiveInteraction());
    CHECK(panel.getFocusBandNum() == 1);
    CHECK(driveKnob->getParamID() == secondDriveID);
    routings = processor.getLfoManager().getModulationRoutingsCopy();
    firstRouting = findRouting(routings, firstDriveID);
    REQUIRE(firstRouting != nullptr);
    CHECK(firstRouting->depth == Catch::Approx(0.60f));
    CHECK(findRouting(routings, secondDriveID) == nullptr);
}

TEST_CASE("Crossover divider hover and press feedback fades on the shared clock",
          "[multiband][ui][divider][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    VerticalLine divider;
    divider.setBounds(0, 0, 24, 160);
    const auto centre = divider.getLocalBounds().toFloat().getCentre();

    CHECK(divider.getHoverAnimation() == 0.0f);
    CHECK(divider.getPressAnimation() == 0.0f);

    static_cast<juce::Component&>(divider).mouseEnter(
        makeMouseEvent(divider, centre));
    REQUIRE(divider.advanceAnimation(1.0f / 60.0f));
    CHECK(divider.getHoverAnimation() > 0.0f);
    CHECK(divider.getHoverAnimation() < 1.0f);
    CHECK(divider.getPressAnimation() == 0.0f);

    static_cast<juce::Component&>(divider).mouseDown(makeMouseEvent(
        divider,
        centre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    REQUIRE(divider.advanceAnimation(1.0f / 60.0f));
    CHECK(divider.getPressAnimation() > 0.0f);

    static_cast<juce::Component&>(divider).mouseUp(
        makeMouseEvent(divider, centre));
    const auto pressedAmount = divider.getPressAnimation();
    for (int frame = 0; frame < 12; ++frame)
        divider.advanceAnimation(1.0f / 60.0f);
    CHECK(divider.getPressAnimation() < pressedAmount);
    CHECK(divider.getHoverAnimation() > 0.0f);

    static_cast<juce::Component&>(divider).mouseExit(
        makeMouseEvent(divider, { -1.0f, -1.0f }));
    const auto hoveredAmount = divider.getHoverAnimation();
    for (int frame = 0; frame < 12; ++frame)
        divider.advanceAnimation(1.0f / 60.0f);
    CHECK(divider.getHoverAnimation() < hoveredAmount);

    for (int frame = 0; frame < 90; ++frame)
        divider.advanceAnimation(1.0f / 60.0f);
    CHECK(divider.getHoverAnimation() == Catch::Approx(0.0f).margin(0.001f));
    CHECK(divider.getPressAnimation() == Catch::Approx(0.0f).margin(0.001f));

    int gestureBegins = 0;
    int gestureEnds = 0;
    divider.setParameterGestureCallbacks([&gestureBegins] { ++gestureBegins; },
                                         [] { return std::make_shared<int>(0); },
                                         [&gestureEnds] { ++gestureEnds; });
    static_cast<juce::Component&>(divider).mouseEnter(
        makeMouseEvent(divider, centre));
    static_cast<juce::Component&>(divider).mouseDown(makeMouseEvent(
        divider,
        centre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    divider.advanceAnimation(1.0f / 60.0f);
    REQUIRE(divider.getHoverAnimation() > 0.0f);
    REQUIRE(divider.getPressAnimation() > 0.0f);

    divider.dismissTransientInteraction();
    CHECK(gestureBegins == 1);
    CHECK(gestureEnds == 1);
    CHECK(divider.getHoverAnimation() == 0.0f);
    CHECK(divider.getPressAnimation() == 0.0f);
}

#if JUCE_MAC
TEST_CASE("macOS Control-click on a modulation handle remains a popup gesture",
          "[multiband][ui][modulation][popup-menu][macos]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.isModulated = true;
    slider.parameterID = "popup-target";
    int polarityToggleCount = 0;
    slider.onBipolarModeToggled = [&](const juce::String&)
    {
        ++polarityToggleCount;
    };

    const auto handleCentre = slider.getModulationHandleBounds().getCentre();
    slider.mouseDown(makeMouseEvent(
        slider,
        handleCentre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::ctrlModifier }));

    CHECK(polarityToggleCount == 0);
    CHECK_FALSE(slider.isModHandleMouseDown);
}
#endif

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
    editor->setVisible(true);
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
        processor.assignLfoToTarget(2, firstDriveID);

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
            const auto routings = processor.getLfoManager()
                                      .getModulationRoutingsCopy();
            const auto* movedRouting = findRouting(routings, secondDriveID);
            REQUIRE(movedRouting != nullptr);
            CHECK(movedRouting->sourceLfoIndex == 2);
            CHECK(findRouting(routings, firstDriveID) == nullptr);
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
            const auto routings = processor.getLfoManager()
                                      .getModulationRoutingsCopy();
            const auto* preservedRouting = findRouting(routings, firstDriveID);
            REQUIRE(preservedRouting != nullptr);
            CHECK(preservedRouting->sourceLfoIndex == 2);
            CHECK(findRouting(routings, secondDriveID) == nullptr);
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

TEST_CASE("Multiband background keyboard commands edit the focused topology",
          "[multiband][ui][keyboard][accessibility][topology][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 1);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    auto* bandCount = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(bandCount != nullptr);
    REQUIRE(bandCount->load(std::memory_order_relaxed)
            == Catch::Approx(1.0f));

    multiband->grabKeyboardFocus();
    REQUIRE(multiband->hasKeyboardFocus(false));
    auto& component = static_cast<juce::Component&>(*multiband);

    // Return splits the selected band at its visual centre.
    CHECK(component.keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(bandCount->load(std::memory_order_relaxed)
          == Catch::Approx(2.0f));
    CHECK(multiband->getFocusIndex() == 0);

    // Backspace removes the selected band through the same transaction used
    // by the close tile and preserves the mandatory one-band floor.
    CHECK(component.keyPressed(
        juce::KeyPress { juce::KeyPress::backspaceKey }));
    CHECK(bandCount->load(std::memory_order_relaxed)
          == Catch::Approx(1.0f));
    CHECK(component.keyPressed(
        juce::KeyPress { juce::KeyPress::deleteKey }));
    CHECK(bandCount->load(std::memory_order_relaxed)
          == Catch::Approx(1.0f));

    const juce::KeyPress plusKey {
        '+', juce::ModifierKeys::shiftModifier, '+'
    };
    CHECK(component.keyPressed(plusKey));
    CHECK(bandCount->load(std::memory_order_relaxed)
          == Catch::Approx(2.0f));

    auto* accessibility = multiband->getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    CHECK(accessibility->getRole() == juce::AccessibilityRole::group);
    CHECK(accessibility->getTitle() == "Multiband spectrum editor");
    CHECK(accessibility->getHelp()
              .containsIgnoreCase("Return or Plus"));

    editor->removeFromDesktop();
}

TEST_CASE("Multiband topology shortcuts do not steal unhandled child keys",
          "[multiband][ui][keyboard][accessibility][topology][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2, { 1000.0f, 0.0f, 0.0f });

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);
    auto& divider = dividerGroups[0]->getVerticalLine();
    divider.grabKeyboardFocus();
    REQUIRE(divider.hasKeyboardFocus(false));

    auto* bandCount = processor.treeState.getRawParameterValue(NUM_BANDS_ID);
    REQUIRE(bandCount != nullptr);
    REQUIRE(bandCount->load(std::memory_order_relaxed)
            == Catch::Approx(2.0f));

    CHECK_FALSE(static_cast<juce::Component&>(*multiband).keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(bandCount->load(std::memory_order_relaxed)
          == Catch::Approx(2.0f));

    editor->removeFromDesktop();
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
    editor->setVisible(true);
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
    auto* firstBandClose = closeButtons.front();
    REQUIRE(firstBandClose != nullptr);
    const auto closeCentre = firstBandClose->getBounds().toFloat().getCentre();
    multiband->mouseMove(makeMouseEvent(*multiband, closeCentre));
    REQUIRE(firstBandClose->isPresented());
    firstBandClose->triggerClick();
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

TEST_CASE("Band hover overlay fades between neighbouring bands",
          "[multiband][ui][hover][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    REQUIRE(multiband->isShowing());

    MultibandPointerTestAccess::setHoveredBand(*multiband, 1);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 1)
          == Catch::Approx(0.0f));

    multiband->animationTick(1.0f / 60.0f);
    const auto enteringAmount =
        MultibandPointerTestAccess::getBandHover(*multiband, 1);
    CHECK(enteringAmount > 0.0f);
    CHECK(enteringAmount < 1.0f);

    for (int frame = 0; frame < 10; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    const auto establishedAmount =
        MultibandPointerTestAccess::getBandHover(*multiband, 1);
    CHECK(establishedAmount > enteringAmount);

    MultibandPointerTestAccess::setHoveredBand(*multiband, 0);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 0)
          == Catch::Approx(0.0f));
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 1)
          == Catch::Approx(establishedAmount));

    multiband->animationTick(1.0f / 60.0f);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 0) > 0.0f);
    for (int frame = 0; frame < 8; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 1)
          < establishedAmount);

    const auto leavingAmount =
        MultibandPointerTestAccess::getBandHover(*multiband, 0);
    MultibandPointerTestAccess::setHoveredBand(*multiband, -1);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 0)
          == Catch::Approx(leavingAmount));
    for (int frame = 0; frame < 12; ++frame)
        multiband->animationTick(1.0f / 60.0f);
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 0)
          < leavingAmount);

    multiband->dismissTransientUi();
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 0)
          == Catch::Approx(0.0f));
    CHECK(MultibandPointerTestAccess::getBandHover(*multiband, 1)
          == Catch::Approx(0.0f));
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
    REQUIRE(divider.getWidth() >= 16);

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

TEST_CASE("Divider compatibility toggles reject user commands but follow automation",
          "[multiband][divider][line-state][input][automation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    initialiseBandLayout(processor, 2);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1000, 500);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    auto* multiband = findDescendant<Multiband>(*editor);
    REQUIRE(multiband != nullptr);
    const auto dividerGroups = getDividerGroupsByIndex(*multiband);
    REQUIRE(dividerGroups[0] != nullptr);

    auto& dividerGroup = *dividerGroups[0];
    const auto lineStateId =
        ParameterIDAndName::getIDString(LINE_STATE_ID, 0);
    const auto* lineState =
        processor.treeState.getRawParameterValue(lineStateId);
    REQUIRE(lineState != nullptr);
    REQUIRE(lineState->load(std::memory_order_relaxed) >= 0.5f);
    REQUIRE(dividerGroup.getToggleState());
    REQUIRE(dividerGroup.isVisible());

    CHECK_FALSE(dividerGroup.getWantsKeyboardFocus());
    CHECK_FALSE(dividerGroup.getMouseClickGrabsKeyboardFocus());
    CHECK(dividerGroup.isAccessible());
    auto* groupAccessibility = dividerGroup.getAccessibilityHandler();
    REQUIRE(groupAccessibility != nullptr);
    CHECK(groupAccessibility->getRole() == juce::AccessibilityRole::ignored);
    CHECK(dividerGroup.getVerticalLine().isAccessible());
    CHECK(dividerGroup.getVerticalLine().getAccessibilityHandler() != nullptr);

    const auto checkStillActive = [&]
    {
        CHECK(lineState->load(std::memory_order_relaxed) >= 0.5f);
        CHECK(dividerGroup.getToggleState());
        CHECK(dividerGroup.isVisible());
    };

    dividerGroup.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    checkStillActive();

    auto& component = static_cast<juce::Component&>(dividerGroup);
    CHECK(component.keyPressed(
        juce::KeyPress { juce::KeyPress::returnKey }));
    CHECK(component.keyPressed(
        juce::KeyPress { juce::KeyPress::spaceKey }));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    checkStillActive();

    // ButtonAttachment updates the ToggleButton with
    // setToggleState(sendNotificationSync). That internal compatibility path
    // must continue to invoke clicked() and update divider presentation.
    setPlainParameter(processor, lineStateId, 0.0f);
    CHECK(lineState->load(std::memory_order_relaxed) < 0.5f);
    CHECK_FALSE(dividerGroup.getToggleState());
    CHECK_FALSE(dividerGroup.isVisible());

    setPlainParameter(processor, lineStateId, 1.0f);
    CHECK(lineState->load(std::memory_order_relaxed) >= 0.5f);
    CHECK(dividerGroup.getToggleState());
    CHECK(dividerGroup.isVisible());
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
