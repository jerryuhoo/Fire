#include <Panels/ControlPanel/GlobalPanel.h>
#include <Panels/SpectrogramPanel/FilterControl.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

struct DraggableButtonPointerTestAccess
{
    static bool hasPrimaryDrag(const DraggableButton& button)
    {
        return button.primaryDragActive;
    }

    static void setTrackedPointerSource(
        DraggableButton& button,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        button.pointerSourceType = sourceType;
        button.pointerSourceIndex = sourceIndex;
    }

    static bool advanceAnimation(DraggableButton& button,
                                 float deltaSeconds) noexcept
    {
        return button.advanceAnimation(deltaSeconds);
    }

    static void pollAnimationLifecycle(DraggableButton& button)
    {
        button.timerCallback();
    }

    static float hoverAnimation(const DraggableButton& button) noexcept
    {
        return button.hoverAnimation.current;
    }

    static float pressAnimation(const DraggableButton& button) noexcept
    {
        return button.pressAnimation.current;
    }

    static float focusAnimation(const DraggableButton& button) noexcept
    {
        return button.focusAnimation.current;
    }

    static bool isKeyboardFocusVisible(
        const DraggableButton& button) noexcept
    {
        return button.keyboardFocusVisible;
    }

    static void notifyFocusGained(
        DraggableButton& button,
        juce::Component::FocusChangeType cause)
    {
        button.focusGained(cause);
    }

    static void notifyFocusLost(DraggableButton& button)
    {
        button.focusLost(juce::Component::focusChangedDirectly);
    }
};

struct FilterControlTestAccess
{
    static DraggableButton& lowButton(FilterControl& control)
    {
        return control.draggableLowButton;
    }

    static DraggableButton& peakButton(FilterControl& control)
    {
        return control.draggablePeakButton;
    }

    static DraggableButton& highButton(FilterControl& control)
    {
        return control.draggableHighButton;
    }

    static bool hasTransientState(const FilterControl& control)
    {
        return control.dragGestureSession != nullptr
            || control.dragTooltipVisible;
    }

    static void updateButtonStates(FilterControl& control)
    {
        control.updateDraggableButtonStates();
    }

    static bool hasResponseCurve(const FilterControl& control)
    {
        return ! control.responseCurve.isEmpty();
    }

    static bool hasLfoResponseCurve(const FilterControl& control)
    {
        return ! control.lfoResponseCurve.isEmpty();
    }

    static bool telemetryPresentationIsActive(const FilterControl& control)
    {
        return control.telemetryPresentationActive;
    }

    static double responseSampleRate(const FilterControl& control)
    {
        return control.responseSampleRate;
    }

    static float responseCurveRight(const FilterControl& control)
    {
        return control.responseCurve.getBounds().getRight();
    }
};

namespace
{
struct GestureEvents
{
    int beginCount = 0;
    int endCount = 0;
    int valueCount = 0;
    int depth = 0;
    int minimumDepth = 0;
    int maximumDepth = 0;
    bool valueOutsideGesture = false;
    std::vector<char> order;
};

class GestureCapture final : public juce::AudioProcessorListener
{
public:
    GestureCapture(FireAudioProcessor& processorToObserve,
                   std::initializer_list<juce::String> parameterIDs)
        : processor(processorToObserve)
    {
        for (const auto& parameterID : parameterIDs)
        {
            const auto& parameters = processor.getParameters();
            for (int index = 0; index < parameters.size(); ++index)
                if (const auto* parameter = dynamic_cast<const juce::AudioProcessorParameterWithID*>(
                        parameters[index]);
                    parameter != nullptr && parameter->getParameterID() == parameterID)
                {
                    observedIndices.push_back(index);
                    events.emplace_back();
                    break;
                }
        }

        REQUIRE(observedIndices.size() == parameterIDs.size());
        processor.addListener(this);
    }

    ~GestureCapture() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int parameterIndex,
                                        float) override
    {
        if (auto* state = find(parameterIndex))
        {
            ++state->valueCount;
            state->valueOutsideGesture = state->valueOutsideGesture || state->depth <= 0;
            state->order.push_back('V');
        }
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,
                                                   int parameterIndex) override
    {
        if (auto* state = find(parameterIndex))
        {
            ++state->beginCount;
            ++state->depth;
            state->maximumDepth = juce::jmax(state->maximumDepth, state->depth);
            state->order.push_back('B');
        }
    }

    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,
                                                 int parameterIndex) override
    {
        if (auto* state = find(parameterIndex))
        {
            ++state->endCount;
            --state->depth;
            state->minimumDepth = juce::jmin(state->minimumDepth, state->depth);
            state->order.push_back('E');
        }
    }

    const GestureEvents& forParameter(size_t index) const
    {
        REQUIRE(index < events.size());
        return events[index];
    }

private:
    GestureEvents* find(int parameterIndex)
    {
        for (size_t index = 0; index < observedIndices.size(); ++index)
            if (observedIndices[index] == parameterIndex)
                return &events[index];
        return nullptr;
    }

    FireAudioProcessor& processor;
    std::vector<int> observedIndices;
    std::vector<GestureEvents> events;
};

enum class ParameterCallbackStage
{
    begin,
    value,
    end
};

class ControlReleaseOnParameterCallback final : public juce::AudioProcessorListener
{
public:
    ControlReleaseOnParameterCallback(FireAudioProcessor& processorToObserve,
                                      const juce::String& parameterID,
                                      ParameterCallbackStage stageToObserve,
                                      std::unique_ptr<FilterControl>& controlToRelease)
        : processor(processorToObserve), control(controlToRelease), stage(stageToObserve)
    {
        const auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(parameter != nullptr);
        targetParameterIndex = parameter->getParameterIndex();
        processor.addListener(this);
    }

    ~ControlReleaseOnParameterCallback() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*,
                                        int parameterIndex,
                                        float) override
    {
        releaseIfMatching(ParameterCallbackStage::value, parameterIndex);
    }

    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override
    {
    }

    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,
                                                   int parameterIndex) override
    {
        releaseIfMatching(ParameterCallbackStage::begin, parameterIndex);
    }

    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,
                                                 int parameterIndex) override
    {
        releaseIfMatching(ParameterCallbackStage::end, parameterIndex);
    }

    bool didRelease() const noexcept { return released; }

