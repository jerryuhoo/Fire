#include <GUI/LookAndFeel.h>
#include <GUI/ModulatableSlider.h>
#include <GUI/PrimaryButton.h>
#include <Panels/ControlPanel/BandPanel.h>
#include <Panels/ControlPanel/GlobalPanel.h>
#include <PluginEditor.h>
#include <PluginProcessor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>
#include <vector>

struct ModulatableSliderInteractionTestAccess
{
    static void setTrackedPointerSource(
        ModulatableSlider& slider,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        slider.pointerSourceType = type;
        slider.pointerSourceIndex = index;
    }

    static bool isMainGesture(const ModulatableSlider& slider) noexcept
    {
        return slider.activePointerGesture
               == ModulatableSlider::PointerGesture::mainSlider;
    }

    static bool isModulationGesture(const ModulatableSlider& slider) noexcept
    {
        return slider.activePointerGesture
               == ModulatableSlider::PointerGesture::modulationHandle;
    }

    static bool isRejectedSequence(const ModulatableSlider& slider) noexcept
    {
        return slider.activePointerGesture
               == ModulatableSlider::PointerGesture::rejected;
    }

    static bool isPopupSequence(const ModulatableSlider& slider) noexcept
    {
        return slider.activePointerGesture
               == ModulatableSlider::PointerGesture::popupMenu;
    }

    static juce::String getPopupTargetParameterID(
        const ModulatableSlider& slider)
    {
        return slider.popupTargetParameterID;
    }

    static std::function<void(int)> createAssignmentHandler(
        ModulatableSlider& slider,
        juce::String targetParameterID)
    {
        return slider.createLfoAssignmentMenuResultHandler(
            std::move(targetParameterID));
    }

    static std::function<void(int)> createModulationHandler(
        ModulatableSlider& slider,
        juce::String targetParameterID)
    {
        return slider.createModulationMenuResultHandler(
            std::move(targetParameterID));
    }

    static void primeAnimations(ModulatableSlider& slider) noexcept
    {
        slider.hoverAnimation = 0.75f;
        slider.pressAnimation = 0.65f;
    }

    static juce::Label* getForwardedValueLabel(
        ModulatableSlider& slider) noexcept
    {
        return slider.forwardedValueLabel.getComponent();
    }

    static const juce::MouseEvent* getLastAcceptedPointerEvent(
        const ModulatableSlider& slider) noexcept
    {
        return slider.lastAcceptedPointerEvent.has_value()
                   ? &*slider.lastAcceptedPointerEvent
                   : nullptr;
    }

    static void forwardValueLabelMouseDown(
        ModulatableSlider& slider,
        const juce::MouseEvent& event)
    {
        slider.valueLabelPopupForwarder.mouseDown(event);
    }

    static void forwardValueLabelMouseUp(
        ModulatableSlider& slider,
        const juce::MouseEvent& event)
    {
        slider.valueLabelPopupForwarder.mouseUp(event);
    }
};

namespace
{
juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::Point<float> position,
                                juce::ModifierKeys modifiers = {},
                                juce::Point<float> mouseDownPosition = {},
                                bool wasDragged = false,
                                int clickCount = 1)
{
    const auto time = juce::Time::getCurrentTime();
    if (mouseDownPosition == juce::Point<float>())
        mouseDownPosition = position;

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
             clickCount,
             wasDragged };
}

const juce::ModifierKeys primaryButton {
    juce::ModifierKeys::leftButtonModifier
};

std::vector<juce::ModifierKeys> rejectedPointerModifiers()
{
    std::vector<juce::ModifierKeys> result {
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::middleButtonModifier },
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                             | juce::ModifierKeys::rightButtonModifier }
    };

#if JUCE_MAC
    result.emplace_back(juce::ModifierKeys::leftButtonModifier
                        | juce::ModifierKeys::ctrlModifier);
#endif

    return result;
}

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

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override {}

    void audioProcessorParameterChangeGestureBegin(
        juce::AudioProcessor*, int changedParameterIndex) override
    {
        if (changedParameterIndex == parameterIndex)
        {
            ++beginCount;
            ++depth;
            maximumDepth = juce::jmax(maximumDepth, depth);
        }
    }

    void audioProcessorParameterChangeGestureEnd(
        juce::AudioProcessor*, int changedParameterIndex) override
    {
        if (changedParameterIndex == parameterIndex)
        {
            ++endCount;
            --depth;
            minimumDepth = juce::jmin(minimumDepth, depth);
        }
    }

    FireAudioProcessor& processor;
    int parameterIndex = -1;
    int beginCount = 0;
    int endCount = 0;
    int depth = 0;
    int maximumDepth = 0;
    int minimumDepth = 0;
};

template <typename Owner>
class DeleteOwnerOnGestureEnd final : public juce::AudioProcessorListener
{
public:
    DeleteOwnerOnGestureEnd(FireAudioProcessor& processorToObserve,
                            std::unique_ptr<Owner>& ownerToDelete)
        : processor(processorToObserve), owner(ownerToDelete)
    {
        processor.addListener(this);
    }

    ~DeleteOwnerOnGestureEnd() override
    {
        processor.removeListener(this);
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails&) override {}

    void audioProcessorParameterChangeGestureBegin(
        juce::AudioProcessor*, int) override
    {
        ++beginCount;
    }

    void audioProcessorParameterChangeGestureEnd(
        juce::AudioProcessor*, int) override
    {
        ++endCount;
        if (owner != nullptr)
        {
            owner.reset();
            callbackCompleted = true;
        }
    }

    FireAudioProcessor& processor;
    std::unique_ptr<Owner>& owner;
    int beginCount = 0;
    int endCount = 0;
    bool callbackCompleted = false;
};

template <typename ComponentType>
ComponentType* findDescendant(juce::Component& root)
{
    if (auto* match = dynamic_cast<ComponentType*>(&root))
        return match;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* match = findDescendant<ComponentType>(*child))
                return match;

    return nullptr;
}

juce::Button* findButtonWithText(juce::Component& root,
                                 const juce::String& text)
{
    if (auto* button = dynamic_cast<juce::Button*>(&root))
        if (button->getButtonText() == text)
            return button;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* match = findButtonWithText(*child, text))
                return match;

    return nullptr;
}

