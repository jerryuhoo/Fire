#include <GUI/LookAndFeel.h>
#include <GUI/PrimarySlider.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

struct PrimarySliderTestAccess
{
    static void setTrackedPointerSource(
        PrimarySlider& slider,
        juce::MouseInputSource::InputSourceType type,
        int index) noexcept
    {
        slider.pointerSourceType = type;
        slider.pointerSourceIndex = index;
    }

    static void setAnimationTargets(PrimarySlider& slider,
                                    float hover,
                                    float press,
                                    float focus,
                                    float disabled) noexcept
    {
        slider.hoverAnimation.setTarget(hover);
        slider.pressAnimation.setTarget(press);
        slider.focusAnimation.setTarget(focus);
        slider.disabledAnimation.setTarget(disabled);
    }

    static bool advanceAnimation(PrimarySlider& slider,
                                 float deltaSeconds) noexcept
    {
        return slider.advanceAnimation(deltaSeconds);
    }

    static void updateAnimationTargets(PrimarySlider& slider) noexcept
    {
        slider.updateAnimationTargets();
    }

    static bool isKeyboardFocusVisible(
        const PrimarySlider& slider) noexcept
    {
        return slider.focusModality.isKeyboardVisible();
    }

    static bool isRecoveryTimerRunning(const PrimarySlider& slider) noexcept
    {
        return slider.isTimerRunning();
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
    const auto now = juce::Time::getCurrentTime();
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
             now,
             mouseDownPosition,
             now,
             clickCount,
             wasDragged };
}

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

class SliderInteractionCapture final : public juce::Slider::Listener
{
public:
    void sliderValueChanged(juce::Slider*) override { ++valueChanges; }
    void sliderDragStarted(juce::Slider*) override { ++dragStarts; }
    void sliderDragEnded(juce::Slider*) override { ++dragEnds; }

    int valueChanges = 0;
    int dragStarts = 0;
    int dragEnds = 0;
};

class DeletingSliderListener final : public juce::Slider::Listener
{
public:
    enum class DeleteOn
    {
        dragStarted,
        valueChanged,
        dragEnded
    };

    DeletingSliderListener(std::unique_ptr<PrimarySlider>& ownerToDeleteIn,
                           DeleteOn deletionPointToUse,
                           SliderInteractionCapture& captureToUse)
        : ownerToDelete(ownerToDeleteIn),
          deletionPoint(deletionPointToUse),
          capture(captureToUse)
    {
    }

    void sliderValueChanged(juce::Slider*) override
    {
        ++capture.valueChanges;
        if (deletionPoint == DeleteOn::valueChanged)
            ownerToDelete.reset();
    }

    void sliderDragStarted(juce::Slider*) override
    {
        ++capture.dragStarts;
        if (deletionPoint == DeleteOn::dragStarted)
            ownerToDelete.reset();
    }

    void sliderDragEnded(juce::Slider*) override
    {
        ++capture.dragEnds;
        if (deletionPoint == DeleteOn::dragEnded)
            ownerToDelete.reset();
    }

private:
    std::unique_ptr<PrimarySlider>& ownerToDelete;
    DeleteOn deletionPoint;
    SliderInteractionCapture& capture;
};

juce::Button* findSliderButton(juce::Slider& slider,
                               const juce::String& componentID)
{
    for (auto* child : slider.getChildren())
        if (auto* button = dynamic_cast<juce::Button*>(child);
            button != nullptr && button->getComponentID() == componentID)
            return button;

    return nullptr;
}
} // namespace