private:
    void releaseIfMatching(ParameterCallbackStage callbackStage, int parameterIndex)
    {
        if (callbackStage == stage && parameterIndex == targetParameterIndex
            && control != nullptr)
        {
            released = true;
            control.reset();
        }
    }

    FireAudioProcessor& processor;
    std::unique_ptr<FilterControl>& control;
    ParameterCallbackStage stage;
    int targetParameterIndex = -1;
    bool released = false;
};

class EditorReleaseOnGestureEnd final : public juce::AudioProcessorListener
{
public:
    EditorReleaseOnGestureEnd(FireAudioProcessor& processorToObserve,
                              const juce::String& parameterID,
                              std::unique_ptr<FireAudioProcessorEditor>& editorToRelease)
        : processor(processorToObserve), editor(editorToRelease)
    {
        auto* parameter = processor.treeState.getParameter(parameterID);
        REQUIRE(parameter != nullptr);
        targetParameterIndex = parameter->getParameterIndex();
        processor.addListener(this);
    }

    ~EditorReleaseOnGestureEnd() override { processor.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override {}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override
    {
        if (index == targetParameterIndex && editor != nullptr)
        {
            released = true;
            editor.reset();
        }
    }

    bool didRelease() const noexcept { return released; }

private:
    FireAudioProcessor& processor;
    std::unique_ptr<FireAudioProcessorEditor>& editor;
    int targetParameterIndex = -1;
    bool released = false;
};

void checkBalanced(const GestureEvents& events)
{
    CHECK(events.beginCount == 1);
    CHECK(events.endCount == 1);
    CHECK(events.valueCount >= 1);
    CHECK(events.depth == 0);
    CHECK(events.minimumDepth == 0);
    CHECK(events.maximumDepth == 1);
    CHECK_FALSE(events.valueOutsideGesture);
    REQUIRE_FALSE(events.order.empty());
    CHECK(events.order.front() == 'B');
    CHECK(events.order.back() == 'E');
}

void checkInactive(const GestureEvents& events)
{
    CHECK(events.beginCount == 0);
    CHECK(events.endCount == 0);
    CHECK(events.valueCount == 0);
    CHECK(events.depth == 0);
    CHECK(events.minimumDepth == 0);
    CHECK(events.maximumDepth == 0);
    CHECK_FALSE(events.valueOutsideGesture);
    CHECK(events.order.empty());
}

void checkClosedExactlyOnce(const GestureEvents& events)
{
    if (events.beginCount == 0)
        checkInactive(events);
    else
        checkBalanced(events);
}

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

constexpr double filterTelemetrySampleRate = 48000.0;
constexpr int filterTelemetryBlockSize = 64;

void prepareFilterTelemetry(FireAudioProcessor& processor)
{
    processor.setRateAndBufferSizeDetails(filterTelemetrySampleRate,
                                          filterTelemetryBlockSize);
    processor.prepareToPlay(filterTelemetrySampleRate,
                            filterTelemetryBlockSize);
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    processor.assignLfoToTarget(0, PEAK_GAIN_ID);
    processor.setModulationDepth(PEAK_GAIN_ID, 0.75f);
}

void publishFilterTelemetry(FireAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, filterTelemetryBlockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers,
                                juce::Point<float> mouseDownPosition)
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
             false };
}

template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;
    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;
    return nullptr;
}
} // namespace

TEST_CASE("Filter response follows prepare and runtime sample-rate changes",
          "[filter-control][ui][sample-rate][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    control.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control.setVisible(true);
    REQUIRE(control.isShowing());

    CHECK(FilterControlTestAccess::responseSampleRate(control) == 0.0);
    CHECK_FALSE(FilterControlTestAccess::hasResponseCurve(control));

    processor.setRateAndBufferSizeDetails(44100.0, 64);
    processor.prepareToPlay(44100.0, 64);
    control.animationTick();
    CHECK(FilterControlTestAccess::responseSampleRate(control) == 44100.0);
    CHECK(FilterControlTestAccess::hasResponseCurve(control));

    processor.setRateAndBufferSizeDetails(96000.0, 64);
    processor.prepareToPlay(96000.0, 64);
    control.animationTick();
    CHECK(FilterControlTestAccess::responseSampleRate(control) == 96000.0);
    CHECK(FilterControlTestAccess::hasResponseCurve(control));
}

TEST_CASE("Filter telemetry epochs reject packets from older presentation sessions",
          "[filter-control][ui][telemetry][freshness][source-epoch][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    prepareFilterTelemetry(processor);

    publishFilterTelemetry(processor);
    const auto freshEpoch =
        processor.requestFreshModulatedFilterValuesEpoch();

    ModulatedFilterValues values;
    CHECK_FALSE(processor.getLatestModulatedFilterValues(values, freshEpoch));
    // Rejected packets are consumed, otherwise a full stale FIFO would also
    // prevent the audio thread from publishing into the new session.
    CHECK_FALSE(processor.getLatestModulatedFilterValues(values));

    publishFilterTelemetry(processor);
    REQUIRE(processor.getLatestModulatedFilterValues(values, freshEpoch));
    CHECK(values.captureEpoch == freshEpoch);
}