juce::Slider* findSliderAttachedToLabel(juce::Component& root,
                                        const juce::String& labelText)
{
    if (auto* label = dynamic_cast<juce::Label*>(&root);
        label != nullptr && label->getText() == labelText)
        return dynamic_cast<juce::Slider*>(label->getAttachedComponent());

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* child = root.getChildComponent(index))
            if (auto* slider = findSliderAttachedToLabel(*child, labelText))
                return slider;

    return nullptr;
}

void beginPrimaryGesture(juce::Slider& slider)
{
    const auto position = slider.getLocalBounds().toFloat().getCentre();
    slider.mouseDown(makeMouseEvent(slider, position, primaryButton));
}

void performPrimaryClick(juce::Button& button)
{
    auto& component = static_cast<juce::Component&>(button);
    const auto position = component.getLocalBounds().toFloat().getCentre();
    component.mouseDown(makeMouseEvent(component, position, primaryButton));
    component.mouseUp(makeMouseEvent(component, position));
}

void checkBalanced(const ParameterGestureCapture& capture,
                   int expectedGestureCount = 1)
{
    CHECK(capture.beginCount == expectedGestureCount);
    CHECK(capture.endCount == expectedGestureCount);
    CHECK(capture.depth == 0);
    CHECK(capture.maximumDepth == 1);
    CHECK(capture.minimumDepth == 0);
}
} // namespace

TEST_CASE("Modulatable slider titles preserve the advertised header hit target",
          "[modulatable-slider][ui][hit-test]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    ModulatableSlider slider;
    slider.setLookAndFeel(&lookAndFeel);
    slider.setBounds(0, 0, 120, 120);
    slider.setVisible(true);

    juce::Label* title = nullptr;
    for (auto* child : slider.getChildren())
        if (auto* candidate = dynamic_cast<juce::Label*>(child))
        {
            title = candidate;
            break;
        }

    REQUIRE(title != nullptr);
    REQUIRE_FALSE(title->getBounds().isEmpty());
    const auto titleCentre = title->getBounds().getCentre();
    REQUIRE(slider.hitTest(titleCentre.x, titleCentre.y));

    // Event dispatch must fall through the presentation label to the Slider.
    CHECK_FALSE(title->hitTest(title->getWidth() / 2, title->getHeight() / 2));
    CHECK(slider.getComponentAt(titleCentre) == &slider);

    slider.setLookAndFeel(nullptr);
}

TEST_CASE("Hover value labels forward only popup gestures to modulatable sliders",
          "[modulatable-slider][ui][input][popup][value-label][hit-test]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    juce::Component root;
    root.setBounds(0, 0, 320, 220);
    root.setLookAndFeel(&lookAndFeel);
    root.setVisible(true);

    ModulatableSlider slider;
    root.addAndMakeVisible(slider);
    slider.setBounds(80, 50, 120, 120);
    slider.mouseEnter(makeMouseEvent(
        slider, slider.getLocalBounds().toFloat().getCentre()));
    REQUIRE(slider.getTextBoxPosition() == juce::Slider::TextBoxAbove);

    juce::Label* valueLabel = nullptr;
    for (auto* child : slider.getChildren())
        if (auto* candidate = dynamic_cast<juce::Label*>(child);
            candidate != nullptr
            && candidate->hitTest(candidate->getWidth() / 2,
                                  candidate->getHeight() / 2))
            valueLabel = candidate;

    REQUIRE(valueLabel != nullptr);
    REQUIRE(valueLabel->isEditable());
    const auto peerPosition = root.getLocalPoint(
        valueLabel, valueLabel->getLocalBounds().toFloat().getCentre());
    REQUIRE(root.getComponentAt(peerPosition.toInt()) == valueLabel);
    REQUIRE(ModulatableSliderInteractionTestAccess::getForwardedValueLabel(
                slider)
            == valueLabel);

    const juce::ModifierKeys popupButton {
        juce::ModifierKeys::rightButtonModifier
    };
    const auto labelCentre =
        valueLabel->getLocalBounds().toFloat().getCentre();
    const auto popupDownEvent =
        makeMouseEvent(*valueLabel, labelCentre, popupButton);
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
        slider, popupDownEvent);
    CHECK(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
    const auto* acceptedEvent =
        ModulatableSliderInteractionTestAccess::getLastAcceptedPointerEvent(
            slider);
    REQUIRE(acceptedEvent != nullptr);
    CHECK(acceptedEvent->eventComponent == &slider);
    CHECK(acceptedEvent->originalComponent == valueLabel);
    CHECK(acceptedEvent->position
          == slider.getLocalPoint(valueLabel, labelCentre));
    CHECK(acceptedEvent->mouseDownPosition
          == slider.getLocalPoint(valueLabel, labelCentre));
    CHECK(acceptedEvent->getScreenPosition()
          == popupDownEvent.getScreenPosition());
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseUp(
        slider, makeMouseEvent(*valueLabel, labelCentre));
    CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));

    int mainDragStarts = 0;
    slider.onMainDragStart =
        [&](ModulatableSlider*) { ++mainDragStarts; };
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
        slider, makeMouseEvent(*valueLabel, labelCentre, primaryButton));
    CHECK_FALSE(slider.hasActiveInteraction());
    CHECK(mainDragStarts == 0);
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseUp(
        slider, makeMouseEvent(*valueLabel, labelCentre));
    CHECK(valueLabel->isEditableOnSingleClick());

    const auto oldValueLabel =
        juce::Component::SafePointer<juce::Label>(valueLabel);
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
        slider, makeMouseEvent(*valueLabel, labelCentre, popupButton));
    REQUIRE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
    slider.setColour(juce::Slider::textBoxTextColourId,
                     juce::Colours::magenta);
    CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
    CHECK(oldValueLabel == nullptr);

    valueLabel =
        ModulatableSliderInteractionTestAccess::getForwardedValueLabel(slider);
    REQUIRE(valueLabel != nullptr);
    REQUIRE(valueLabel->isEditable());
    const auto rebuiltLabelCentre =
        valueLabel->getLocalBounds().toFloat().getCentre();
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
        slider,
        makeMouseEvent(*valueLabel, rebuiltLabelCentre, popupButton));
    REQUIRE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseUp(
        slider, makeMouseEvent(*valueLabel, rebuiltLabelCentre));
    CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));