TEST_CASE("PrimarySlider interaction presentation is continuous and hidden-idle",
          "[primary-slider][ui][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;

    PrimarySliderTestAccess::setAnimationTargets(slider, 1.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(PrimarySliderTestAccess::advanceAnimation(slider, 1.0f / 60.0f));
    CHECK(slider.getHoverAnimation() > 0.0f);
    CHECK(slider.getHoverAnimation() < 1.0f);
    CHECK(slider.getPressAnimation() > slider.getHoverAnimation());
    CHECK(slider.getFocusAnimation() > 0.0f);
    CHECK(slider.getDisabledAnimation() > 0.0f);

    // An off-screen slider (including one hidden by any ancestor) must snap
    // to an idle state rather than leaving a repaint timer alive.
    PrimarySliderTestAccess::updateAnimationTargets(slider);
    CHECK(slider.getHoverAnimation() == Catch::Approx(0.0f));
    CHECK(slider.getPressAnimation() == Catch::Approx(0.0f));
    CHECK(slider.getFocusAnimation() == Catch::Approx(0.0f));
    CHECK(slider.getDisabledAnimation() == Catch::Approx(0.0f));

    slider.setEnabled(false);
    CHECK(slider.getDisabledAnimation() == Catch::Approx(1.0f));
}

TEST_CASE("PrimarySlider focus presentation follows keyboard modality",
          "[primary-slider][ui][input][focus][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;
    slider.setBounds(0, 0, 180, 32);
    slider.setRange(0.0, 1.0, 0.01);
    slider.setValue(0.5, juce::dontSendNotification);
    slider.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    slider.setVisible(true);
    REQUIRE(slider.isShowing());
    REQUIRE(slider.getWantsKeyboardFocus());
    REQUIRE(slider.getMouseClickGrabsKeyboardFocus());

    slider.focusGained(
        juce::Component::FocusChangeType::focusChangedDirectly);
    REQUIRE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));

    slider.focusGained(
        juce::Component::FocusChangeType::focusChangedByMouseClick);
    CHECK_FALSE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));
    slider.focusLost(
        juce::Component::FocusChangeType::focusChangedDirectly);
    CHECK_FALSE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));
    slider.focusGained(
        juce::Component::FocusChangeType::focusChangedDirectly);
    CHECK_FALSE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));

    slider.focusGained(
        juce::Component::FocusChangeType::focusChangedByTabKey);
    REQUIRE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));

    const auto centre = slider.getLocalBounds().toFloat().getCentre();
    slider.mouseDown(makeMouseEvent(
        slider,
        centre,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    CHECK_FALSE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));
    slider.mouseUp(makeMouseEvent(slider, centre));

    slider.focusLost(
        juce::Component::FocusChangeType::focusChangedDirectly);
    CHECK_FALSE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));

    // Directly calling focus callbacks above does not assign JUCE's actual
    // keyboard focus. Establish the delivery precondition instead of relying
    // on whichever temporary native window happened to become active.
    slider.grabKeyboardFocus();
    REQUIRE(slider.hasKeyboardFocus(true));
    slider.keyPressed(juce::KeyPress { juce::KeyPress::rightKey });
    REQUIRE(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));
    slider.focusLost(
        juce::Component::FocusChangeType::focusChangedDirectly);
    slider.focusGained(
        juce::Component::FocusChangeType::focusChangedDirectly);
    CHECK(PrimarySliderTestAccess::isKeyboardFocusVisible(slider));
}

TEST_CASE("PrimarySlider preserves accessible value semantics while showing",
          "[primary-slider][ui][input][accessibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;
    slider.setBounds(0, 0, 120, 30);
    slider.setRange(0.0, 1.0, 0.1);
    slider.setValue(0.2, juce::dontSendNotification);
    slider.setTooltip("Adjust the test value");
    slider.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    slider.setVisible(true);
    REQUIRE(slider.isShowing());

    SliderInteractionCapture capture;
    slider.addListener(&capture);
    auto* handler = slider.getAccessibilityHandler();
    REQUIRE(handler != nullptr);
    CHECK(handler->getRole() == juce::AccessibilityRole::slider);
    CHECK(handler->getHelp() == slider.getTooltip());
    auto* value = handler->getValueInterface();
    REQUIRE(value != nullptr);
    CHECK_FALSE(value->isReadOnly());
    CHECK(value->getCurrentValue() == Catch::Approx(0.2));
    CHECK(value->getRange().getMinimumValue() == Catch::Approx(0.0));
    CHECK(value->getRange().getMaximumValue() == Catch::Approx(1.0));
    CHECK(value->getRange().getInterval() == Catch::Approx(0.1));

    value->setValue(0.8);
    CHECK(slider.getValue() == Catch::Approx(0.8));
    CHECK(capture.valueChanges == 1);
    CHECK(capture.dragStarts == 1);
    CHECK(capture.dragEnds == 1);

    value->setValueAsString("0.4");
    CHECK(slider.getValue() == Catch::Approx(0.4));
    CHECK(capture.valueChanges == 2);
    CHECK(capture.dragStarts == 2);
    CHECK(capture.dragEnds == 2);

    slider.removeFromDesktop();
}