TEST_CASE("Hidden filter pages drain telemetry and wait for a fresh visible packet",
          "[filter-control][ui][telemetry][workspace][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    prepareFilterTelemetry(processor);
    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    control.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control.setVisible(true);
    REQUIRE(control.isShowing());

    control.animationTick();
    REQUIRE(FilterControlTestAccess::telemetryPresentationIsActive(control));
    publishFilterTelemetry(processor);
    control.animationTick();
    REQUIRE(FilterControlTestAccess::hasLfoResponseCurve(control));

    // This is the same effective visibility transition as MASTER LAB to BAND
    // LAB or MOD FORGE. The old curve must disappear immediately.
    control.setVisible(false);
    CHECK_FALSE(FilterControlTestAccess::telemetryPresentationIsActive(control));
    CHECK_FALSE(FilterControlTestAccess::hasLfoResponseCurve(control));

    publishFilterTelemetry(processor);
    publishFilterTelemetry(processor);
    control.animationTick();
    ModulatedFilterValues drainedValues;
    CHECK_FALSE(processor.getLatestModulatedFilterValues(drainedValues));

    // Playback can stop while the page is hidden. Showing the page must not
    // resurrect its last hidden packet; only a callback from the new epoch is
    // eligible for presentation.
    control.setVisible(true);
    control.animationTick();
    CHECK_FALSE(FilterControlTestAccess::hasLfoResponseCurve(control));
    publishFilterTelemetry(processor);
    control.animationTick();
    CHECK(FilterControlTestAccess::hasLfoResponseCurve(control));
}

TEST_CASE("Detached editors drain filter telemetry before returning from their timer",
          "[filter-control][ui][editor][hidden][telemetry][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    prepareFilterTelemetry(processor);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    REQUIRE(editor->isShowing());

    auto* control = findDescendant<FilterControl>(*editor);
    REQUIRE(control != nullptr);
    control->setBounds(0, 0, 1000, 400);
    control->setVisible(true);
    control->animationTick();
    publishFilterTelemetry(processor);
    control->animationTick();
    REQUIRE(FilterControlTestAccess::hasLfoResponseCurve(*control));

    editor->removeFromDesktop();
    REQUIRE_FALSE(editor->isShowing());
    publishFilterTelemetry(processor);
    editor->timerCallback();

    CHECK_FALSE(FilterControlTestAccess::telemetryPresentationIsActive(*control));
    CHECK_FALSE(FilterControlTestAccess::hasLfoResponseCurve(*control));
    ModulatedFilterValues drainedValues;
    CHECK_FALSE(processor.getLatestModulatedFilterValues(drainedValues));
}

TEST_CASE("New filter controls do not inherit telemetry from destroyed editors",
          "[filter-control][ui][editor][lifecycle][telemetry][freshness][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    prepareFilterTelemetry(processor);
    publishFilterTelemetry(processor);

    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    auto* control = findDescendant<FilterControl>(*editor);
    REQUIRE(control != nullptr);
    control->setBounds(0, 0, 1000, 400);
    control->setVisible(true);
    control->animationTick();

    CHECK_FALSE(FilterControlTestAccess::hasLfoResponseCurve(*control));
    ModulatedFilterValues drainedValues;
    CHECK_FALSE(processor.getLatestModulatedFilterValues(drainedValues));

    publishFilterTelemetry(processor);
    control->animationTick();
    CHECK(FilterControlTestAccess::hasLfoResponseCurve(*control));
}

TEST_CASE("Low sample-rate filter response stays on the fixed spectrum axis",
          "[filter-control][ui][sample-rate][nyquist][layout][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, HIGHCUT_FREQ_ID, 20000.0f);
    processor.setRateAndBufferSizeDetails(32000.0, 64);
    processor.prepareToPlay(32000.0, 64);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);

    REQUIRE(FilterControlTestAccess::hasResponseCurve(control));
    const auto safeNyquist = std::nextafter(16000.0f, 0.0f);
    const auto expectedRight = 1000.0f * static_cast<float>(
        juce::mapFromLog10(static_cast<double>(safeNyquist),
                           20.0,
                           20000.0));
    const auto curveRight = FilterControlTestAccess::responseCurveRight(control);
    const auto nodeCentre = FilterControlTestAccess::highButton(control)
                                .getBounds()
                                .toFloat()
                                .getCentreX();

    CHECK(curveRight == Catch::Approx(expectedRight).margin(0.5f));
    CHECK(nodeCentre == Catch::Approx(expectedRight).margin(1.0f));
    CHECK(curveRight < 980.0f);

    auto& highButton = FilterControlTestAccess::highButton(control);
    const auto downPosition = highButton.getLocalBounds().toFloat().getCentre();
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    highButton.mouseDown(makeMouseEvent(highButton,
                                        downPosition,
                                        leftButton,
                                        downPosition));
    const auto beyondNyquist = juce::Point<float> { 1200.0f, 200.0f }
                             - highButton.getPosition().toFloat();
    highButton.mouseDrag(makeMouseEvent(highButton,
                                        beyondNyquist,
                                        leftButton,
                                        downPosition));
    const auto* highFrequency =
        processor.treeState.getRawParameterValue(HIGHCUT_FREQ_ID);
    REQUIRE(highFrequency != nullptr);
    CHECK(highFrequency->load() <= 16000.0f);
    CHECK(highFrequency->load() >= 15999.0f);
    highButton.mouseUp(makeMouseEvent(highButton,
                                      beyondNyquist,
                                      {},
                                      downPosition));
}