#if JUCE_MAC
    const juce::ModifierKeys controlClick {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::ctrlModifier
    };
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
        slider,
        makeMouseEvent(*valueLabel, rebuiltLabelCentre, controlClick));
    REQUIRE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
    ModulatableSliderInteractionTestAccess::forwardValueLabelMouseUp(
        slider, makeMouseEvent(*valueLabel, rebuiltLabelCentre));
    CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
#endif

    root.setLookAndFeel(nullptr);
}

TEST_CASE("Modulatable sliders reject auxiliary input and auxiliary double-clicks",
          "[modulatable-slider][ui][input][primary]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.8, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, 0.25);
    slider.isModulated = true;
    slider.parameterID = "strict-input";

    int assignCount = 0;
    int mainStartCount = 0;
    int modStartCount = 0;
    int resetCount = 0;
    slider.onClickInAssignMode = [&](const juce::String&) { ++assignCount; };
    slider.onMainDragStart = [&](ModulatableSlider*) { ++mainStartCount; };
    slider.onModDragStart = [&](ModulatableSlider*) { ++modStartCount; };
    slider.onModulationReset = [&] { ++resetCount; };

    const auto mainPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto handlePosition = slider.getModulationHandleBounds().getCentre();
    for (const auto modifiers : rejectedPointerModifiers())
    {
        CAPTURE(modifiers.getRawFlags());
        slider.mouseDown(makeMouseEvent(slider, mainPosition, modifiers));
        slider.mouseDoubleClick(makeMouseEvent(
            slider, mainPosition, modifiers, mainPosition, false, 2));
        slider.mouseDown(makeMouseEvent(slider, handlePosition, modifiers));
        slider.mouseDoubleClick(makeMouseEvent(
            slider, handlePosition, modifiers, handlePosition, false, 2));

        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(slider.getValue() == Catch::Approx(0.8));
    }

    CHECK(assignCount == 0);
    CHECK(mainStartCount == 0);
    CHECK(modStartCount == 0);
    CHECK(resetCount == 0);

    slider.mouseDown(makeMouseEvent(slider, mainPosition, primaryButton));
    CHECK(assignCount == 1);
}

TEST_CASE("Disabled modulatable sliders reject every double-click path",
          "[modulatable-slider][ui][input][primary][disabled]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.8, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, 0.25);
    slider.isModulated = true;
    int modulationResets = 0;
    slider.onModulationReset = [&] { ++modulationResets; };
    slider.setEnabled(false);

    const auto mainPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto handlePosition = slider.getModulationHandleBounds().getCentre();
    slider.mouseDoubleClick(makeMouseEvent(
        slider, handlePosition, primaryButton, handlePosition, false, 2));
    slider.mouseDoubleClick(makeMouseEvent(
        slider, mainPosition, primaryButton, mainPosition, false, 2));

    CHECK(modulationResets == 0);
    CHECK(slider.getValue() == Catch::Approx(0.8));
}

TEST_CASE("Modulatable sliders freeze pointer ownership for main and modulation drags",
          "[modulatable-slider][ui][input][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();

    SECTION("main drag ignores another source and accepts a popup-modified release")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.setRange(0.0, 1.0);

        int starts = 0;
        int moves = 0;
        int ends = 0;
        int interactionEnds = 0;
        slider.onMainDragStart = [&](ModulatableSlider*) { ++starts; };
        slider.onMainDragMove = [&](ModulatableSlider*) { ++moves; };
        slider.onMainDragEnd = [&](ModulatableSlider*) { ++ends; };
        slider.onInteractionEnded = [&] { ++interactionEnds; };

        const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
        const auto dragPosition = downPosition + juce::Point<float> { 0.0f, -30.0f };
        slider.mouseDown(makeMouseEvent(slider, downPosition, primaryButton));
        REQUIRE(ModulatableSliderInteractionTestAccess::isMainGesture(slider));
        REQUIRE(starts == 1);

        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex() + 1);
        slider.mouseDrag(makeMouseEvent(
            slider, dragPosition, primaryButton, downPosition, true));
        slider.mouseUp(makeMouseEvent(slider, dragPosition, {}, downPosition, true));
        CHECK(slider.hasActiveInteraction());
        CHECK(moves == 0);
        CHECK(ends == 0);
        CHECK(interactionEnds == 0);

        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider, source.getType(), source.getIndex());
        slider.mouseUp(makeMouseEvent(
            slider,
            dragPosition,
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
            downPosition,
            true));

        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(ends == 1);
        CHECK(interactionEnds == 1);
        slider.dismissTransientInteraction();
        CHECK(ends == 1);
        CHECK(interactionEnds == 1);
    }

    SECTION("modulation drag ignores another source and dismisses once")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.setVisible(true);
        slider.isModulated = true;
        slider.lfoAmount = 0.2;

        int starts = 0;
        int moves = 0;
        int ends = 0;
        int amountChanges = 0;
        int interactionEnds = 0;
        slider.onModDragStart = [&](ModulatableSlider*) { ++starts; };
        slider.onModDragMove = [&](ModulatableSlider*) { ++moves; };
        slider.onModDragEnd = [&](ModulatableSlider*) { ++ends; };
        slider.onModAmountChanged = [&](double) { ++amountChanges; };
        slider.onInteractionEnded = [&] { ++interactionEnds; };

        const auto downPosition = slider.getModulationHandleBounds().getCentre();
        const auto dragPosition = downPosition + juce::Point<float> { 0.0f, -40.0f };
        slider.mouseDown(makeMouseEvent(slider, downPosition, primaryButton));
        REQUIRE(ModulatableSliderInteractionTestAccess::isModulationGesture(slider));
        REQUIRE(starts == 1);

        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex() + 1);
        slider.mouseDrag(makeMouseEvent(
            slider, dragPosition, primaryButton, downPosition, true));
        slider.mouseUp(makeMouseEvent(slider, dragPosition, {}, downPosition, true));
        CHECK(slider.lfoAmount == Catch::Approx(0.2));
        CHECK(moves == 0);
        CHECK(amountChanges == 0);
        CHECK(ends == 0);

        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider, source.getType(), source.getIndex());
        slider.mouseDrag(makeMouseEvent(
            slider, dragPosition, primaryButton, downPosition, true));
        CHECK(slider.lfoAmount == Catch::Approx(0.4));
        CHECK(moves == 1);
        CHECK(amountChanges == 1);

        slider.dismissTransientInteraction();
        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(ends == 1);
        CHECK(interactionEnds == 1);
        slider.dismissTransientInteraction();
        CHECK(ends == 1);
        CHECK(interactionEnds == 1);
    }
}