TEST_CASE("PrimarySlider accessible writes stop when callbacks end the visible session",
          "[primary-slider][ui][input][accessibility][lifecycle][callback-lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (int entry = 0; entry < 3; ++entry)
    {
        for (int boundary = 0; boundary < 3; ++boundary)
        {
            DYNAMIC_SECTION("entry " << entry << ", lifecycle boundary " << boundary)
            {
                juce::Component desktopHost;
                desktopHost.setBounds(0, 0, 160, 60);
                PrimarySlider slider;
                slider.setBounds(0, 0, 120, 30);
                slider.setRange(0.0, 1.0, 0.1);
                slider.setValue(0.2, juce::dontSendNotification);
                desktopHost.addAndMakeVisible(slider);
                desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
                desktopHost.setVisible(true);
                REQUIRE(slider.isShowing());

                SliderInteractionCapture capture;
                slider.addListener(&capture);
                const auto endVisibleSession = [&]
                {
                    if (boundary == 0)
                        desktopHost.setVisible(false);
                    else if (boundary == 1)
                        desktopHost.setEnabled(false);
                    else
                        desktopHost.removeFromDesktop();
                };

                if (entry == 2)
                    slider.valueFromTextFunction = [&](const juce::String&)
                    {
                        endVisibleSession();
                        return 0.8;
                    };
                else
                    slider.onDragStart = endVisibleSession;

                auto* handler = slider.getAccessibilityHandler();
                REQUIRE(handler != nullptr);
                auto* value = handler->getValueInterface();
                REQUIRE(value != nullptr);
                if (entry == 0)
                    value->setValue(0.8);
                else
                    value->setValueAsString("0.8");

                // A host may hide its editor from beginGesture without
                // deleting it. End that gesture, but do not write after the
                // callback has ended the session. Text conversion happens
                // before beginGesture, so it must not open a gesture at all.
                CHECK(slider.getValue() == Catch::Approx(0.2));
                CHECK(capture.valueChanges == 0);
                CHECK(capture.dragStarts == (entry == 2 ? 0 : 1));
                CHECK(capture.dragEnds == capture.dragStarts);

                slider.onDragStart = nullptr;
                slider.valueFromTextFunction = nullptr;
                if (boundary == 0)
                    desktopHost.setVisible(true);
                else if (boundary == 1)
                    desktopHost.setEnabled(true);
                else
                    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
                REQUIRE(slider.isShowing());
                handler = slider.getAccessibilityHandler();
                REQUIRE(handler != nullptr);
                value = handler->getValueInterface();
                REQUIRE(value != nullptr);
                value->setValue(0.4);
                CHECK(slider.getValue() == Catch::Approx(0.4));
                CHECK(capture.valueChanges == 1);
                CHECK(capture.dragStarts == (entry == 2 ? 1 : 2));
                CHECK(capture.dragEnds == capture.dragStarts);
                slider.removeListener(&capture);
            }
        }
    }
}

TEST_CASE("PrimarySlider accessible writes survive synchronous Slider deletion",
          "[primary-slider][ui][input][accessibility][lifecycle][deletion]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto deletionPoint : {
             DeletingSliderListener::DeleteOn::dragStarted,
             DeletingSliderListener::DeleteOn::valueChanged,
             DeletingSliderListener::DeleteOn::dragEnded })
    {
        const auto* const sectionName =
            deletionPoint == DeletingSliderListener::DeleteOn::dragStarted
                ? "deleted by drag-start listener"
            : deletionPoint == DeletingSliderListener::DeleteOn::valueChanged
                ? "deleted by value listener"
                : "deleted by drag-end listener";
        DYNAMIC_SECTION(sectionName)
        {
            juce::Component desktopHost;
            desktopHost.setBounds(0, 0, 160, 60);
            auto slider = std::make_unique<PrimarySlider>();
            slider->setBounds(0, 0, 120, 30);
            slider->setRange(0.0, 1.0, 0.1);
            slider->setValue(0.2, juce::dontSendNotification);
            desktopHost.addAndMakeVisible(*slider);
            desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            desktopHost.setVisible(true);
            REQUIRE(slider->isShowing());

            SliderInteractionCapture capture;
            DeletingSliderListener deletingListener(slider,
                                                     deletionPoint,
                                                     capture);
            slider->addListener(&deletingListener);
            auto* const handler = slider->getAccessibilityHandler();
            REQUIRE(handler != nullptr);
            auto* const value = handler->getValueInterface();
            REQUIRE(value != nullptr);

            // The interface and handler are destroyed during this call. The
            // call must return without touching either one again.
            value->setValue(0.8);

            CHECK(slider == nullptr);
            CHECK(capture.dragStarts == 1);
            CHECK(capture.valueChanges
                  == (deletionPoint
                              == DeletingSliderListener::DeleteOn::dragStarted
                          ? 0
                          : 1));
            CHECK(capture.dragEnds
                  == (deletionPoint
                              == DeletingSliderListener::DeleteOn::dragEnded
                          ? 1
                          : 0));

            desktopHost.removeFromDesktop();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        }
    }
}

