#include <Panels/ControlPanel/GlobalPanel.h>
#include <Panels/SpectrogramPanel/FilterControl.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <vector>

struct FilterControlTestAccess
{
    static DraggableButton& lowButton(FilterControl& control)
    {
        return control.draggableLowButton;
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

void setPlainParameter(FireAudioProcessor& processor,
                       const juce::String& parameterID,
                       float plainValue)
{
    auto* parameter = processor.treeState.getParameter(parameterID);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
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
} // namespace

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