TEST_CASE("Assignment and popup sequences stay owned through their matching release",
          "[modulatable-slider][ui][input][gesture][assignment][popup]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();

    SECTION("assign mode consumes the rest of a double-click after clearing itself")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.setRange(0.0, 1.0);
        slider.setValue(0.8, juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, 0.25);
        int assignments = 0;
        int mainStarts = 0;
        slider.onMainDragStart = [&](ModulatableSlider*) { ++mainStarts; };
        slider.onClickInAssignMode = [&](const juce::String&)
        {
            ++assignments;
            slider.onClickInAssignMode = nullptr;
        };

        const auto position = slider.getLocalBounds().toFloat().getCentre();
        slider.mouseDown(makeMouseEvent(slider, position, primaryButton));
        REQUIRE(assignments == 1);
        slider.mouseUp(makeMouseEvent(slider, position));

        slider.mouseDown(makeMouseEvent(
            slider, position, primaryButton, position, false, 2));
        REQUIRE(ModulatableSliderInteractionTestAccess::isRejectedSequence(slider));
        slider.mouseUp(makeMouseEvent(
            slider, position, {}, position, false, 2));
        REQUIRE_FALSE(slider.hasActiveInteraction());

        // JUCE dispatches the double-click only after the second mouseUp.
        slider.mouseDoubleClick(makeMouseEvent(
            slider, position, primaryButton, position, false, 2));

        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(mainStarts == 0);
        CHECK(slider.getValue() == Catch::Approx(0.8));
    }

    SECTION("a popup release without its popup down cannot open a sequence")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        const auto position = slider.getLocalBounds().toFloat().getCentre();
        const auto popup = juce::ModifierKeys {
            juce::ModifierKeys::rightButtonModifier
        };

        slider.mouseUp(makeMouseEvent(slider, position, popup));
        CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));

        slider.mouseDown(makeMouseEvent(slider, position, popup));
        REQUIRE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider,
            source.getType() == juce::MouseInputSource::mouse
                ? juce::MouseInputSource::touch
                : juce::MouseInputSource::mouse,
            source.getIndex() + 1);
        slider.mouseUp(makeMouseEvent(slider, position, popup));
        CHECK(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));

        ModulatableSliderInteractionTestAccess::setTrackedPointerSource(
            slider, source.getType(), source.getIndex());
        slider.mouseUp(makeMouseEvent(slider, position, popup));
        CHECK_FALSE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));

        slider.mouseDown(makeMouseEvent(
            slider,
            position,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                 | juce::ModifierKeys::rightButtonModifier }));
        CHECK(ModulatableSliderInteractionTestAccess::isRejectedSequence(slider));
    }

    SECTION("popup assignment keeps the parameter that received mouseDown")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.parameterID = "clicked-band-target";
        const auto position = slider.getLocalBounds().toFloat().getCentre();
        const auto popup = juce::ModifierKeys {
            juce::ModifierKeys::rightButtonModifier
        };

        slider.mouseDown(makeMouseEvent(slider, position, popup));
        REQUIRE(ModulatableSliderInteractionTestAccess::isPopupSequence(slider));
        const auto frozenTarget =
            ModulatableSliderInteractionTestAccess::getPopupTargetParameterID(slider);
        slider.parameterID = "rebound-band-target";

        int assignedLfo = -1;
        juce::String assignedTarget;
        slider.onLfoAssignmentRequested = [&](int lfoIndex,
                                               const juce::String& target)
        {
            assignedLfo = lfoIndex;
            assignedTarget = target;
        };
        auto deliverResult =
            ModulatableSliderInteractionTestAccess::createAssignmentHandler(
                slider, frozenTarget);
        deliverResult(3);

        CHECK(assignedLfo == 2);
        CHECK(assignedTarget == "clicked-band-target");
        slider.dismissTransientInteraction();
    }
}

TEST_CASE("A fresh owned down closes the stale slider gesture before changing kind",
          "[modulatable-slider][ui][input][gesture][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.isModulated = true;

    int mainStarts = 0;
    int mainEnds = 0;
    int modStarts = 0;
    int modEnds = 0;
    int interactionEnds = 0;
    slider.onMainDragStart = [&](ModulatableSlider*) { ++mainStarts; };
    slider.onMainDragEnd = [&](ModulatableSlider*) { ++mainEnds; };
    slider.onModDragStart = [&](ModulatableSlider*) { ++modStarts; };
    slider.onModDragEnd = [&](ModulatableSlider*) { ++modEnds; };
    slider.onInteractionEnded = [&] { ++interactionEnds; };

    const auto mainPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto handlePosition = slider.getModulationHandleBounds().getCentre();
    slider.mouseDown(makeMouseEvent(slider, mainPosition, primaryButton));
    REQUIRE(ModulatableSliderInteractionTestAccess::isMainGesture(slider));

    slider.mouseDown(makeMouseEvent(slider, handlePosition, primaryButton));
    CHECK(mainStarts == 1);
    CHECK(mainEnds == 1);
    CHECK(modStarts == 1);
    CHECK(interactionEnds == 1);
    CHECK(ModulatableSliderInteractionTestAccess::isModulationGesture(slider));

    slider.dismissTransientInteraction();
    CHECK(modEnds == 1);
    CHECK(interactionEnds == 2);
}

