#include <Panels/SpectrogramPanel/DraggableButton.h>
#include <Panels/SpectrogramPanel/VerticalLine.h>
#include <Utility/AudioHelpers.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>

namespace
{
class ShowingDesktopHost final : public juce::Component
{
public:
    ShowingDesktopHost(int width, int height)
    {
        setBounds(0, 0, width, height);
        addToDesktop(juce::ComponentPeer::windowIsTemporary);
        setVisible(true);
    }

    ~ShowingDesktopHost() override
    {
        removeFromDesktop();
    }
};

juce::KeyPress shiftedKey(int keyCode)
{
    return { keyCode,
             juce::ModifierKeys { juce::ModifierKeys::shiftModifier },
             0 };
}

juce::MouseEvent makeDividerMouseEvent(
    juce::Component& component,
    juce::ModifierKeys modifiers,
    juce::Point<float> position = {})
{
    if (position == juce::Point<float> {})
        position = component.getLocalBounds().toFloat().getCentre();

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

void settleDividerAnimation(VerticalLine& divider)
{
    for (int frame = 0; frame < 180; ++frame)
        divider.advanceAnimation(1.0f / 60.0f);
}

juce::Image renderDivider(VerticalLine& divider)
{
    juce::Image image(juce::Image::ARGB,
                      juce::jmax(1, divider.getWidth()),
                      juce::jmax(1, divider.getHeight()),
                      true);
    juce::Graphics graphics(image);
    divider.paintEntireComponent(graphics, true);
    return image;
}

std::uint64_t imageFingerprint(const juce::Image& image)
{
    std::uint64_t fingerprint = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            fingerprint ^= image.getPixelAt(x, y).getARGB();
            fingerprint *= 1099511628211ull;
        }

    return fingerprint;
}

int dividerOuterEdgeAlpha(const juce::Image& image)
{
    int total = 0;
    for (int y = 16; y < image.getHeight() - 16; ++y)
        for (const auto x : { 0, 1,
                              image.getWidth() - 2,
                              image.getWidth() - 1 })
            total += image.getPixelAt(x, y).getAlpha();

    return total;
}

int imageAlphaIn(const juce::Image& image, juce::Rectangle<int> area)
{
    int total = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
            total += image.getPixelAt(x, y).getAlpha();

    return total;
}
} // namespace

