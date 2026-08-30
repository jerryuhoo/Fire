#include <Panels/SpectrogramPanel/CloseButton.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

struct CloseButtonPointerTestAccess
{
    static bool hasPrimaryPointer(const CloseButton& button)
    {
        return button.primaryPointerDown;
    }

    static void setTrackedPointerSource(
        CloseButton& button,
        juce::MouseInputSource::InputSourceType sourceType,
        int sourceIndex)
    {
        button.pointerSourceType = sourceType;
        button.pointerSourceIndex = sourceIndex;
    }
};

namespace
{
class TestableCloseButton final : public CloseButton
{
public:
    std::unique_ptr<juce::AccessibilityHandler> createHandlerForTest()
    {
        return juce::Button::createAccessibilityHandler();
    }
};

void settleAnimation(CloseButton& button)
{
    for (int frame = 0; frame < 120; ++frame)
        button.advanceAnimation(1.0f / 60.0f);
}

std::uint64_t renderFingerprint(CloseButton& button)
{
    juce::Image image(juce::Image::ARGB,
                      juce::jmax(1, button.getWidth()),
                      juce::jmax(1, button.getHeight()),
                      true);
    juce::Graphics graphics(image);
    button.paintEntireComponent(graphics, true);

    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}

juce::MouseEvent makeMouseEvent(juce::Component& component,
                                juce::ModifierKeys modifiers)
{
    const auto position = component.getLocalBounds().toFloat().getCentre();
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
} // namespace

TEST_CASE("Band delete control has an accessible full-size hit target",
          "[close-button][multiband][ui][accessibility]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestableCloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);

    CHECK(CloseButton::minimumHitTargetSize >= 24);
    CHECK(button.getTitle() == "Delete band");
    CHECK(button.getDescription() == "Remove this frequency band");
    CHECK(button.getHelpText().isNotEmpty());
    CHECK(button.getTooltip() == "Delete band");
    CHECK(button.isAccessible());
    CHECK(button.getWantsKeyboardFocus());
    auto accessibilityHandler = button.createHandlerForTest();
    REQUIRE(accessibilityHandler != nullptr);
    CHECK(accessibilityHandler->getRole() == juce::AccessibilityRole::button);
    CHECK_FALSE(button.isPresented());
    CHECK_FALSE(button.isVisible());

    button.setPresented(true, false);
    CHECK(button.isPresented());
    CHECK(button.isVisible());
    CHECK(button.hitTest(0, 0));
    CHECK(button.hitTest(button.getWidth() - 1, button.getHeight() - 1));

    bool interceptsButton = false;
    bool interceptsChildren = true;
    button.getInterceptsMouseClicks(interceptsButton, interceptsChildren);
    CHECK(interceptsButton);
    CHECK_FALSE(interceptsChildren);
}

TEST_CASE("Band delete feedback fades on the shared animation clock",
          "[close-button][multiband][ui][animation]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);

    button.setPresented(true);
    CHECK(button.isVisible());
    CHECK(button.getVisibilityAnimation() == 0.0f);
    REQUIRE(button.advanceAnimation(1.0f / 60.0f));
    CHECK(button.getVisibilityAnimation() > 0.0f);
    CHECK(button.getVisibilityAnimation() < 1.0f);

    settleAnimation(button);
    CHECK(button.getVisibilityAnimation() == Catch::Approx(1.0f).margin(0.001f));

    button.setState(juce::Button::buttonOver);
    REQUIRE(button.advanceAnimation(1.0f / 60.0f));
    CHECK(button.getHoverAnimation() > 0.0f);
    CHECK(button.getHoverAnimation() < 1.0f);
    CHECK(button.getPressAnimation() == 0.0f);

    button.setState(juce::Button::buttonDown);
    REQUIRE(button.advanceAnimation(1.0f / 60.0f));
    CHECK(button.getPressAnimation() > 0.0f);

    button.setState(juce::Button::buttonNormal);
    const auto hoverBeforeRelease = button.getHoverAnimation();
    const auto pressBeforeRelease = button.getPressAnimation();
    for (int frame = 0; frame < 12; ++frame)
        button.advanceAnimation(1.0f / 60.0f);
    CHECK(button.getHoverAnimation() < hoverBeforeRelease);
    CHECK(button.getPressAnimation() < pressBeforeRelease);

    settleAnimation(button);
    CHECK(button.getHoverAnimation() == Catch::Approx(0.0f).margin(0.001f));
    CHECK(button.getPressAnimation() == Catch::Approx(0.0f).margin(0.001f));

    button.setPresented(false);
    CHECK_FALSE(button.isPresented());
    CHECK(button.isVisible());

    bool interceptsButton = true;
    bool interceptsChildren = true;
    button.getInterceptsMouseClicks(interceptsButton, interceptsChildren);
    CHECK_FALSE(interceptsButton);
    CHECK_FALSE(interceptsChildren);

    const auto visibilityBeforeFade = button.getVisibilityAnimation();
    REQUIRE(button.advanceAnimation(1.0f / 60.0f));
    CHECK(button.getVisibilityAnimation() < visibilityBeforeFade);
    CHECK(button.isVisible());

    settleAnimation(button);
    CHECK(button.getVisibilityAnimation() == Catch::Approx(0.0f).margin(0.001f));
    CHECK_FALSE(button.isVisible());
}

TEST_CASE("Band delete tile scales and animates without changing its hit geometry",
          "[close-button][multiband][ui][render]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (const auto size : { CloseButton::minimumHitTargetSize, 36 })
    {
        CloseButton button;
        button.setBounds(0, 0, size, size);
        button.setPresented(true, false);

        const auto idleFingerprint = renderFingerprint(button);

        button.setState(juce::Button::buttonOver);
        settleAnimation(button);
        const auto hoverFingerprint = renderFingerprint(button);

        button.setState(juce::Button::buttonDown);
        settleAnimation(button);
        const auto pressFingerprint = renderFingerprint(button);

        CAPTURE(size, idleFingerprint, hoverFingerprint, pressFingerprint);
        CHECK(idleFingerprint != hoverFingerprint);
        CHECK(hoverFingerprint != pressFingerprint);
        CHECK(button.hitTest(0, 0));
        CHECK(button.hitTest(size - 1, size - 1));
    }
}

TEST_CASE("Band delete focus feedback follows keyboard modality",
          "[close-button][multiband][ui][animation][focus][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Component host;
    juce::Component focusSink;
    CloseButton button;
    host.setBounds(0, 0, 96, 48);
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);
    focusSink.setBounds(48, 0, 24, 24);
    focusSink.setWantsKeyboardFocus(true);
    host.addAndMakeVisible(button);
    host.addAndMakeVisible(focusSink);
    button.setPresented(true, false);
    host.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    host.setVisible(true);
    focusSink.grabKeyboardFocus();
    REQUIRE_FALSE(button.hasKeyboardFocus(true));
    settleAnimation(button);
    const auto restingFingerprint = renderFingerprint(button);