TEST_CASE("PrimarySlider accessible text conversion survives Slider deletion",
          "[primary-slider][ui][input][accessibility][lifecycle][deletion]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component desktopHost;
    desktopHost.setBounds(0, 0, 160, 60);
    auto slider = std::make_unique<PrimarySlider>();
    slider->setBounds(0, 0, 120, 30);
    slider->setRange(0.0, 1.0, 0.1);
    desktopHost.addAndMakeVisible(*slider);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(slider->isShowing());

    slider->valueFromTextFunction = [&slider](const juce::String&)
    {
        slider.reset();
        return 0.8;
    };
    auto* const handler = slider->getAccessibilityHandler();
    REQUIRE(handler != nullptr);
    auto* const value = handler->getValueInterface();
    REQUIRE(value != nullptr);

    value->setValueAsString("0.8");

    CHECK(slider == nullptr);
    desktopHost.removeFromDesktop();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

TEST_CASE("Cached PrimarySlider accessibility rejects stale value writes",
          "[primary-slider][ui][input][accessibility][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (int boundaryIndex = 0; boundaryIndex < 3; ++boundaryIndex)
    {
        const auto* boundaryName = boundaryIndex == 0 ? "hidden"
                                 : boundaryIndex == 1 ? "disabled"
                                                      : "peer detached";
        DYNAMIC_SECTION(boundaryName)
        {
            PrimarySlider slider;
            slider.setBounds(0, 0, 120, 30);
            slider.setRange(0.0, 1.0, 0.1);
            slider.setValue(0.2, juce::dontSendNotification);
            slider.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            slider.setVisible(true);
            REQUIRE(slider.isShowing());

            SliderInteractionCapture capture;
            slider.addListener(&capture);
            auto* handler = slider.getAccessibilityHandler();
            REQUIRE(handler != nullptr);
            auto* value = handler->getValueInterface();
            REQUIRE(value != nullptr);

            if (boundaryIndex == 0)
                slider.setVisible(false);
            else if (boundaryIndex == 1)
                slider.setEnabled(false);
            else
                slider.removeFromDesktop();

            if (boundaryIndex == 1)
            {
                REQUIRE_FALSE(slider.isEnabled());
                REQUIRE(slider.isShowing());
            }
            else
            {
                REQUIRE_FALSE(slider.isShowing());
            }

            value->setValue(0.8);
            value->setValueAsString("0.9");

            CHECK(slider.getValue() == Catch::Approx(0.2));
            CHECK(capture.valueChanges == 0);
            CHECK(capture.dragStarts == 0);
            CHECK(capture.dragEnds == 0);
            slider.removeFromDesktop();
        }
    }
}

TEST_CASE("PrimarySlider rejects auxiliary drags and double-clicks",
          "[primary-slider][ui][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.8, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, 0.25);
    SliderInteractionCapture capture;
    slider.addListener(&capture);

    const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto dragPosition = downPosition + juce::Point<float> { 20.0f, -25.0f };
    for (const auto modifiers : rejectedPointerModifiers())
    {
        CAPTURE(modifiers.getRawFlags());
        slider.mouseDown(makeMouseEvent(slider, downPosition, modifiers));
        slider.mouseDrag(makeMouseEvent(
            slider, dragPosition, modifiers, downPosition, true));
        slider.mouseUp(makeMouseEvent(
            slider, dragPosition, {}, downPosition, true));
        slider.mouseDoubleClick(makeMouseEvent(
            slider, downPosition, modifiers, downPosition, false, 2));

        CHECK_FALSE(slider.hasActivePointerGesture());
        CHECK(slider.getValue() == Catch::Approx(0.8));
    }

    CHECK(capture.dragStarts == 0);
    CHECK(capture.dragEnds == 0);
    CHECK(capture.valueChanges == 0);

    const juce::ModifierKeys primary {
        juce::ModifierKeys::leftButtonModifier
    };
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragStarts == 1);
    slider.mouseUp(makeMouseEvent(slider, downPosition));
    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);

    slider.setValue(0.8, juce::dontSendNotification);
    slider.mouseDown(makeMouseEvent(
        slider, downPosition, primary, downPosition, false, 2));
    slider.mouseUp(makeMouseEvent(
        slider, downPosition, {}, downPosition, false, 2));
    slider.mouseDoubleClick(makeMouseEvent(
        slider, downPosition, primary, downPosition, false, 2));
    CHECK(slider.getValue() == Catch::Approx(0.25));
}