TEST_CASE("Filter graph nodes support guarded keyboard and accessible frequency input",
          "[filter-control][ui][input][keyboard][accessibility][lifecycle]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(1000, 400);
    DraggableButton button;
    host.addAndMakeVisible(button);
    button.setBounds(495, 195, 10, 10);

    int dragCount = 0;
    int finishCount = 0;
    int qCount = 0;
    float lastQDelta = 0.0f;
    juce::Point<float> lastTargetInHost;
    button.onDrag = [&](DraggableButton& source,
                        const juce::MouseEvent& event)
    {
        ++dragCount;
        lastTargetInHost = event.getEventRelativeTo(&host).position;
        source.setBounds(juce::Rectangle<float>(10.0f, 10.0f)
                             .withCentre(lastTargetInHost)
                             .toNearestInt());
    };
    button.onDragFinished = [&] { ++finishCount; };
    button.onQValueChanged = [&](float delta)
    {
        ++qCount;
        lastQDelta = delta;
    };

    REQUIRE(button.getWantsKeyboardFocus());
    REQUIRE(button.getMouseClickGrabsKeyboardFocus());
    button.grabKeyboardFocus();
    CHECK(button.hasKeyboardFocus(true));

    CHECK(button.keyPressed(juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(lastTargetInHost.x == Catch::Approx(504.0f));
    CHECK(lastTargetInHost.y == Catch::Approx(200.0f));
    CHECK(dragCount == 1);
    CHECK(finishCount == 1);

    CHECK(button.keyPressed(shiftedKey(juce::KeyPress::upKey)));
    CHECK(lastTargetInHost.y == Catch::Approx(199.0f));
    CHECK(dragCount == 2);
    CHECK(finishCount == 2);

    CHECK(button.keyPressed(juce::KeyPress { juce::KeyPress::pageUpKey }));
    CHECK(qCount == 1);
    CHECK(lastQDelta == Catch::Approx(0.04f));

    auto* accessibility = button.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    CHECK(accessibility->getRole() == juce::AccessibilityRole::slider);
    CHECK(accessibility->getTitle() == "Filter node frequency");
    CHECK(accessibility->getHelp().containsIgnoreCase("arrow"));
    auto* value = accessibility->getValueInterface();
    REQUIRE(value != nullptr);
    CHECK_FALSE(value->isReadOnly());
    const auto range = value->getRange();
    REQUIRE(range.isValid());
    CHECK(range.getMinimumValue() == 20.0);
    CHECK(range.getMaximumValue() == 20000.0);

    value->setValue(2000.0);
    const auto expectedX = 1000.0
                         * juce::mapFromLog10(2000.0, 20.0, 20000.0);
    CHECK(lastTargetInHost.x == Catch::Approx(expectedX).margin(0.001));
    CHECK(dragCount == 3);
    CHECK(finishCount == 3);
    CHECK(value->getCurrentValue()
          == Catch::Approx(2000.0).epsilon(0.015));
    CHECK(value->getCurrentValueAsString().contains("Hz"));

    value->setValueAsString("3.5 kHz");
    const auto expectedKilohertzX =
        1000.0 * juce::mapFromLog10(3500.0, 20.0, 20000.0);
    CHECK(lastTargetInHost.x
          == Catch::Approx(expectedKilohertzX).margin(0.001));
    CHECK(dragCount == 4);
    CHECK(finishCount == 4);

    value->setValueAsString("2k");
    CHECK(value->getCurrentValue()
          == Catch::Approx(2000.0).epsilon(0.015));
    CHECK(dragCount == 5);
    CHECK(finishCount == 5);

    const auto boundsBeforeInvalidText = button.getBounds();
    for (const auto& invalidText : {
             juce::String { "garbage" },
             juce::String { "2 kHz trailing" },
             juce::String { "NaN Hz" } })
        value->setValueAsString(invalidText);

    CHECK(button.getBounds() == boundsBeforeInvalidText);
    CHECK(dragCount == 5);
    CHECK(finishCount == 5);

    host.removeFromDesktop();
    REQUIRE_FALSE(button.isShowing());
    const auto detachedBounds = button.getBounds();
    CHECK(value->isReadOnly());
    value->setValue(8000.0);
    CHECK(button.getBounds() == detachedBounds);
    CHECK_FALSE(button.keyPressed(
        juce::KeyPress { juce::KeyPress::leftKey }));
    juce::MouseWheelDetails wheel;
    wheel.deltaY = 0.5f;
    const auto eventTime = juce::Time::getCurrentTime();
    const auto eventPosition = button.getLocalBounds().toFloat().getCentre();
    const juce::MouseEvent wheelEvent(
        juce::Desktop::getInstance().getMainMouseSource(),
        eventPosition, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &button, &button, eventTime, eventPosition, eventTime, 1, false);
    button.mouseWheelMove(wheelEvent, wheel);
    CHECK(dragCount == 5);
    CHECK(finishCount == 5);
    CHECK(qCount == 1);
}

TEST_CASE("Filter graph keyboard nudges tolerate synchronous control deletion",
          "[filter-control][ui][input][keyboard][lifecycle][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(600, 240);
    auto button = std::make_unique<DraggableButton>();
    host.addAndMakeVisible(*button);
    button->setBounds(295, 115, 10, 10);

    int dragCount = 0;
    int finishCount = 0;
    button->onDragFinished = [&] { ++finishCount; };
    button->onDrag = [&](DraggableButton&, const juce::MouseEvent&)
    {
        ++dragCount;
        button.reset();
    };

    auto* rawButton = button.get();
    CHECK(rawButton->keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(button == nullptr);
    CHECK(dragCount == 1);
    CHECK(finishCount == 0);
}

TEST_CASE("Crossover keyboard and accessibility edits use balanced topology gestures",
          "[multiband][divider][ui][input][keyboard][accessibility][gesture]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(120, 240);
    VerticalLine divider;
    host.addAndMakeVisible(divider);
    divider.setBounds(48, 0, 24, 220);
    divider.setRange(20.0, 20000.0, 1.0);
    divider.setValue(1000.0, juce::dontSendNotification);
    divider.setXPercent(transformToLog(1000.0));
    divider.setIndex(1);

    int beginCount = 0;
    int changeCount = 0;
    int endCount = 0;
    int gestureDepth = 0;
    divider.setParameterGestureCallbacks(
        [&]
        {
            ++beginCount;
            ++gestureDepth;
        },
        [&]
        {
            ++changeCount;
            CHECK(gestureDepth == 1);
            return std::make_shared<int>(0);
        },
        [&]
        {
            ++endCount;
            --gestureDepth;
        });

    REQUIRE(divider.getWantsKeyboardFocus());
    REQUIRE(divider.getMouseClickGrabsKeyboardFocus());
    divider.grabKeyboardFocus();
    CHECK(divider.hasKeyboardFocus(true));
    REQUIRE(divider.advanceAnimation(1.0f / 60.0f));
    CHECK(divider.getHoverAnimation() > 0.0f);

    const auto originalValue = divider.getValue();
    CHECK(divider.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(divider.getValue() > originalValue);
    CHECK(beginCount == 1);
    CHECK(changeCount == 1);
    CHECK(endCount == 1);
    CHECK(gestureDepth == 0);

    const auto coarseValue = divider.getValue();
    CHECK(divider.keyPressed(shiftedKey(juce::KeyPress::leftKey)));
    CHECK(divider.getValue() < coarseValue);
    CHECK(beginCount == 2);
    CHECK(changeCount == 2);
    CHECK(endCount == 2);
    CHECK(gestureDepth == 0);

    auto* accessibility = divider.getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    CHECK(accessibility->getRole() == juce::AccessibilityRole::slider);
    CHECK(accessibility->getTitle() == "Crossover 2 frequency");
    CHECK(accessibility->getHelp().containsIgnoreCase("Shift"));
    auto* value = accessibility->getValueInterface();
    REQUIRE(value != nullptr);
    CHECK_FALSE(value->isReadOnly());
    REQUIRE(value->getRange().isValid());
    CHECK(value->getRange().getMinimumValue() == 20.0);
    CHECK(value->getRange().getMaximumValue() == 20000.0);
    CHECK(value->getCurrentValueAsString().contains("Hz"));

    value->setValue(4000.0);
    CHECK(divider.getValue() == Catch::Approx(4000.0));
    CHECK(divider.getXPercent()
          == Catch::Approx(transformToLog(4000.0)).margin(0.0001));
    CHECK(beginCount == 3);
    CHECK(changeCount == 3);
    CHECK(endCount == 3);
    CHECK(gestureDepth == 0);

    value->setValueAsString("3.5 kHz");
    CHECK(divider.getValue() == Catch::Approx(3500.0));
    CHECK(divider.getXPercent()
          == Catch::Approx(transformToLog(3500.0)).margin(0.0001));
    CHECK(beginCount == 4);
    CHECK(changeCount == 4);
    CHECK(endCount == 4);
    CHECK(gestureDepth == 0);

    const auto valueBeforeInvalidText = divider.getValue();
    const auto xBeforeInvalidText = divider.getXPercent();
    for (const auto& invalidText : {
             juce::String { "garbage" },
             juce::String { "1kHzjunk" },
             juce::String { "2 kHz trailing" },
             juce::String { "NaN Hz" } })
        value->setValueAsString(invalidText);

    CHECK(divider.getValue() == Catch::Approx(valueBeforeInvalidText));
    CHECK(divider.getXPercent()
          == Catch::Approx(xBeforeInvalidText).margin(0.0001));
    CHECK(beginCount == 4);
    CHECK(changeCount == 4);
    CHECK(endCount == 4);
    CHECK(gestureDepth == 0);

    host.removeFromDesktop();
    REQUIRE_FALSE(divider.isShowing());
    CHECK(value->isReadOnly());
    const auto detachedValue = divider.getValue();
    value->setValue(8000.0);
    CHECK(divider.getValue() == Catch::Approx(detachedValue));
    CHECK_FALSE(divider.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(beginCount == 4);
    CHECK(changeCount == 4);
    CHECK(endCount == 4);
}

TEST_CASE("Crossover accessible edits remain balanced during synchronous deletion",
          "[multiband][divider][ui][input][accessibility][gesture][self-delete]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(120, 240);
    auto divider = std::make_unique<VerticalLine>();
    host.addAndMakeVisible(*divider);
    divider->setBounds(48, 0, 24, 220);
    divider->setRange(20.0, 20000.0, 1.0);
    divider->setValue(1000.0, juce::dontSendNotification);
    divider->setXPercent(transformToLog(1000.0));

    int beginCount = 0;
    int changeCount = 0;
    int endCount = 0;
    divider->setParameterGestureCallbacks(
        [&] { ++beginCount; },
        [&]
        {
            ++changeCount;
            divider.reset();
            return VerticalLine::ParameterGestureToken {};
        },
        [&] { ++endCount; });

    auto* accessibility = divider->getAccessibilityHandler();
    REQUIRE(accessibility != nullptr);
    auto* value = accessibility->getValueInterface();
    REQUIRE(value != nullptr);
    value->setValue(2000.0);

    CHECK(divider == nullptr);
    CHECK(beginCount == 1);
    CHECK(changeCount == 1);
    CHECK(endCount == 1);
}

TEST_CASE("Crossover focus presentation follows keyboard and pointer modality",
          "[multiband][divider][ui][input][focus][animation][regression]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(120, 240);
    VerticalLine divider;
    host.addAndMakeVisible(divider);
    divider.setBounds(48, 0, 24, 220);
    divider.setRange(20.0, 20000.0, 1.0);
    divider.setValue(1000.0, juce::dontSendNotification);
    divider.setXPercent(transformToLog(1000.0));

    settleDividerAnimation(divider);
    const auto idleImage = renderDivider(divider);
    const auto idleFingerprint = imageFingerprint(idleImage);

    REQUIRE(divider.getMouseClickGrabsKeyboardFocus());
    divider.grabKeyboardFocus();
    REQUIRE(divider.hasKeyboardFocus(true));
    settleDividerAnimation(divider);
    const auto keyboardFocusImage = renderDivider(divider);
    const auto keyboardFocusFingerprint = imageFingerprint(keyboardFocusImage);

    // Keyboard/direct focus remains clearly visible around the line handle,
    // but never outlines the full 24-pixel pointer hit target.
    CHECK(keyboardFocusFingerprint != idleFingerprint);
    const auto handleArea = juce::Rectangle<int>(0, 0,
                                                  divider.getWidth(), 16);
    CHECK(imageAlphaIn(keyboardFocusImage, handleArea)
          > imageAlphaIn(idleImage, handleArea));
    CHECK(dividerOuterEdgeAlpha(keyboardFocusImage) == 0);

    // Rejected pointer gestures must not change a keyboard-origin focus cue.
    for (const auto modifiers : {
             juce::ModifierKeys { juce::ModifierKeys::rightButtonModifier },
             juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier
                                  | juce::ModifierKeys::rightButtonModifier } })
    {
        static_cast<juce::Component&>(divider).mouseDown(
            makeDividerMouseEvent(divider, modifiers));
        settleDividerAnimation(divider);
        CHECK(imageFingerprint(renderDivider(divider))
              == keyboardFocusFingerprint);
    }

    divider.setPointerGestureAdmissionCallback(
        [](juce::MouseInputSource::InputSourceType, int) { return false; });
    static_cast<juce::Component&>(divider).mouseDown(
        makeDividerMouseEvent(
            divider,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    settleDividerAnimation(divider);
    CHECK(imageFingerprint(renderDivider(divider))
          == keyboardFocusFingerprint);
    divider.setPointerGestureAdmissionCallback({});

    // An accepted drag keeps actual focus for immediate arrow-key use, while
    // its pointer presentation settles completely back to the idle drawing.
    static_cast<juce::Component&>(divider).mouseDown(
        makeDividerMouseEvent(
            divider,
            juce::ModifierKeys { juce::ModifierKeys::leftButtonModifier }));
    CHECK(divider.hasKeyboardFocus(true));
    CHECK(imageFingerprint(renderDivider(divider))
          != keyboardFocusFingerprint);
    static_cast<juce::Component&>(divider).mouseUp(
        makeDividerMouseEvent(divider, {}));
    static_cast<juce::Component&>(divider).mouseExit(
        makeDividerMouseEvent(divider, {}, { -1.0f, -1.0f }));
    settleDividerAnimation(divider);

    CHECK(divider.hasKeyboardFocus(true));
    CHECK(divider.getHoverAnimation()
          == Catch::Approx(0.0f).margin(0.001f));
    CHECK(divider.getPressAnimation()
          == Catch::Approx(0.0f).margin(0.001f));
    CHECK(imageFingerprint(renderDivider(divider)) == idleFingerprint);

    CHECK(divider.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    settleDividerAnimation(divider);
    CHECK(imageFingerprint(renderDivider(divider))
          == keyboardFocusFingerprint);

    divider.setEnabled(false);
    CHECK(divider.getHoverAnimation() == 0.0f);
    CHECK(divider.getPressAnimation() == 0.0f);

    divider.setEnabled(true);
    divider.giveAwayKeyboardFocus();
    divider.grabKeyboardFocus();
    settleDividerAnimation(divider);
    REQUIRE(divider.getHoverAnimation() > 0.0f);

    divider.setVisible(false);
    CHECK(divider.getHoverAnimation() == 0.0f);
    CHECK(divider.getPressAnimation() == 0.0f);

    divider.setVisible(true);
    divider.giveAwayKeyboardFocus();
    divider.grabKeyboardFocus();
    settleDividerAnimation(divider);
    REQUIRE(divider.getHoverAnimation() > 0.0f);
    host.removeChildComponent(&divider);
    CHECK_FALSE(divider.isShowing());
    CHECK(divider.getHoverAnimation() == 0.0f);
    CHECK(divider.getPressAnimation() == 0.0f);
}

TEST_CASE("Rejected crossover keyboard publication restores its visual position",
          "[multiband][divider][ui][input][keyboard][gesture][rejected]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ShowingDesktopHost host(120, 240);
    VerticalLine divider;
    host.addAndMakeVisible(divider);
    divider.setBounds(48, 0, 24, 220);
    divider.setRange(20.0, 20000.0, 1.0);
    divider.setValue(1000.0, juce::dontSendNotification);
    divider.setXPercent(transformToLog(1000.0));

    int beginCount = 0;
    int changeCount = 0;
    int endCount = 0;
    divider.setParameterGestureCallbacks(
        [&] { ++beginCount; },
        [&]
        {
            ++changeCount;
            return VerticalLine::ParameterGestureToken {};
        },
        [&] { ++endCount; });

    const auto originalValue = divider.getValue();
    const auto originalX = divider.getXPercent();
    CHECK(divider.keyPressed(
        juce::KeyPress { juce::KeyPress::rightKey }));
    CHECK(divider.getValue() == Catch::Approx(originalValue));
    CHECK(divider.getXPercent() == Catch::Approx(originalX).margin(0.0001));
    CHECK(beginCount == 1);
    CHECK(changeCount == 1);
    CHECK(endCount == 1);
}