TEST_CASE("Filter graph drags and Q wheel changes bracket host gestures",
          "[filter-control][ui][automation][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    control.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control.setVisible(true);
    auto& lowButton = FilterControlTestAccess::lowButton(control);
    REQUIRE_FALSE(lowButton.getBounds().isEmpty());

    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto downPosition = lowButton.getLocalBounds().toFloat().getCentre();

    SECTION("node drag")
    {
        GestureCapture capture(processor,
                               { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
        lowButton.mouseDown(makeMouseEvent(lowButton,
                                           downPosition,
                                           leftButton,
                                           downPosition));

        const auto targetInControl = juce::Point<float> { 650.0f, 95.0f };
        const auto targetInButton = targetInControl
                                  - lowButton.getPosition().toFloat();
        lowButton.mouseDrag(makeMouseEvent(lowButton,
                                           targetInButton,
                                           leftButton,
                                           downPosition));
        lowButton.mouseUp(makeMouseEvent(lowButton,
                                         targetInButton,
                                         {},
                                         downPosition));

        checkBalanced(capture.forParameter(0));
        checkBalanced(capture.forParameter(1));
        checkBalanced(capture.forParameter(2));
    }

    SECTION("Q wheel")
    {
        GestureCapture capture(processor, { LOWCUT_Q_ID });
        juce::MouseWheelDetails wheel;
        wheel.deltaY = 0.4f;
        lowButton.mouseWheelMove(makeMouseEvent(lowButton,
                                                downPosition,
                                                {},
                                                downPosition),
                                 wheel);

        checkBalanced(capture.forParameter(0));
    }
}

TEST_CASE("Filter graph nodes reject popup and auxiliary pointer drags",
          "[filter-control][ui][automation][gesture][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto exerciseRejectedGesture = [](juce::ModifierKeys downModifiers,
                                            juce::ModifierKeys upModifiers)
    {
        FireAudioProcessor processor;
        setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
        setPlainParameter(processor, LOW_ID, 0.0f);

        GlobalPanel panel(processor, {}, {}, {}, {}, {});
        FilterControl control(processor, panel);
        control.setBounds(0, 0, 1000, 400);
        auto& lowButton = FilterControlTestAccess::lowButton(control);
        REQUIRE_FALSE(lowButton.getBounds().isEmpty());

        const std::array<juce::String, 3> parameterIDs {
            LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID
        };
        std::array<float, 3> initialValues {};
        for (size_t index = 0; index < parameterIDs.size(); ++index)
        {
            const auto* parameter = processor.treeState.getRawParameterValue(
                parameterIDs[index]);
            REQUIRE(parameter != nullptr);
            initialValues[index] = parameter->load(std::memory_order_relaxed);
        }

        GestureCapture capture(processor,
                               { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
        const auto downPosition = lowButton.getLocalBounds().toFloat().getCentre();
        lowButton.mouseDown(makeMouseEvent(lowButton,
                                           downPosition,
                                           downModifiers,
                                           downPosition));

        const auto targetInControl = juce::Point<float> { 650.0f, 95.0f };
        const auto targetInButton = targetInControl
                                  - lowButton.getPosition().toFloat();
        lowButton.mouseDrag(makeMouseEvent(lowButton,
                                           targetInButton,
                                           downModifiers,
                                           downPosition));
        lowButton.mouseUp(makeMouseEvent(lowButton,
                                         targetInButton,
                                         upModifiers,
                                         downPosition));

        for (size_t index = 0; index < parameterIDs.size(); ++index)
        {
            const auto* parameter = processor.treeState.getRawParameterValue(
                parameterIDs[index]);
            REQUIRE(parameter != nullptr);
            INFO("Parameter " << parameterIDs[index]);
            CHECK(parameter->load(std::memory_order_relaxed)
                  == Catch::Approx(initialValues[index]));
            checkInactive(capture.forParameter(index));
        }
    };

    SECTION("physical right drag")
    {
        exerciseRejectedGesture(
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
            {});
    }

    SECTION("middle-button drag")
    {
        exerciseRejectedGesture(
            juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
            {});
    }

#if JUCE_MAC
    SECTION("macOS Control-click drag")
    {
        exerciseRejectedGesture(
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                 | juce::ModifierKeys::ctrlModifier },
            juce::ModifierKeys { juce::ModifierKeys::ctrlModifier });
    }
#endif
}

TEST_CASE("Filter graph nodes expose distinct names and usable hit targets",
          "[filter-control][ui][accessibility][layout][hit-test]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    control.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control.setVisible(true);

    const std::array<std::pair<DraggableButton*, juce::String>, 3> nodes {{
        { &FilterControlTestAccess::lowButton(control),
          "Low-cut filter frequency" },
        { &FilterControlTestAccess::peakButton(control),
          "Peak filter frequency" },
        { &FilterControlTestAccess::highButton(control),
          "High-cut filter frequency" }
    }};

    for (const auto& [node, expectedTitle] : nodes)
    {
        REQUIRE(node != nullptr);
        CHECK(node->getWidth() >= 20);
        CHECK(node->getHeight() >= 20);
        CHECK(node->getTitle() == expectedTitle);
        REQUIRE_FALSE(node->getTooltip().isEmpty());

        auto* accessibility = node->getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        CHECK(accessibility->getRole() == juce::AccessibilityRole::slider);
        CHECK(accessibility->getTitle() == expectedTitle);
        CHECK(accessibility->getHelp().containsIgnoreCase("arrow"));
    }

    control.setBounds(0, 0, 320, 160);
    for (const auto& [node, expectedTitle] : nodes)
    {
        juce::ignoreUnused(expectedTitle);
        CHECK(node->getWidth() >= 20);
        CHECK(node->getHeight() >= 20);
    }
}

TEST_CASE("Filter graph node interaction feedback fades continuously and clears off-peer",
          "[filter-control][ui][animation][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DraggableButton node;
    node.setBounds(0, 0, 24, 24);
    int dragFinishes = 0;
    node.onDrag = [](DraggableButton&, const juce::MouseEvent&) {};
    node.onDragFinished = [&dragFinishes] { ++dragFinishes; };
    node.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    node.setVisible(true);
    REQUIRE(node.isShowing());

    const auto centre = node.getLocalBounds().toFloat().getCentre();
    const auto frame = [&node]
    {
        return DraggableButtonPointerTestAccess::advanceAnimation(
            node, 1.0f / 60.0f);
    };
    const auto hover = [&node]
    {
        return DraggableButtonPointerTestAccess::hoverAnimation(node);
    };
    const auto press = [&node]
    {
        return DraggableButtonPointerTestAccess::pressAnimation(node);
    };
    const auto focus = [&node]
    {
        return DraggableButtonPointerTestAccess::focusAnimation(node);
    };

    CHECK(hover() == 0.0f);
    CHECK(press() == 0.0f);
    CHECK(focus() == 0.0f);

    node.mouseEnter(makeMouseEvent(node, centre, {}, centre));
    REQUIRE(frame());
    CHECK(hover() > 0.0f);
    CHECK(hover() < 1.0f);
    CHECK(press() == 0.0f);

    node.mouseDown(makeMouseEvent(
        node,
        centre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
        centre));
    REQUIRE(frame());
    CHECK(press() > 0.0f);
    CHECK(press() < 1.0f);

    node.mouseUp(makeMouseEvent(node, centre, {}, centre));
    CHECK(dragFinishes == 1);
    const auto pressBeforeRelease = press();
    for (int frameIndex = 0; frameIndex < 12; ++frameIndex)
        frame();
    CHECK(press() < pressBeforeRelease);
    CHECK(hover() > 0.0f);

    node.mouseExit(makeMouseEvent(node, { -1.0f, -1.0f }, {}, centre));
    const auto hoverBeforeExit = hover();
    for (int frameIndex = 0; frameIndex < 12; ++frameIndex)
        frame();
    CHECK(hover() < hoverBeforeExit);

    node.grabKeyboardFocus();
    REQUIRE(node.hasKeyboardFocus(true));
    REQUIRE(frame());
    CHECK(focus() > 0.0f);
    CHECK(focus() < 1.0f);

    node.giveAwayKeyboardFocus();
    const auto focusBeforeLoss = focus();
    for (int frameIndex = 0; frameIndex < 12; ++frameIndex)
        frame();
    CHECK(focus() < focusBeforeLoss);

    node.mouseEnter(makeMouseEvent(node, centre, {}, centre));
    frame();
    REQUIRE(hover() > 0.0f);
    node.setVisible(false);
    CHECK(hover() == 0.0f);
    CHECK(press() == 0.0f);
    CHECK(focus() == 0.0f);

    node.setVisible(true);
    REQUIRE(node.isShowing());
    node.mouseEnter(makeMouseEvent(node, centre, {}, centre));
    frame();
    REQUIRE(hover() > 0.0f);
    node.setEnabled(false);
    CHECK(hover() == 0.0f);
    CHECK(press() == 0.0f);
    CHECK(focus() == 0.0f);

    node.setEnabled(true);
    node.mouseEnter(makeMouseEvent(node, centre, {}, centre));
    frame();
    REQUIRE(hover() > 0.0f);
    node.removeFromDesktop();
    REQUIRE_FALSE(node.isShowing());
    DraggableButtonPointerTestAccess::pollAnimationLifecycle(node);
    CHECK(hover() == 0.0f);
    CHECK(press() == 0.0f);
    CHECK(focus() == 0.0f);
    CHECK(dragFinishes == 1);
}

TEST_CASE("Filter graph node focus presentation follows keyboard modality",
          "[filter-control][ui][input][focus][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    DraggableButton node;
    node.setBounds(0, 0, 24, 24);
    node.onDrag = [](DraggableButton&, const juce::MouseEvent&) {};
    node.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    node.setVisible(true);
    REQUIRE(node.isShowing());

    node.grabKeyboardFocus();
    REQUIRE(node.hasKeyboardFocus(true));
    DraggableButtonPointerTestAccess::notifyFocusGained(
        node, juce::Component::focusChangedByMouseClick);
    CHECK_FALSE(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));

    DraggableButtonPointerTestAccess::notifyFocusGained(
        node, juce::Component::focusChangedByTabKey);
    CHECK(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));
    DraggableButtonPointerTestAccess::notifyFocusLost(node);
    CHECK_FALSE(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));

    DraggableButtonPointerTestAccess::notifyFocusGained(
        node, juce::Component::focusChangedDirectly);
    REQUIRE(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));
    const auto centre = node.getLocalBounds().toFloat().getCentre();
    node.mouseDown(makeMouseEvent(
        node,
        centre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
        centre));
    CHECK_FALSE(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));
    node.mouseUp(makeMouseEvent(node, centre, {}, centre));

    REQUIRE(node.keyPressed(juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));
    DraggableButtonPointerTestAccess::advanceAnimation(
        node, 1.0f / 60.0f);
    CHECK(DraggableButtonPointerTestAccess::focusAnimation(node) > 0.0f);

    node.setVisible(false);
    CHECK_FALSE(DraggableButtonPointerTestAccess::isKeyboardFocusVisible(node));
    CHECK(DraggableButtonPointerTestAccess::focusAnimation(node) == 0.0f);
}