    button.grabKeyboardFocus();
    REQUIRE(button.hasKeyboardFocus(true));
    settleAnimation(button);
    CHECK(button.getFocusAnimation() == Catch::Approx(1.0f).margin(0.001f));
    const auto keyboardFingerprint = renderFingerprint(button);
    CHECK(keyboardFingerprint != restingFingerprint);

    auto& component = static_cast<juce::Component&>(button);
    component.mouseDown(makeMouseEvent(
        button,
        juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    component.mouseUp(makeMouseEvent(button, {}));
    component.mouseExit(makeMouseEvent(button, {}));
    settleAnimation(button);
    CHECK(button.getFocusAnimation() == Catch::Approx(0.0f).margin(0.001f));
    CHECK(renderFingerprint(button) == restingFingerprint);

    REQUIRE_FALSE(component.keyPressed(juce::KeyPress { 'x' }));
    settleAnimation(button);
    CHECK(button.getFocusAnimation() == Catch::Approx(1.0f).margin(0.001f));
    CHECK(renderFingerprint(button) == keyboardFingerprint);
}

TEST_CASE("Band deletion requires a primary click",
          "[close-button][multiband][ui][input]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);
    button.setPresented(true, false);
    button.addToDesktop(juce::ComponentPeer::windowIsTemporary);

    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };
    auto& component = static_cast<juce::Component&>(button);

    const auto rightClick = juce::ModifierKeys {
        juce::ModifierKeys::rightButtonModifier };
    component.mouseDown(makeMouseEvent(button, rightClick));
    component.mouseUp(makeMouseEvent(button, {}));
    CHECK(clickCount == 0);

    const auto middleClick = juce::ModifierKeys {
        juce::ModifierKeys::middleButtonModifier };
    component.mouseDown(makeMouseEvent(button, middleClick));
    component.mouseUp(makeMouseEvent(button, {}));
    CHECK(clickCount == 0);

    const auto primaryAndMiddle = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::middleButtonModifier };
    component.mouseDown(makeMouseEvent(button, primaryAndMiddle));
    component.mouseUp(makeMouseEvent(button, {}));
    CHECK(clickCount == 0);

#if JUCE_MAC
    const auto controlClick = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
        | juce::ModifierKeys::ctrlModifier };
    component.mouseDown(makeMouseEvent(button, controlClick));
    component.mouseUp(makeMouseEvent(button, juce::ModifierKeys::ctrlModifier));
    CHECK(clickCount == 0);
