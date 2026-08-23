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