TEST_CASE("Filter graph nodes keep a primary drag owned by one pointer source",
          "[filter-control][ui][automation][gesture][input][source][multitouch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    auto& lowButton = FilterControlTestAccess::lowButton(control);
    REQUIRE_FALSE(lowButton.getBounds().isEmpty());

    const std::array<juce::String, 3> parameterIDs {
        LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID
    };
    GestureCapture capture(processor,
                           { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto downPosition = lowButton.getLocalBounds().toFloat().getCentre();
    lowButton.mouseDown(makeMouseEvent(lowButton,
                                       downPosition,
                                       primary,
                                       downPosition));

    const auto acceptedInControl = juce::Point<float> { 650.0f, 95.0f };
    const auto acceptedInButton = acceptedInControl
                                - lowButton.getPosition().toFloat();
    lowButton.mouseDrag(makeMouseEvent(lowButton,
                                       acceptedInButton,
                                       primary,
                                       downPosition));

    std::array<float, 3> acceptedValues {};
    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        acceptedValues[index] = parameter->load(std::memory_order_relaxed);
        REQUIRE(capture.forParameter(index).beginCount == 1);
        REQUIRE(capture.forParameter(index).endCount == 0);
        REQUIRE(capture.forParameter(index).depth == 1);
    }

    const auto mainSource = juce::Desktop::getInstance().getMainMouseSource();
    constexpr auto ownerType = juce::MouseInputSource::touch;
    const int ownerIndex = mainSource.getIndex() + 23;
    DraggableButtonPointerTestAccess::setTrackedPointerSource(lowButton,
                                                              ownerType,
                                                              ownerIndex);

    const auto foreignInControl = juce::Point<float> { 270.0f, 305.0f };
    const auto foreignInButton = foreignInControl
                               - lowButton.getPosition().toFloat();

    SECTION("foreign drag and release are ignored")
    {
        lowButton.mouseDrag(makeMouseEvent(lowButton,
                                           foreignInButton,
                                           primary,
                                           downPosition));
        lowButton.mouseUp(makeMouseEvent(lowButton,
                                         foreignInButton,
                                         {},
                                         downPosition));
    }

    SECTION("foreign down cannot replace the owning gesture")
    {
        lowButton.mouseDown(makeMouseEvent(lowButton,
                                           foreignInButton,
                                           primary,
                                           foreignInButton));
    }

    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        INFO("Parameter " << parameterIDs[index]);
        CHECK(parameter->load(std::memory_order_relaxed)
              == Catch::Approx(acceptedValues[index]));
        CHECK(capture.forParameter(index).beginCount == 1);
        CHECK(capture.forParameter(index).endCount == 0);
        CHECK(capture.forParameter(index).depth == 1);
    }
    CHECK(DraggableButtonPointerTestAccess::hasPrimaryDrag(lowButton));

    DraggableButtonPointerTestAccess::setTrackedPointerSource(
        lowButton, mainSource.getType(), mainSource.getIndex());
    lowButton.mouseUp(makeMouseEvent(lowButton,
                                     acceptedInButton,
                                     {},
                                     downPosition));

    for (size_t index = 0; index < parameterIDs.size(); ++index)
        checkBalanced(capture.forParameter(index));
}