TEST_CASE("Modulatable sliders recover omitted releases from hover events",
          "[modulatable-slider][ui][input][gesture][stale][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    enum class HoverEvent
    {
        move,
        enter,
        exit
    };

    for (const auto hoverEvent : { HoverEvent::move,
                                   HoverEvent::enter,
                                   HoverEvent::exit })
        for (const bool modulationHandle : { false, true })
            for (const auto releaseModifiers : {
                     juce::ModifierKeys {},
                     juce::ModifierKeys {
                         juce::ModifierKeys::rightButtonModifier } })
        {
            DYNAMIC_SECTION((modulationHandle ? "modulation" : "main")
                            << " via "
                            << (hoverEvent == HoverEvent::move ? "move"
                                : hoverEvent == HoverEvent::enter ? "enter"
                                                                  : "exit")
                            << (releaseModifiers.isRightButtonDown()
                                    ? " while right remains down"
                                    : " with no button down"))
            {
                ModulatableSlider slider;
                slider.setBounds(0, 0, 120, 120);
                slider.isModulated = true;
                int mainEnds = 0;
                int modulationEnds = 0;
                int interactionEnds = 0;
                slider.onMainDragEnd = [&](ModulatableSlider*) { ++mainEnds; };
                slider.onModDragEnd = [&](ModulatableSlider*) { ++modulationEnds; };
                slider.onInteractionEnded = [&] { ++interactionEnds; };

                const auto position = modulationHandle
                                          ? slider.getModulationHandleBounds().getCentre()
                                          : slider.getLocalBounds().toFloat().getCentre();
                slider.mouseDown(makeMouseEvent(slider, position, primaryButton));
                REQUIRE(slider.hasActiveInteraction());

                const auto releaseEvent =
                    makeMouseEvent(slider, position, releaseModifiers);
                if (hoverEvent == HoverEvent::move)
                    slider.mouseMove(releaseEvent);
                else if (hoverEvent == HoverEvent::enter)
                    slider.mouseEnter(releaseEvent);
                else
                    slider.mouseExit(releaseEvent);

                CHECK_FALSE(slider.hasActiveInteraction());
                CHECK(mainEnds == (modulationHandle ? 0 : 1));
                CHECK(modulationEnds == (modulationHandle ? 1 : 0));
                CHECK(interactionEnds == 1);

                slider.mouseUp(releaseEvent);
                CHECK(mainEnds == (modulationHandle ? 0 : 1));
                CHECK(modulationEnds == (modulationHandle ? 1 : 0));
                CHECK(interactionEnds == 1);
            }
        }
}

TEST_CASE("Modulation menu callbacks tolerate synchronous slider deletion",
          "[modulatable-slider][ui][popup][callback-safety]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto slider = std::make_unique<ModulatableSlider>();
    auto* const originalSlider = slider.get();
    ModulatableSlider* callbackTarget = nullptr;
    juce::String callbackParameterID;

    auto deliverResult =
        ModulatableSliderInteractionTestAccess::createModulationHandler(
            *slider, "menu-target");
    slider->onSetValueRequested =
        [&](ModulatableSlider* target, const juce::String& targetParameterID)
    {
        callbackTarget = target;
        callbackParameterID = targetParameterID;
        slider.reset();
    };

    deliverResult(static_cast<int>(
        ModulatableSlider::ModulationMenuCommand::setValue));

    CHECK(slider == nullptr);
    CHECK(callbackTarget == originalSlider);
    CHECK(callbackParameterID == "menu-target");
}