TEST_CASE("PrimarySlider freezes source ownership and closes a stale gesture once",
          "[primary-slider][ui][input][source][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5, juce::dontSendNotification);
    SliderInteractionCapture capture;
    slider.addListener(&capture);

    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto dragPosition = downPosition + juce::Point<float> { 30.0f, -20.0f };
    const juce::ModifierKeys primary {
        juce::ModifierKeys::leftButtonModifier
    };
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragStarts == 1);

    PrimarySliderTestAccess::setTrackedPointerSource(
        slider,
        source.getType() == juce::MouseInputSource::mouse
            ? juce::MouseInputSource::touch
            : juce::MouseInputSource::mouse,
        source.getIndex() + 1);
    slider.mouseDrag(makeMouseEvent(
        slider, dragPosition, primary, downPosition, true));
    slider.mouseUp(makeMouseEvent(
        slider, dragPosition, {}, downPosition, true));
    CHECK(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 0);
    CHECK(slider.getValue() == Catch::Approx(0.5));

    PrimarySliderTestAccess::setTrackedPointerSource(
        slider, source.getType(), source.getIndex());
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    CHECK(capture.dragStarts == 2);
    CHECK(capture.dragEnds == 1);
    CHECK(slider.hasActivePointerGesture());

    slider.mouseUp(makeMouseEvent(
        slider,
        downPosition,
        juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier }));
    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 2);
}

TEST_CASE("PrimarySlider closes a gesture when pointer release is lost",
          "[primary-slider][ui][input][gesture][lifecycle][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PrimarySlider slider;
    slider.setBounds(0, 0, 120, 120);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5, juce::dontSendNotification);
    SliderInteractionCapture capture;
    slider.addListener(&capture);

    const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto dragPosition = downPosition
                            + juce::Point<float> { 30.0f, -20.0f };
    const juce::ModifierKeys primary {
        juce::ModifierKeys::leftButtonModifier
    };
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    slider.mouseDrag(makeMouseEvent(
        slider, dragPosition, primary, downPosition, true));
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragStarts == 1);
    REQUIRE(capture.dragEnds == 0);

    slider.mouseMove(makeMouseEvent(slider, dragPosition, {}, downPosition, true));

    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);
    const auto recoveredValue = slider.getValue();

    slider.mouseUp(makeMouseEvent(
        slider, dragPosition, {}, downPosition, true));
    CHECK(capture.dragEnds == 1);
    CHECK(slider.getValue() == Catch::Approx(recoveredValue));
}

TEST_CASE("PrimarySlider closes a gesture when its desktop peer is detached",
          "[primary-slider][ui][input][gesture][lifecycle][peer][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component desktopHost;
    PrimarySlider slider;
    desktopHost.setBounds(0, 0, 160, 160);
    desktopHost.addAndMakeVisible(slider);
    slider.setBounds(0, 0, 120, 120);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5, juce::dontSendNotification);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(slider.isShowing());

    SliderInteractionCapture capture;
    slider.addListener(&capture);

    const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
    const auto dragPosition = downPosition
                            + juce::Point<float> { 30.0f, -20.0f };
    const juce::ModifierKeys primary {
        juce::ModifierKeys::leftButtonModifier
    };
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    slider.mouseDrag(makeMouseEvent(
        slider, dragPosition, primary, downPosition, true));
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragStarts == 1);
    REQUIRE(capture.dragEnds == 0);

    // Keep the gesture held beyond the visual press transition. Peer-loss
    // recovery must not depend on an animation that happens to still be moving.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragEnds == 0);

    desktopHost.removeFromDesktop();

    CHECK_FALSE(slider.isShowing());
    // A late native focus notification must not stop peer-loss recovery before
    // its timer can balance the drag. Exercise that ordering explicitly.
    slider.focusLost(juce::Component::FocusChangeType::focusChangedDirectly);
    if (slider.hasActivePointerGesture())
        CHECK(PrimarySliderTestAccess::isRecoveryTimerRunning(slider));
    const auto recoveryStarted = juce::Time::getMillisecondCounter();
    while (slider.hasActivePointerGesture()
           && juce::Time::getMillisecondCounter() - recoveryStarted < 1000)
        if (! juce::MessageManager::getInstance()->runDispatchLoopUntil(5))
            break;
    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);
    const auto detachedValue = slider.getValue();

    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(slider.isShowing());
    slider.mouseUp(makeMouseEvent(
        slider, dragPosition, {}, downPosition, true));

    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);
    CHECK(slider.getValue() == Catch::Approx(detachedValue));
    desktopHost.removeFromDesktop();
}