TEST_CASE("Filter graph nodes recover when a primary pointer release is lost",
          "[filter-control][ui][automation][gesture][input][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    auto& lowButton = FilterControlTestAccess::lowButton(control);
    REQUIRE_FALSE(lowButton.getBounds().isEmpty());

    const std::array<juce::String, 3> parameterIDs {
        LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID
    };
    GestureCapture capture(processor,
                           { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    const auto downPosition = lowButton.getLocalBounds().toFloat().getCentre();
    lowButton.mouseDown(makeMouseEvent(lowButton,
                                       downPosition,
                                       primary,
                                       downPosition));

    const auto acceptedInControl = juce::Point<float> { 650.0f, 95.0f };
    const auto acceptedInButton = acceptedInControl
                                - lowButton.getPosition().toFloat();
    lowButton.mouseDrag(makeMouseEvent(lowButton,
                                       acceptedInButton,
                                       primary,
                                       downPosition));

    std::array<float, 3> acceptedValues {};
    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        acceptedValues[index] = parameter->load(std::memory_order_relaxed);
        REQUIRE(capture.forParameter(index).beginCount == 1);
        REQUIRE(capture.forParameter(index).endCount == 0);
        REQUIRE(capture.forParameter(index).depth == 1);
    }

    SECTION("owner movement without the primary button closes the stale drag")
    {
        lowButton.mouseMove(makeMouseEvent(lowButton,
                                           acceptedInButton,
                                           {},
                                           downPosition));
    }

    SECTION("a fresh popup down from the owner closes the stale drag")
    {
        lowButton.mouseDown(makeMouseEvent(
            lowButton,
            acceptedInButton,
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
            acceptedInButton));
    }

    CHECK_FALSE(DraggableButtonPointerTestAccess::hasPrimaryDrag(lowButton));
    for (size_t index = 0; index < parameterIDs.size(); ++index)
        checkBalanced(capture.forParameter(index));

    const auto rejectedInControl = juce::Point<float> { 270.0f, 305.0f };
    const auto rejectedInButton = rejectedInControl
                                - lowButton.getPosition().toFloat();
    lowButton.mouseDrag(makeMouseEvent(lowButton,
                                       rejectedInButton,
                                       primary,
                                       downPosition));

    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        INFO("Parameter " << parameterIDs[index]);
        CHECK(parameter->load(std::memory_order_relaxed)
              == Catch::Approx(acceptedValues[index]));
        checkBalanced(capture.forParameter(index));
    }
}

