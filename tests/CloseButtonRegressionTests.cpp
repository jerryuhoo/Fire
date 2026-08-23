#include <Panels/SpectrogramPanel/CloseButton.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

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

    int clickCount = 0;
    button.onClick = [&clickCount] { ++clickCount; };
    auto& component = static_cast<juce::Component&>(button);

    const auto rightClick = juce::ModifierKeys {
        juce::ModifierKeys::rightButtonModifier };
    component.mouseDown(makeMouseEvent(button, rightClick));
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