#endif

    const auto leftClick = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier };
    component.mouseDown(makeMouseEvent(button, leftClick));
    component.mouseUp(makeMouseEvent(button, {}));
    CHECK(clickCount == 1);
}

TEST_CASE("Band deletion activation stays inside the presented session",
          "[close-button][multiband][ui][input][keyboard][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);
    button.setPresented(true, false);
    button.addToDesktop(juce::ComponentPeer::windowIsTemporary);

    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };

    SECTION("programmatic and keyboard activation are synchronous")
    {
        button.triggerClick();
        CHECK(clickCount == 1);

        REQUIRE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK(clickCount == 2);

        REQUIRE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        CHECK(clickCount == 3);

        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(clickCount == 3);
    }

    SECTION("withdrawn presentation rejects stale commands")
    {
        button.setPresented(false, false);
        CHECK_FALSE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        CHECK_FALSE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::spaceKey }));
        button.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(clickCount == 0);
    }

    SECTION("hidden parent rejects accessibility activation")
    {
        juce::Component hiddenParent;
        hiddenParent.addAndMakeVisible(button);
        hiddenParent.setVisible(false);
        button.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        CHECK(clickCount == 0);
    }

    SECTION("cached accessibility rejects peer detachment")
    {
        auto* accessibility = button.getAccessibilityHandler();
        REQUIRE(accessibility != nullptr);
        CHECK(accessibility->getRole() == juce::AccessibilityRole::button);
        REQUIRE(accessibility->getActions().contains(
            juce::AccessibilityActionType::press));

        button.removeFromDesktop();
        REQUIRE(button.isVisible());
        REQUIRE_FALSE(button.isShowing());

        CHECK_FALSE(button.keyPressed(
            juce::KeyPress { juce::KeyPress::returnKey }));
        REQUIRE(accessibility->getActions().invoke(
            juce::AccessibilityActionType::press));
        CHECK(clickCount == 0);
    }
}