TEST_CASE("PrimarySlider closes a gesture when removed from its parent",
          "[primary-slider][ui][input][gesture][lifecycle][hierarchy][stale]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component desktopHost;
    PrimarySlider slider;
    desktopHost.setBounds(0, 0, 160, 160);
    desktopHost.addAndMakeVisible(slider);
    slider.setBounds(0, 0, 120, 120);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5, juce::dontSendNotification);
    desktopHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    desktopHost.setVisible(true);
    REQUIRE(slider.isShowing());

    SliderInteractionCapture capture;
    slider.addListener(&capture);

    const auto downPosition = slider.getLocalBounds().toFloat().getCentre();
    const juce::ModifierKeys primary {
        juce::ModifierKeys::leftButtonModifier
    };
    slider.mouseDown(makeMouseEvent(slider, downPosition, primary));
    REQUIRE(slider.hasActivePointerGesture());
    REQUIRE(capture.dragStarts == 1);

    desktopHost.removeChildComponent(&slider);

    CHECK_FALSE(slider.isShowing());
    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);
    const auto detachedValue = slider.getValue();

    desktopHost.addAndMakeVisible(slider);
    REQUIRE(slider.isShowing());
    slider.mouseUp(makeMouseEvent(slider, downPosition));

    CHECK_FALSE(slider.hasActivePointerGesture());
    CHECK(capture.dragEnds == 1);
    CHECK(slider.getValue() == Catch::Approx(detachedValue));
    desktopHost.removeFromDesktop();
}

TEST_CASE("Fire IncDec slider arrows accept only primary clicks",
          "[primary-slider][ui][input][incdec]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FireLookAndFeel lookAndFeel;
    PrimarySlider slider;
    slider.setLookAndFeel(&lookAndFeel);
    slider.setSliderStyle(juce::Slider::IncDecButtons);
    slider.setRange(2.0, 16.0, 1.0);
    slider.setValue(5.0, juce::dontSendNotification);
    slider.setBounds(0, 0, 160, 30);
    slider.resized();

    auto* increment = findSliderButton(slider, "slider_up_arrow");
    auto* decrement = findSliderButton(slider, "slider_down_arrow");
    REQUIRE(increment != nullptr);
    REQUIRE(decrement != nullptr);
    REQUIRE(dynamic_cast<PrimaryTextButton*>(increment) != nullptr);
    REQUIRE(dynamic_cast<PrimaryTextButton*>(decrement) != nullptr);
    const auto position = increment->getLocalBounds().toFloat().getCentre();
    const auto decrementPosition = decrement->getLocalBounds().toFloat().getCentre();

    for (const auto modifiers : rejectedPointerModifiers())
    {
        CAPTURE(modifiers.getRawFlags());
        static_cast<juce::Component&>(*increment).mouseDown(
            makeMouseEvent(*increment, position, modifiers));
        static_cast<juce::Component&>(*increment).mouseUp(
            makeMouseEvent(*increment, position));
        static_cast<juce::Component&>(*decrement).mouseDown(
            makeMouseEvent(*decrement, decrementPosition, modifiers));
        static_cast<juce::Component&>(*decrement).mouseUp(
            makeMouseEvent(*decrement, decrementPosition));
        CHECK(slider.getValue() == Catch::Approx(5.0));
    }

    static_cast<juce::Component&>(*increment).mouseDown(makeMouseEvent(
        *increment,
        position,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    static_cast<juce::Component&>(*increment).mouseUp(
        makeMouseEvent(*increment, position));
    CHECK(slider.getValue() == Catch::Approx(6.0));

    static_cast<juce::Component&>(*decrement).mouseDown(makeMouseEvent(
        *decrement,
        decrementPosition,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    static_cast<juce::Component&>(*decrement).mouseUp(
        makeMouseEvent(*decrement, decrementPosition));
    CHECK(slider.getValue() == Catch::Approx(5.0));

    slider.setLookAndFeel(nullptr);
}