TEST_CASE("Slider context menu sessions reject stale and repeated results",
          "[modulatable-slider][ui][popup][lifecycle][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto exerciseBoundary = [](const std::function<void(ModulatableSlider&)>& boundary)
    {
        ModulatableSlider slider;
        slider.parameterID = "menu-target";
        slider.setBounds(0, 0, 120, 120);
        slider.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        slider.setVisible(true);
        const juce::ScopeGuard removePeer { [&]
        {
            slider.removeFromDesktop();
        } };

        int modulationCommandCount = 0;
        int assignmentCount = 0;
        slider.onModulationCleared = [&](const juce::String& target)
        {
            CHECK(target == "menu-target");
            ++modulationCommandCount;
        };
        slider.onLfoAssignmentRequested = [&](int lfoIndex,
                                               const juce::String& target)
        {
            CHECK(lfoIndex == 1);
            CHECK(target == "menu-target");
            ++assignmentCount;
        };

        auto staleModulationResult =
            ModulatableSliderInteractionTestAccess::createModulationHandler(
                slider, "menu-target");
        boundary(slider);
        staleModulationResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        CHECK(modulationCommandCount == 0);

        auto freshModulationResult =
            ModulatableSliderInteractionTestAccess::createModulationHandler(
                slider, "menu-target");
        freshModulationResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        freshModulationResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        CHECK(modulationCommandCount == 1);

        auto staleAssignmentResult =
            ModulatableSliderInteractionTestAccess::createAssignmentHandler(
                slider, "menu-target");
        boundary(slider);
        staleAssignmentResult(2);
        CHECK(assignmentCount == 0);

        auto freshAssignmentResult =
            ModulatableSliderInteractionTestAccess::createAssignmentHandler(
                slider, "menu-target");
        freshAssignmentResult(2);
        freshAssignmentResult(2);
        CHECK(assignmentCount == 1);
    };

    SECTION("explicit dismissal")
    {
        exerciseBoundary([](ModulatableSlider& slider)
        {
            slider.dismissTransientInteraction();
        });
    }

    SECTION("visibility ABA")
    {
        exerciseBoundary([](ModulatableSlider& slider)
        {
            slider.setVisible(false);
            slider.setVisible(true);
        });
    }

    SECTION("enablement ABA")
    {
        exerciseBoundary([](ModulatableSlider& slider)
        {
            slider.setEnabled(false);
            slider.setEnabled(true);
        });
    }

    SECTION("a replacement menu supersedes both menu kinds")
    {
        ModulatableSlider slider;
        slider.parameterID = "menu-target";
        int modulationCommandCount = 0;
        int assignmentCount = 0;
        slider.onModulationCleared = [&](const juce::String&)
        {
            ++modulationCommandCount;
        };
        slider.onLfoAssignmentRequested = [&](int,
                                               const juce::String&)
        {
            ++assignmentCount;
        };

        auto staleModulationResult =
            ModulatableSliderInteractionTestAccess::createModulationHandler(
                slider, "menu-target");
        auto currentAssignmentResult =
            ModulatableSliderInteractionTestAccess::createAssignmentHandler(
                slider, "menu-target");

        staleModulationResult(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        CHECK(modulationCommandCount == 0);
        currentAssignmentResult(3);
        currentAssignmentResult(3);
        CHECK(assignmentCount == 1);
    }

    SECTION("cancellation consumes its session")
    {
        ModulatableSlider slider;
        slider.parameterID = "menu-target";
        int commandCount = 0;
        slider.onModulationCleared = [&](const juce::String&)
        {
            ++commandCount;
        };
        auto result =
            ModulatableSliderInteractionTestAccess::createModulationHandler(
                slider, "menu-target");

        result(0);
        result(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        CHECK(commandCount == 0);
    }

    SECTION("a deleted slider makes its captured result inert")
    {
        int commandCount = 0;
        std::function<void(int)> result;
        {
            auto slider = std::make_unique<ModulatableSlider>();
            slider->parameterID = "menu-target";
            slider->onModulationCleared = [&](const juce::String&)
            {
                ++commandCount;
            };
            result =
                ModulatableSliderInteractionTestAccess::createModulationHandler(
                    *slider, "menu-target");
        }

        result(static_cast<int>(
            ModulatableSlider::ModulationMenuCommand::clearModulation));
        CHECK(commandCount == 0);
    }
}

TEST_CASE("Dismiss resets slider hover, editor and animation presentation exactly once",
          "[modulatable-slider][ui][hover][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ModulatableSlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.isModulated = true;

    int hoverStarts = 0;
    int hoverEnds = 0;
    slider.onHoverStart = [&](ModulatableSlider*) { ++hoverStarts; };
    slider.onHoverEnd = [&](ModulatableSlider*) { ++hoverEnds; };

    const auto handlePosition = slider.getModulationHandleBounds().getCentre();
    slider.mouseEnter(makeMouseEvent(slider, handlePosition));
    REQUIRE(slider.isModHandleMouseOver);
    REQUIRE(hoverStarts == 1);
    REQUIRE(slider.getTextBoxPosition() == juce::Slider::TextBoxAbove);

    slider.mouseExit(makeMouseEvent(slider, handlePosition));
    REQUIRE(slider.isTimerRunning());
    // Re-enter so dismiss has an active hover callback to close.
    const auto relaidOutHandlePosition = slider.getModulationHandleBounds().getCentre();
    slider.mouseEnter(makeMouseEvent(slider, relaidOutHandlePosition));
    REQUIRE(hoverStarts == 2);
    ModulatableSliderInteractionTestAccess::primeAnimations(slider);

    slider.dismissTransientInteraction();
    CHECK_FALSE(slider.isTimerRunning());
    CHECK_FALSE(slider.isModHandleMouseOver);
    CHECK(slider.getHoverAnimation() == 0.0f);
    CHECK(slider.getPressAnimation() == 0.0f);
    CHECK(slider.getTextBoxPosition() == juce::Slider::NoTextBox);
    CHECK(hoverEnds == 2);

    bool titleVisible = false;
    for (auto* child : slider.getChildren())
        if (auto* title = dynamic_cast<juce::Label*>(child))
            titleVisible = title->isVisible();
    CHECK(titleVisible);

    slider.dismissTransientInteraction();
    CHECK(hoverEnds == 2);
}

TEST_CASE("Slider lifecycle boundaries finish gestures and tolerate synchronous deletion",
          "[modulatable-slider][ui][input][lifecycle][callback-safety]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    SECTION("disable and direct visibility changes close each accepted kind")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.setVisible(true);
        slider.isModulated = true;
        int mainEnds = 0;
        int modEnds = 0;
        int interactionEnds = 0;
        slider.onMainDragEnd = [&](ModulatableSlider*) { ++mainEnds; };
        slider.onModDragEnd = [&](ModulatableSlider*) { ++modEnds; };
        slider.onInteractionEnded = [&] { ++interactionEnds; };

        const auto mainPosition = slider.getLocalBounds().toFloat().getCentre();
        slider.mouseDown(makeMouseEvent(slider, mainPosition, primaryButton));
        REQUIRE(slider.hasActiveInteraction());
        slider.setEnabled(false);
        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(mainEnds == 1);
        CHECK(interactionEnds == 1);

        slider.setEnabled(true);
        slider.mouseUp(makeMouseEvent(slider, mainPosition));
        CHECK(mainEnds == 1);
        CHECK(interactionEnds == 1);

        const auto handlePosition = slider.getModulationHandleBounds().getCentre();
        slider.mouseDown(makeMouseEvent(slider, handlePosition, primaryButton));
        REQUIRE(slider.hasActiveInteraction());
        slider.setVisible(false);
        CHECK_FALSE(slider.hasActiveInteraction());
        CHECK(modEnds == 1);
        CHECK(interactionEnds == 2);
    }

    SECTION("main start may synchronously destroy its slider")
    {
        int mainEnds = 0;
        auto slider = std::make_unique<ModulatableSlider>();
        slider->setBounds(0, 0, 120, 120);
        slider->onMainDragEnd = [&](ModulatableSlider*) { ++mainEnds; };
        slider->onMainDragStart = [&](ModulatableSlider*) { slider.reset(); };
        auto* rawSlider = slider.get();
        juce::Component::SafePointer<ModulatableSlider> safeSlider(rawSlider);
        const auto position = rawSlider->getLocalBounds().toFloat().getCentre();

        rawSlider->mouseDown(makeMouseEvent(*rawSlider, position, primaryButton));
        CHECK(! safeSlider);
        CHECK(mainEnds == 1);
    }

    SECTION("modulation value callback may synchronously destroy its slider")
    {
        int modEnds = 0;
        auto slider = std::make_unique<ModulatableSlider>();
        slider->setBounds(0, 0, 120, 120);
        slider->isModulated = true;
        slider->onModDragEnd = [&](ModulatableSlider*) { ++modEnds; };
        slider->onModAmountChanged = [&](double) { slider.reset(); };
        auto* rawSlider = slider.get();
        juce::Component::SafePointer<ModulatableSlider> safeSlider(rawSlider);
        const auto downPosition = rawSlider->getModulationHandleBounds().getCentre();
        const auto dragPosition = downPosition + juce::Point<float> { 0.0f, -20.0f };

        rawSlider->mouseDown(makeMouseEvent(
            *rawSlider, downPosition, primaryButton));
        rawSlider->mouseDrag(makeMouseEvent(
            *rawSlider, dragPosition, primaryButton, downPosition, true));
        CHECK(! safeSlider);
        CHECK(modEnds == 1);
    }

    SECTION("value label popup restart may synchronously destroy its slider")
    {
        auto slider = std::make_unique<ModulatableSlider>();
        slider->setBounds(0, 0, 120, 120);
        slider->mouseEnter(makeMouseEvent(
            *slider, slider->getLocalBounds().toFloat().getCentre()));
        auto* valueLabel =
            ModulatableSliderInteractionTestAccess::getForwardedValueLabel(
                *slider);
        REQUIRE(valueLabel != nullptr);

        const auto mainPosition =
            slider->getLocalBounds().toFloat().getCentre();
        slider->mouseDown(makeMouseEvent(
            *slider, mainPosition, primaryButton));
        REQUIRE(slider->hasActiveInteraction());
        slider->onMainDragEnd =
            [&](ModulatableSlider*) { slider.reset(); };

        auto* const rawSlider = slider.get();
        const auto labelCentre =
            valueLabel->getLocalBounds().toFloat().getCentre();
        ModulatableSliderInteractionTestAccess::forwardValueLabelMouseDown(
            *rawSlider,
            makeMouseEvent(
                *valueLabel,
                labelCentre,
                juce::ModifierKeys {
                    juce::ModifierKeys::rightButtonModifier }));
        CHECK(slider == nullptr);
    }
}

TEST_CASE("Slider cleanup balances APVTS gestures and releases deferred band focus",
          "[modulatable-slider][ui][gesture][attachment][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;

    SECTION("direct slider dismissal")
    {
        ModulatableSlider slider;
        slider.setBounds(0, 0, 120, 120);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        juce::AudioProcessorValueTreeState::SliderAttachment attachment(
            processor.treeState, MIX_ID, slider);
        auto* parameter = processor.treeState.getParameter(MIX_ID);
        REQUIRE(parameter != nullptr);
        ParameterGestureCapture host(processor, parameter->getParameterIndex());

        const auto position = slider.getLocalBounds().toFloat().getCentre();
        slider.mouseDown(makeMouseEvent(slider, position, primaryButton));
        REQUIRE(host.beginCount == 1);
        REQUIRE(host.endCount == 0);

        slider.dismissTransientInteraction();
        checkBalanced(host);
        slider.dismissTransientInteraction();
        checkBalanced(host);
    }

    SECTION("BandPanel applies a focus change deferred by the abandoned drag")
    {
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setBounds(0, 0, 1000, 300);
        auto* drive = panel.getDriveKnob();
        REQUIRE(drive != nullptr);
        const auto originalParameterID = drive->getParamID();
        auto* originalParameter = processor.treeState.getParameter(originalParameterID);
        REQUIRE(originalParameter != nullptr);
        ParameterGestureCapture host(
            processor, originalParameter->getParameterIndex());

        const auto position = drive->getLocalBounds().toFloat().getCentre();
        drive->mouseDown(makeMouseEvent(*drive, position, primaryButton));
        REQUIRE(host.beginCount == 1);
        panel.setFocusBandNum(1);
        REQUIRE(panel.getFocusBandNum() == 0);

        drive->dismissTransientInteraction();
        CHECK(panel.getFocusBandNum() == 1);
        CHECK(drive->getParamID()
              == ParameterIDAndName::getIDString(DRIVE_ID, 1));
        checkBalanced(host);
    }

    SECTION("BandPanel discards uncommitted text before rebinding")
    {
        FireLookAndFeel lookAndFeel;
        BandPanel panel(processor, {}, {}, {}, {}, {});
        panel.setLookAndFeel(&lookAndFeel);
        panel.setBounds(0, 0, 1000, 300);
        panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        panel.setVisible(true);
        auto* drive = panel.getDriveKnob();
        auto* band0Parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(DRIVE_ID, 0));
        auto* band1Parameter = processor.treeState.getParameter(
            ParameterIDAndName::getIDString(DRIVE_ID, 1));
        REQUIRE(drive != nullptr);
        REQUIRE(band0Parameter != nullptr);
        REQUIRE(band1Parameter != nullptr);
        band0Parameter->setValueNotifyingHost(0.0f);
        band1Parameter->setValueNotifyingHost(0.0f);

        const auto position = drive->getLocalBounds().toFloat().getCentre();
        drive->mouseEnter(makeMouseEvent(*drive, position));
        auto* valueLabel =
            ModulatableSliderInteractionTestAccess::getForwardedValueLabel(
                *drive);
        REQUIRE(valueLabel != nullptr);
        valueLabel->showEditor();
        auto* textEditor = valueLabel->getCurrentTextEditor();
        REQUIRE(textEditor != nullptr);
        textEditor->setText("50", false);
        juce::Component::SafePointer<juce::TextEditor> safeEditor(textEditor);

        panel.setFocusBandNum(1);

        CHECK(panel.getFocusBandNum() == 1);
        CHECK(drive->getParamID()
              == ParameterIDAndName::getIDString(DRIVE_ID, 1));
        CHECK(safeEditor == nullptr);
        CHECK(band0Parameter->getValue() == 0.0f);
        CHECK(band1Parameter->getValue() == 0.0f);
        panel.removeFromDesktop();
        panel.setLookAndFeel(nullptr);
    }

    SECTION("PanelBase teardown runs before its SliderAttachments")
    {
        auto panel = std::make_unique<GlobalPanel>(
            processor, nullptr, nullptr, nullptr, nullptr, nullptr);
        auto* slider = panel->getModulatableSliders().front();
        REQUIRE(slider != nullptr);
        auto* parameter = processor.treeState.getParameter(slider->getParamID());
        REQUIRE(parameter != nullptr);
        ParameterGestureCapture host(processor, parameter->getParameterIndex());

        const auto position = slider->getLocalBounds().toFloat().getCentre();
        slider->mouseDown(makeMouseEvent(*slider, position, primaryButton));
        REQUIRE(host.beginCount == 1);
        REQUIRE(host.endCount == 0);

        panel.reset();
        checkBalanced(host);
    }
}