TEST_CASE("Band deletion callbacks may synchronously delete their control",
          "[close-button][multiband][ui][input][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto makeButton = []
    {
        auto button = std::make_unique<CloseButton>();
        button->setBounds(0,
                          0,
                          CloseButton::minimumHitTargetSize,
                          CloseButton::minimumHitTargetSize);
        button->setPresented(true, false);
        button->addToDesktop(juce::ComponentPeer::windowIsTemporary);
        return button;
    };

    SECTION("activation callback")
    {
        auto button = makeButton();
        auto* rawButton = button.get();
        rawButton->onClick = [&button] { button.reset(); };

        rawButton->triggerClick();
        CHECK(button == nullptr);
    }

    SECTION("hover state callback")
    {
        auto button = makeButton();
        auto* rawButton = button.get();
        auto& component = static_cast<juce::Component&>(*rawButton);
        component.mouseDown(makeMouseEvent(
            *rawButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(rawButton->isDown());
        rawButton->onStateChange = [&button] { button.reset(); };

        component.mouseExit(makeMouseEvent(*rawButton, {}));
        CHECK(button == nullptr);
    }

    SECTION("presentation withdrawal callback")
    {
        auto button = makeButton();
        auto* rawButton = button.get();
        auto& component = static_cast<juce::Component&>(*rawButton);
        component.mouseDown(makeMouseEvent(
            *rawButton,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
        REQUIRE(rawButton->isDown());
        rawButton->onStateChange = [&button] { button.reset(); };

        rawButton->setPresented(false, false);
        CHECK(button == nullptr);
    }
}

TEST_CASE("Band deletion remains owned by one pointer source",
          "[close-button][multiband][ui][input][source][multitouch]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);
    button.setPresented(true, false);

    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };
    auto& component = static_cast<juce::Component&>(button);
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };
    component.mouseDown(makeMouseEvent(button, primary));
    REQUIRE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));

    const auto mainSource = juce::Desktop::getInstance().getMainMouseSource();
    CloseButtonPointerTestAccess::setTrackedPointerSource(
        button,
        juce::MouseInputSource::touch,
        mainSource.getIndex() + 19);

    SECTION("foreign release is ignored")
    {
        component.mouseUp(makeMouseEvent(button, {}));
    }

    SECTION("foreign popup down cannot cancel the owner")
    {
        component.mouseDown(makeMouseEvent(
            button,
            juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier }));
    }

    SECTION("foreign hover events cannot cancel the owner")
    {
        component.mouseExit(makeMouseEvent(button, {}));
        CHECK(button.isDown());
        component.mouseMove(makeMouseEvent(button, {}));
        CHECK(button.isDown());
        component.mouseEnter(makeMouseEvent(button, {}));
        CHECK(button.isDown());
    }

    CHECK(clickCount == 0);
    CHECK(CloseButtonPointerTestAccess::hasPrimaryPointer(button));

    CloseButtonPointerTestAccess::setTrackedPointerSource(
        button, mainSource.getType(), mainSource.getIndex());
    component.mouseUp(makeMouseEvent(button, {}));

    CHECK(clickCount == 1);
    CHECK_FALSE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));
}

TEST_CASE("Band deletion recovers when its primary release is lost",
          "[close-button][multiband][ui][input][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CloseButton button;
    button.setBounds(0,
                     0,
                     CloseButton::minimumHitTargetSize,
                     CloseButton::minimumHitTargetSize);
    button.setPresented(true, false);

    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };
    auto& component = static_cast<juce::Component&>(button);
    const auto primary = juce::ModifierKeys {
        juce::ModifierKeys::leftButtonModifier
    };

    SECTION("owner movement without the primary button")
    {
        component.mouseDown(makeMouseEvent(button, primary));
        REQUIRE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));

        component.mouseMove(makeMouseEvent(button, {}));
        CHECK_FALSE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));
        CHECK_FALSE(button.isDown());

        component.mouseUp(makeMouseEvent(button, {}));
    }

    SECTION("presentation is withdrawn while pressed")
    {
        component.mouseDown(makeMouseEvent(button, primary));
        REQUIRE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));

        button.setPresented(false);
        CHECK_FALSE(CloseButtonPointerTestAccess::hasPrimaryPointer(button));
        CHECK_FALSE(button.isDown());

        component.mouseUp(makeMouseEvent(button, {}));
    }

    CHECK(clickCount == 0);
}