TEST_CASE("Filter graph nodes discard primary drag ownership when hidden",
          "[filter-control][ui][automation][gesture][input][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    FilterControl control(processor, panel);
    control.setBounds(0, 0, 1000, 400);
    auto& lowButton = FilterControlTestAccess::lowButton(control);
    REQUIRE_FALSE(lowButton.getBounds().isEmpty());

    const std::array<juce::String, 3> parameterIDs {
        LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID
    };
    const auto downPosition = lowButton.getLocalBounds().toFloat().getCentre();

    {
        GestureCapture acceptedCapture(processor,
                                       { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
        lowButton.mouseDown(makeMouseEvent(
            lowButton,
            downPosition,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier },
            downPosition));

        // This headless component is not showing, matching a host that hides
        // the editor before JUCE can deliver the corresponding mouseUp.
        control.visibilityChanged();
        checkBalanced(acceptedCapture.forParameter(0));
    }

    std::array<float, 3> valuesAfterHide {};
    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        valuesAfterHide[index] = parameter->load(std::memory_order_relaxed);
    }

    GestureCapture rejectedCapture(processor,
                                   { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    lowButton.mouseDown(makeMouseEvent(
        lowButton,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        downPosition));

    const auto dragPosition = juce::Point<float> {
        static_cast<float>(lowButton.getWidth() - 1), 1.0f
    };
    lowButton.mouseDrag(makeMouseEvent(
        lowButton,
        dragPosition,
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        downPosition));
    lowButton.mouseUp(makeMouseEvent(lowButton, dragPosition, {}, downPosition));

    for (size_t index = 0; index < parameterIDs.size(); ++index)
    {
        const auto* parameter = processor.treeState.getRawParameterValue(
            parameterIDs[index]);
        REQUIRE(parameter != nullptr);
        INFO("Parameter " << parameterIDs[index]);
        CHECK(parameter->load(std::memory_order_relaxed)
              == Catch::Approx(valuesAfterHide[index]));
        checkInactive(rejectedCapture.forParameter(index));
    }
}

TEST_CASE("Filter graph drag completion survives control release from a host callback",
          "[filter-control][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    auto control = std::make_unique<FilterControl>(processor, panel);
    control->setBounds(0, 0, 1000, 400);
    auto* lowButton = &FilterControlTestAccess::lowButton(*control);
    REQUIRE_FALSE(lowButton->getBounds().isEmpty());

    GestureCapture capture(processor,
                           { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    const auto downPosition = lowButton->getLocalBounds().toFloat().getCentre();
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    lowButton->mouseDown(makeMouseEvent(*lowButton,
                                        downPosition,
                                        leftButton,
                                        downPosition));

    const auto targetInControl = juce::Point<float> { 650.0f, 95.0f };
    const auto targetInButton = targetInControl
                              - lowButton->getPosition().toFloat();
    lowButton->mouseDrag(makeMouseEvent(*lowButton,
                                        targetInButton,
                                        leftButton,
                                        downPosition));

    for (size_t parameter = 0; parameter < 3; ++parameter)
    {
        INFO("Parameter " << parameter);
        CHECK(capture.forParameter(parameter).beginCount == 1);
        CHECK(capture.forParameter(parameter).depth == 1);
    }

    ControlReleaseOnParameterCallback releaseOnEnd(processor,
                                                   LOW_ID,
                                                   ParameterCallbackStage::end,
                                                   control);
    SECTION("explicit mouse release")
    {
        lowButton->mouseUp(makeMouseEvent(*lowButton,
                                          targetInButton,
                                          {},
                                          downPosition));
    }

    SECTION("movement after a missing mouse release")
    {
        lowButton->mouseMove(makeMouseEvent(*lowButton,
                                            targetInButton,
                                            {},
                                            downPosition));
    }

    CHECK(releaseOnEnd.didRelease());
    CHECK(control == nullptr);
    checkBalanced(capture.forParameter(0));
    checkBalanced(capture.forParameter(1));
    checkBalanced(capture.forParameter(2));
}

TEST_CASE("Filter graph drag publication survives control release from host callbacks",
          "[filter-control][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    auto control = std::make_unique<FilterControl>(processor, panel);
    control->setBounds(0, 0, 1000, 400);
    auto* lowButton = &FilterControlTestAccess::lowButton(*control);
    REQUIRE_FALSE(lowButton->getBounds().isEmpty());

    GestureCapture capture(processor,
                           { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    const auto downPosition = lowButton->getLocalBounds().toFloat().getCentre();
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("gesture begin")
    {
        ControlReleaseOnParameterCallback releaseOnBegin(
            processor, LOW_ID, ParameterCallbackStage::begin, control);
        lowButton->mouseDown(makeMouseEvent(*lowButton,
                                            downPosition,
                                            leftButton,
                                            downPosition));

        CHECK(releaseOnBegin.didRelease());
        CHECK(control == nullptr);
        const auto& selectionEvents = capture.forParameter(0);
        CHECK(selectionEvents.beginCount == 1);
        CHECK(selectionEvents.endCount == 1);
        CHECK(selectionEvents.valueCount == 0);
        CHECK(selectionEvents.depth == 0);
        CHECK(selectionEvents.minimumDepth == 0);
        CHECK(selectionEvents.maximumDepth == 1);
        CHECK_FALSE(selectionEvents.valueOutsideGesture);
        CHECK(selectionEvents.order == std::vector<char> { 'B', 'E' });
        checkInactive(capture.forParameter(1));
        checkInactive(capture.forParameter(2));
    }

    SECTION("parameter value")
    {
        ControlReleaseOnParameterCallback releaseOnValue(
            processor, LOW_ID, ParameterCallbackStage::value, control);
        lowButton->mouseDown(makeMouseEvent(*lowButton,
                                            downPosition,
                                            leftButton,
                                            downPosition));

        CHECK(releaseOnValue.didRelease());
        CHECK(control == nullptr);
        checkBalanced(capture.forParameter(0));
        checkInactive(capture.forParameter(1));
        checkInactive(capture.forParameter(2));
    }
}

TEST_CASE("Filter graph Q wheel survives control release from a host callback",
          "[filter-control][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    auto control = std::make_unique<FilterControl>(processor, panel);
    control->setBounds(0, 0, 1000, 400);
    control->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    control->setVisible(true);
    auto* lowButton = &FilterControlTestAccess::lowButton(*control);
    REQUIRE_FALSE(lowButton->getBounds().isEmpty());

    GestureCapture capture(processor, { LOWCUT_Q_ID });
    ControlReleaseOnParameterCallback releaseOnValue(
        processor, LOWCUT_Q_ID, ParameterCallbackStage::value, control);
    const auto position = lowButton->getLocalBounds().toFloat().getCentre();
    juce::MouseWheelDetails wheel;
    wheel.deltaY = 0.4f;
    lowButton->mouseWheelMove(makeMouseEvent(*lowButton,
                                             position,
                                             {},
                                             position),
                                  wheel);

    CHECK(releaseOnValue.didRelease());
    CHECK(control == nullptr);
    checkBalanced(capture.forParameter(0));
}

TEST_CASE("Filter graph disable survives control release from a host callback",
          "[filter-control][ui][automation][gesture][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    setPlainParameter(processor, LOW_ID, 0.0f);

    GlobalPanel panel(processor, {}, {}, {}, {}, {});
    auto control = std::make_unique<FilterControl>(processor, panel);
    control->setBounds(0, 0, 1000, 400);
    auto* lowButton = &FilterControlTestAccess::lowButton(*control);
    REQUIRE_FALSE(lowButton->getBounds().isEmpty());

    GestureCapture capture(processor,
                           { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    const auto downPosition = lowButton->getLocalBounds().toFloat().getCentre();
    const auto leftButton = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    lowButton->mouseDown(makeMouseEvent(*lowButton,
                                        downPosition,
                                        leftButton,
                                        downPosition));

    const auto targetInControl = juce::Point<float> { 650.0f, 95.0f };
    const auto targetInButton = targetInControl
                              - lowButton->getPosition().toFloat();
    lowButton->mouseDrag(makeMouseEvent(*lowButton,
                                        targetInButton,
                                        leftButton,
                                        downPosition));

    setPlainParameter(processor, FILTER_BYPASS_ID, 0.0f);
    ControlReleaseOnParameterCallback releaseOnEnd(processor,
                                                   LOW_ID,
                                                   ParameterCallbackStage::end,
                                                   control);
    FilterControlTestAccess::updateButtonStates(*control);

    CHECK(releaseOnEnd.didRelease());
    CHECK(control == nullptr);
    checkBalanced(capture.forParameter(0));
    checkBalanced(capture.forParameter(1));
    checkBalanced(capture.forParameter(2));
}

TEST_CASE("Editor lifecycle boundaries close every filter node gesture exactly once",
          "[filter-control][ui][automation][gesture][lifecycle][editor]")
{
    struct NodeCase
    {
        int node = 0;
        std::array<juce::String, 3> parameters;
    };
    const std::array<NodeCase, 3> cases {{
        { 0, { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID } },
        { 1, { BAND_ID, PEAK_FREQ_ID, PEAK_GAIN_ID } },
        { 2, { HIGH_ID, HIGHCUT_FREQ_ID, HIGHCUT_GAIN_ID } }
    }};

    for (const auto& nodeCase : cases)
    {
        DYNAMIC_SECTION("node " << nodeCase.node << " editor hide")
        {
            juce::ScopedJuceInitialiser_GUI gui;
            FireAudioProcessor processor;
            processor.hasUpdateCheckBeenPerformed = true;
            setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
            auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
            setPlainParameter(processor, nodeCase.parameters[0], 0.0f);
            editor->setVisible(true);
            auto* control = findDescendant<FilterControl>(*editor);
            REQUIRE(control != nullptr);
            control->setBounds(0, 0, 1000, 400);
            FilterControlTestAccess::updateButtonStates(*control);
            auto* button = nodeCase.node == 0 ? &FilterControlTestAccess::lowButton(*control)
                         : nodeCase.node == 1 ? &FilterControlTestAccess::peakButton(*control)
                                              : &FilterControlTestAccess::highButton(*control);
            GestureCapture capture(processor, { nodeCase.parameters[0], nodeCase.parameters[1],
                                                nodeCase.parameters[2] });
            const auto position = button->getLocalBounds().toFloat().getCentre();
            button->mouseDown(makeMouseEvent(*button, position,
                juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
            const auto dragPosition = juce::Point<float> { 700.0f, 100.0f }
                                    - button->getPosition().toFloat();
            button->mouseDrag(makeMouseEvent(*button, dragPosition,
                juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
            REQUIRE(FilterControlTestAccess::hasTransientState(*control));

            editor->setVisible(false);
            CHECK_FALSE(FilterControlTestAccess::hasTransientState(*control));
            CHECK_FALSE(DraggableButtonPointerTestAccess::hasPrimaryDrag(*button));
            for (size_t parameter = 0; parameter < 3; ++parameter)
                checkClosedExactlyOnce(capture.forParameter(parameter));

            editor->setVisible(false);
            button->mouseUp(makeMouseEvent(*button, position, {}, position));
            for (size_t parameter = 0; parameter < 3; ++parameter)
                CHECK(capture.forParameter(parameter).endCount == 1);
        }

        DYNAMIC_SECTION("node " << nodeCase.node << " editor disable")
        {
            juce::ScopedJuceInitialiser_GUI gui;
            FireAudioProcessor processor;
            processor.hasUpdateCheckBeenPerformed = true;
            setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
            auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
            setPlainParameter(processor, nodeCase.parameters[0], 0.0f);
            editor->setVisible(true);
            auto* control = findDescendant<FilterControl>(*editor);
            REQUIRE(control != nullptr);
            control->setBounds(0, 0, 1000, 400);
            FilterControlTestAccess::updateButtonStates(*control);
            auto* button = nodeCase.node == 0 ? &FilterControlTestAccess::lowButton(*control)
                         : nodeCase.node == 1 ? &FilterControlTestAccess::peakButton(*control)
                                              : &FilterControlTestAccess::highButton(*control);
            GestureCapture capture(processor, { nodeCase.parameters[0], nodeCase.parameters[1],
                                                nodeCase.parameters[2] });
            const auto position = button->getLocalBounds().toFloat().getCentre();
            button->mouseDown(makeMouseEvent(*button, position,
                juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
            const auto dragPosition = juce::Point<float> { 700.0f, 100.0f }
                                    - button->getPosition().toFloat();
            button->mouseDrag(makeMouseEvent(*button, dragPosition,
                juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
            REQUIRE(FilterControlTestAccess::hasTransientState(*control));

            editor->setEnabled(false);
            CHECK_FALSE(FilterControlTestAccess::hasTransientState(*control));
            for (size_t parameter = 0; parameter < 3; ++parameter)
                checkClosedExactlyOnce(capture.forParameter(parameter));

            editor->setEnabled(false);
            button->mouseUp(makeMouseEvent(*button, position, {}, position));
            for (size_t parameter = 0; parameter < 3; ++parameter)
                CHECK(capture.forParameter(parameter).endCount == 1);
        }
    }
}

TEST_CASE("Editor hide survives deletion from a filter gesture-end callback",
          "[filter-control][ui][automation][gesture][lifecycle][editor][deletion]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    setPlainParameter(processor, FILTER_BYPASS_ID, 1.0f);
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    setPlainParameter(processor, LOW_ID, 0.0f);
    editor->setVisible(true);
    auto* control = findDescendant<FilterControl>(*editor);
    REQUIRE(control != nullptr);
    control->setBounds(0, 0, 1000, 400);
    FilterControlTestAccess::updateButtonStates(*control);
    auto& button = FilterControlTestAccess::lowButton(*control);
    const auto position = button.getLocalBounds().toFloat().getCentre();
    GestureCapture capture(processor, { LOW_ID, LOWCUT_FREQ_ID, LOWCUT_GAIN_ID });
    button.mouseDown(makeMouseEvent(button, position,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
    const auto dragPosition = juce::Point<float> { 700.0f, 100.0f }
                            - button.getPosition().toFloat();
    button.mouseDrag(makeMouseEvent(button, dragPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }, position));
    EditorReleaseOnGestureEnd releaseOnEnd(processor, LOWCUT_FREQ_ID, editor);

    editor->setVisible(false);
    CHECK(releaseOnEnd.didRelease());
    CHECK(editor == nullptr);
    checkClosedExactlyOnce(capture.forParameter(0));
    checkClosedExactlyOnce(capture.forParameter(1));
    checkClosedExactlyOnce(capture.forParameter(2));
}