TEST_CASE("Editor hide and panel transitions dismiss all modulatable slider gestures",
          "[modulatable-slider][ui][editor][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    auto* panel = findDescendant<BandPanel>(*editor);
    REQUIRE(panel != nullptr);
    auto* drive = panel->getDriveKnob();
    REQUIRE(drive != nullptr);
    auto* parameter = processor.treeState.getParameter(drive->getParamID());
    REQUIRE(parameter != nullptr);
    ParameterGestureCapture host(processor, parameter->getParameterIndex());
    const auto position = drive->getLocalBounds().toFloat().getCentre();

    drive->mouseDown(makeMouseEvent(*drive, position, primaryButton));
    REQUIRE(host.beginCount == 1);
    auto* masterTab = findButtonWithText(*editor, "MASTER LAB");
    REQUIRE(masterTab != nullptr);
    performPrimaryClick(*masterTab);
    CHECK_FALSE(drive->hasActiveInteraction());
    checkBalanced(host);

    auto* bandTab = findButtonWithText(*editor, "BAND LAB");
    REQUIRE(bandTab != nullptr);
    performPrimaryClick(*bandTab);
    REQUIRE(panel->isVisible());
    drive->mouseDown(makeMouseEvent(*drive, position, primaryButton));
    REQUIRE(host.beginCount == 2);

    auto* zoom = dynamic_cast<juce::Button*>(editor->findChildWithID("zoom"));
    REQUIRE(zoom != nullptr);
    performPrimaryClick(*zoom);
    CHECK_FALSE(drive->hasActiveInteraction());
    checkBalanced(host, 2);

    // Restore the normal workspace, then verify the host-hide boundary too.
    performPrimaryClick(*zoom);
    drive->mouseDown(makeMouseEvent(*drive, position, primaryButton));
    REQUIRE(host.beginCount == 3);
    editor->setVisible(false);
    CHECK_FALSE(drive->hasActiveInteraction());
    checkBalanced(host, 3);

    editor->removeFromDesktop();
}

TEST_CASE("Panel gesture cleanup survives synchronous owner deletion",
          "[modulatable-slider][ui][gesture][lifecycle][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;

    SECTION("BandPanel")
    {
        auto panel = std::make_unique<BandPanel>(
            processor, nullptr, nullptr, nullptr, nullptr, nullptr);
        panel->setBounds(0, 0, 1000, 300);
        auto* slider = panel->getDriveKnob();
        REQUIRE(slider != nullptr);
        DeleteOwnerOnGestureEnd<BandPanel> deleteOnEnd(processor, panel);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        auto* rawPanel = panel.get();
        rawPanel->dismissTransientInteraction();

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("GlobalPanel")
    {
        auto panel = std::make_unique<GlobalPanel>(
            processor, nullptr, nullptr, nullptr, nullptr, nullptr);
        panel->setBounds(0, 0, 1000, 300);
        auto* slider = panel->getModulatableSliders().front();
        REQUIRE(slider != nullptr);
        DeleteOwnerOnGestureEnd<GlobalPanel> deleteOnEnd(processor, panel);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        auto* rawPanel = panel.get();
        rawPanel->dismissTransientInteraction();

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("LfoPanel")
    {
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->setBounds(0, 0, 1000, 500);
        auto* slider = findSliderAttachedToLabel(*panel, "Rate");
        REQUIRE(slider != nullptr);
        DeleteOwnerOnGestureEnd<LfoPanel> deleteOnEnd(processor, panel);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        auto* rawPanel = panel.get();
        rawPanel->dismissTransientInteraction();

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("LfoPanel selection")
    {
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->setBounds(0, 0, 1000, 500);
        auto* slider = findSliderAttachedToLabel(*panel, "Rate");
        auto* button = findButtonWithText(*panel, "LFO 2");
        REQUIRE(slider != nullptr);
        REQUIRE(button != nullptr);
        DeleteOwnerOnGestureEnd<LfoPanel> deleteOnEnd(processor, panel);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        performPrimaryClick(*button);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }

    SECTION("LfoPanel visibility boundary")
    {
        auto panel = std::make_unique<LfoPanel>(processor);
        panel->setBounds(0, 0, 1000, 500);
        panel->setVisible(true);
        auto* slider = findSliderAttachedToLabel(*panel, "Rate");
        REQUIRE(slider != nullptr);
        DeleteOwnerOnGestureEnd<LfoPanel> deleteOnEnd(processor, panel);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        auto* rawPanel = panel.get();
        rawPanel->setVisible(false);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(panel == nullptr);
    }
}

TEST_CASE("Editor gesture cleanup survives synchronous owner deletion",
          "[modulatable-slider][ui][editor][gesture][lifecycle][self-delete][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireAudioProcessor processor;
    processor.hasUpdateCheckBeenPerformed = true;
    auto editor = std::make_unique<FireAudioProcessorEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    auto* panel = findDescendant<BandPanel>(*editor);
    REQUIRE(panel != nullptr);
    auto* slider = panel->getDriveKnob();
    REQUIRE(slider != nullptr);
    DeleteOwnerOnGestureEnd<FireAudioProcessorEditor> deleteOnEnd(
        processor, editor);

    SECTION("workspace transition")
    {
        auto* button = findButtonWithText(*editor, "MASTER LAB");
        REQUIRE(button != nullptr);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        performPrimaryClick(*button);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(editor == nullptr);
    }

    SECTION("LFO workspace transition")
    {
        auto* lfoButton = findButtonWithText(*editor, "MOD FORGE");
        auto* masterButton = findButtonWithText(*editor, "MASTER LAB");
        auto* lfoPanel = findDescendant<LfoPanel>(*editor);
        REQUIRE(lfoButton != nullptr);
        REQUIRE(masterButton != nullptr);
        REQUIRE(lfoPanel != nullptr);

        performPrimaryClick(*lfoButton);
        auto* rateSlider = findSliderAttachedToLabel(*lfoPanel, "Rate");
        REQUIRE(rateSlider != nullptr);
        beginPrimaryGesture(*rateSlider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        performPrimaryClick(*masterButton);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(editor == nullptr);
    }

    SECTION("zoom transition")
    {
        auto* button = dynamic_cast<juce::Button*>(
            editor->findChildWithID("zoom"));
        REQUIRE(button != nullptr);
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        performPrimaryClick(*button);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(editor == nullptr);
    }

    SECTION("visibility boundary")
    {
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        editor->setVisible(false);

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(editor == nullptr);
    }

    SECTION("hidden timer cleanup")
    {
        editor->removeFromDesktop();
        beginPrimaryGesture(*slider);
        REQUIRE(deleteOnEnd.beginCount == 1);

        editor->timerCallback();

        CHECK(deleteOnEnd.endCount == 1);
        CHECK(deleteOnEnd.callbackCompleted);
        CHECK(editor == nullptr);
    }
}
